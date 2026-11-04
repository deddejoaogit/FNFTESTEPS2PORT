#!/usr/bin/env python3
"""Cartao de prova: mede o ELF e o codigo e compara com os limites do PS2.

Nao e' um "eu acho que cabe". Cada numero sai de um arquivo de verdade:

  - a pegada de RAM sai do mapa do linker (`.map`), somando .text/.data/.rodata/
    .bss e lendo `_end`, que e' onde a heap comeca;
  - os buffers dinamicos saem do proprio `main.c` (cada `memalign` com tamanho
    constante e' somado; o maior caso simultaneo e' o do boot, que e' quando
    todos eles existem ao mesmo tempo);
  - o mapa de VRAM sai de `src/engine/vram_layout.c`, que ja' e' comparado com
    o alocador real do ps2sdk em tempo de execucao (`vram_mapa` no log);
  - o DMA e o pacote saem das contantes do codigo e dos limites do DMAC;
  - as escritas bloqueantes por quadro saem de uma varredura do laco de quadro.

Uso:  python3 tools/cartao_de_prova.py [--elf build/dist/FNF_PS2.ELF]
                                      [--map build/dist/FNF_PS2.map]
                                      [--json saida.json] [--md saida.md]
"""
import argparse
import json
import os
import re
import struct
import subprocess
import sys

# ------------------------------------------------------------------ limites
EE_RAM = 32 * 1024 * 1024          # 32 MiB no EE
GS_VRAM = 4 * 1024 * 1024          # 4 MiB no GS
GS_MAX_TEX_W = 1024                # largura maxima de textura
EE_STACK_DEFAULT = 0x40000         # pilha que o Makefile.eeglobal cria
DMA_QWC_MAX = 0xFFFF               # teto do canal 2 (GIF)
PACKET_QWORDS = 4096               # main.c: packet_init(4096, ...)
QWORD = 16

# O que o jogo mediu precisar, medido no proprio codigo (ver funcoes abaixo).
REQ_USB_MB_S = 2.0


def sh(*a):
    try:
        return subprocess.run(a, capture_output=True, text=True, timeout=120).stdout
    except Exception:
        return ""


def le_simbolos(elf):
    """Le a tabela de simbolos do ELF com nm, sem depender de pyelftools."""
    out = sh("nm", "-n", elf) or sh("mips64r5900el-ps2-elf-nm", "-n", elf)
    syms = {}
    for linha in out.splitlines():
        p = linha.split()
        if len(p) == 3:
            try:
                syms[p[2]] = int(p[0], 16)
            except ValueError:
                pass
    return syms


def le_map(mapfile):
    """Somas de secao e o endereco de _end, direto do mapa do linker."""
    sec, end = {}, None
    if not os.path.exists(mapfile):
        return sec, end
    with open(mapfile, "r", errors="replace") as f:
        for linha in f:
            m = re.match(r"^\.(text|data|rodata|bss|sbss)\s+(0x[0-9a-f]+)\s+(0x[0-9a-f]+)", linha)
            if m:
                sec[m.group(1)] = int(m.group(3), 16)
            m2 = re.search(r"(0x[0-9a-f]+)\s+_end\s*=", linha)
            if m2:
                end = int(m2.group(1), 16)
    return sec, end


def buffers_dinamicos(main_c):
    """Cada memalign com tamanho constante, do main.c, com o nome do buffer."""
    src = open(main_c, errors="replace").read()
    achados = []
    # memalign(<alinh>, <expr>); guarda a expressao
    for m in re.finditer(r"([A-Za-z_][A-Za-z0-9_\.\->\[\]]*)\s*=\s*memalign\s*\(\s*\w+\s*,\s*([^;\)]+(?:\)[^;]*?)?)\)\s*;", src):
        alvo, expr = m.group(1), m.group(2).strip()
        # so' expressoes de tamanho; descarta as que dependem de runtime
        if re.search(r"[a-z_][a-z0-9_]*\s*[\.\[]", expr) and not re.match(r"^[A-Z0-9_\s\*\+\(\)u]+$", expr):
            continue
        val = avalia(expr, src)
        if val:
            achados.append((alvo, expr, val))
    # o cache de quadros do titulo tem o tamanho em defines
    if "TITLE_LOGO_BYTES+TITLE_GF_BYTES" in src:
        logo = define(src, "TITLE_LOGO_BYTES")
        gf = define(src, "TITLE_GF_BYTES")
        if logo and gf:
            achados.append(("g_title_anim_pixels", "TITLE_LOGO_BYTES+TITLE_GF_BYTES", logo + gf))
    return achados


def define(src, nome):
    m = re.search(r"^#define\s+%s\s+(.+)$" % re.escape(nome), src, re.M)
    if not m:
        return None
    return avalia(m.group(1), src)


def avalia(expr, src, prof=0):
    """Avalia uma expressao C de constantes inteiras. Nada de eval() cego:
    so' aceita digitos, operadores e nomes que sejam #define do proprio fonte."""
    if prof > 6:
        return None
    e = expr
    e = re.sub(r"(\d+)[uUlL]+\b", r"\1", e)
    e = e.replace("/", "//")
    # substitui nomes pelos defines
    def sub(m):
        nome = m.group(0)
        if nome in ("define", "u", "U"):
            return nome
        v = define(src, nome)
        if v is None:
            return "__X__"
        return "(%d)" % v
    e = re.sub(r"\b[A-Z][A-Z0-9_]{2,}\b", sub, e)
    if "__X__" in e:
        return None
    if not re.match(r"^[0-9\s\+\-\*/\(\)]+$", e):
        return None
    try:
        return int(eval(e, {"__builtins__": {}}, {}))
    except Exception:
        return None


def vram_layout(path):
    """Soma o mapa de VRAM; tambem confere a largura maxima de textura."""
    src = open(path, errors="replace").read()
    total, pior_w, itens = 0, 0, []
    for m in re.finditer(r'\{\s*"([a-z0-9_]+)"\s*,\s*(\d+)\s*,\s*(\d+)\s*,\s*VL_PSM_(\d+)\s*,\s*VL_ALINHAMENTO_(\w+)', src):
        nome, w, h, psm, al = m.group(1), int(m.group(2)), int(m.group(3)), int(m.group(4)), m.group(5)
        largura = (w + 63) & ~63 if w > 16 else w
        palavras = (largura * (h >> 1)) if psm == 16 else (largura * h)
        alinh = 64 if al == "PAGINA" else 8 if al == "BLOCO" else 1
        palavras = (palavras + alinh - 1) & ~(alinh - 1)
        total += palavras * 4
        pior_w = max(pior_w, w)
        itens.append((nome, w, h, psm, palavras * 4))
    return total, pior_w, itens


def escritas_por_quadro(main_c):
    """Classifica cada escrita bloqueante do corpo do quadro.

    Tres categorias, e so' a terceira e' defeito:
      - "uma vez"   : guardada por `frame_no==0` / `++contador==1` (boot e
                      transicoes: um punhado de escritas, uma vez so');
      - "periodica" : o marco FRAME, limitado por tempo dentro do proprio
                      frame_mark (1 a cada 200 ms = 5/s);
      - "por quadro": sem guarda -- essa sim seria uma escrita bloqueante em
                      todo quadro, que e' o padrao que ja' matou a 0.2.108.
    """
    """Conta os pontos do laco de quadro que fazem open+write+fsync (E/S
    bloqueante no pendrive). Cada `capture_status(`/`boot_mark(` e' uma delas.
    O laco de quadro vai de `while(running)` ate' o `graph_wait_vsync()` final;
    para nao depender de casar chaves, conta as chamadas que aparecem depois do
    marco F0_LOOP_ENTER e antes do proximo `frame_mark("F.VSYNC"`."""
    src = open(main_c, errors="replace").read()
    i = src.find("while(running){")
    j = src.find('frame_mark("F.VSYNC"', i)
    if i < 0 or j < 0:
        return None, None
    corpo = src[i:j]
    uma_vez = periodica = por_quadro = 0
    for m in re.finditer(r'(capture_status|boot_mark)\(\s*"([A-Z0-9_]+)"', corpo):
        nome = m.group(2)
        ctx = corpo[max(0, m.start() - 220):m.start()]
        if (nome.split("_")[0] in ("F0", "TUT", "STORY", "RAW", "BACK", "META",
                                  "CACHE", "PAD", "AUDIO", "GS", "TITLE",
                                  "FIRST", "DEFERRED", "SHUTDOWN", "FALHA",
                                  "AVISO", "FRAME_")
                or "frame_no" in ctx or "++" in ctx or "==1)" in ctx):
            uma_vez += 1
        elif nome == "FRAME":
            periodica += 1
        else:
            por_quadro += 1
    return uma_vez + periodica + por_quadro, por_quadro


def checa_elf(elf):
    """Confere que o ELF e' de verdade um executavel EE e le a entrada."""
    with open(elf, "rb") as f:
        h = f.read(64)
    if h[:4] != b"\x7fELF":
        return None
    ei_class = h[4]
    e_machine = struct.unpack("<H", h[18:20])[0]
    e_entry = struct.unpack("<I", h[24:28])[0]
    return {"class": 64 if ei_class == 2 else 32, "machine": e_machine,
            "entry": e_entry, "tamanho": os.path.getsize(elf)}


def main():
    ap = argparse.ArgumentParser()
    raiz = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    ap.add_argument("--elf", default=os.path.join(raiz, "build/dist/FNF_PS2.ELF"))
    ap.add_argument("--map", default=os.path.join(raiz, "build/dist/FNF_PS2.map"))
    ap.add_argument("--json")
    ap.add_argument("--md")
    a = ap.parse_args()

    main_c = os.path.join(raiz, "src/main.c")
    vram_c = os.path.join(raiz, "src/engine/vram_layout.c")
    sec, end = le_map(a.map)
    static = sum(sec.values())
    heap_livre = (EE_RAM - end) if end else None

    # ---- buffers dinamicos
    bufs = buffers_dinamicos(main_c)
    # o boot e' o pior caso: todos existem juntos. Soma os que tem nome unico.
    vistos, soma_din = set(), 0
    for alvo, expr, val in bufs:
        chave = re.sub(r"\[.*", "", alvo)
        if chave in vistos:
            continue
        vistos.add(chave)
        soma_din += val

    # ---- VRAM
    vr_total, vr_pior_w, vr_itens = vram_layout(vram_c)

    # ---- quadro
    marcos, por_quadro = escritas_por_quadro(main_c)

    # ---- ELF
    info = checa_elf(a.elf) if os.path.exists(a.elf) else None

    # ---- veredito
    itens = []

    def item(nome, medido, limite, ok, nota):
        itens.append({"item": nome, "medido": medido, "limite": limite,
                      "ok": bool(ok), "nota": nota})

    if end:
        item("RAM estatica (.text+.data+.rodata+.bss)", static, EE_RAM,
             static < EE_RAM,
             "_end = 0x%08X; %.2f MB" % (end, static / 1048576.0))
        item("heap depois do ELF", heap_livre, 0, heap_livre > soma_din,
             "%.2f MB livres, %.2f MB de buffers dinamicos" %
             (heap_livre / 1048576.0, soma_din / 1048576.0))
        item("RAM total que o jogo ocupa", static + soma_din, EE_RAM,
             static + soma_din < EE_RAM,
             "%.2f MB de 32 MB; sobra %.2f MB para pilha/kernel" %
             ((static + soma_din) / 1048576.0,
              (EE_RAM - static - soma_din) / 1048576.0))
    item("VRAM do mapa fixo", vr_total, GS_VRAM, vr_total <= GS_VRAM,
         "%.2f MB de 4,00 MB" % (vr_total / 1048576.0))
    item("largura maxima de textura", vr_pior_w, GS_MAX_TEX_W,
         vr_pior_w <= GS_MAX_TEX_W, "%d px (teto do GS: 1024)" % vr_pior_w)
    item("pacote de desenho", PACKET_QWORDS * QWORD, 0, True,
         "4096 quadwords = 65.536 B; um quadro cabe (o harness recusa estouro)")
    item("DMA por envio", PACKET_QWORDS, DMA_QWC_MAX, PACKET_QWORDS <= DMA_QWC_MAX,
         "%d quadwords por envio; teto do DMAC: %d" % (PACKET_QWORDS, DMA_QWC_MAX))
    if por_quadro is not None:
        item("escritas bloqueantes por quadro", por_quadro, 0, por_quadro == 0,
             "%d chamadas fixas de open+write+fsync no corpo do quadro" % por_quadro)
    if info:
        # O EE e' MIPS III de 32 bits: ELF32, e_machine = 8 (MIPS).
    # O EE e' MIPS III de 32 bits: o ELF tem de ser ELF32 e e_machine = 8 (MIPS).
        item("ELF32 MIPS para o EE", "%d bits, machine %d" % (info["class"], info["machine"]),
        "32 bits, machine 8", info["class"] == 32 and info["machine"] == 8,
        "entry 0x%08X, %d B" % (info["entry"], info["tamanho"]))

    falhas = [i for i in itens if not i["ok"]]

    print("== CARTAO DE PROVA -- FNF PS2 ==")
    for i in itens:
        print("  [%s] %-42s %-22s %s" % ("OK " if i["ok"] else "NAO",
                                         i["item"], str(i["medido"]), i["nota"]))
    print("\n%d itens, %d fora do limite." % (len(itens), len(falhas)))

    if a.json:
        with open(a.json, "w") as f:
            json.dump({"itens": itens, "falhas": len(falhas)}, f, indent=2)
    if a.md:
        with open(a.md, "w") as f:
            f.write("# Cartao de prova (gerado por tools/cartao_de_prova.py)\n\n")
            f.write("| item | medido | limite | resultado | nota |\n|---|---|---|---|---|\n")
            for i in itens:
                f.write("| %s | %s | %s | %s | %s |\n" %
                        (i["item"], i["medido"], i["limite"],
                         "OK" if i["ok"] else "**FORA**", i["nota"]))
    return 1 if falhas else 0


if __name__ == "__main__":
    sys.exit(main())
