#!/usr/bin/env python3
"""Gera a raiz de assets que o harness precisa para rodar o jogo inteiro.

O jogo le tudo de mass:/FNFPS2/assets/... . Na vida real esses arquivos estao no
memory card do PS2; no host o harness traduz mass:/ para esta pasta.

Por que sintetizar em vez de copiar: os assets de arte nao estao no repositorio
(so os pacotes .S e alguns .lz4f de verificacao). Sem eles o jogo para no
inicio e o harness nao exercita quase nada -- exatamente o cenario que nao
serve para achar erro.

O sintetizador produz conteudo VALIDO para cada formato, com as dimensoes
certas, para que o codigo real de leitura/decodificacao/textura corra. Os
pixels sao padroes deterministicos (nada de aleatorio) para que duas rodadas
sejam comparaveis.

Dois formatos exigem resolver DUAS contas ao mesmo tempo, porque o decodificador
do jogo devolve erro se qualquer uma nao fechar:
  - o RLE do menu (menu_cache.rle e o blob de alfa dentro de menu_sfx.pcm);
  - o pacote CFZ4/LZ4 (bf_frames.lz4f, gf_frames.lz4f).
"""

import os
import struct
import sys

# ------------------------------------------------------------ RLE do menu ----
# Um byte de controle c.
#   c < 128  -> literal: (c+1)*4 bytes copiados literalmente
#   c > 128  -> repeticao: os 4 bytes seguintes repetidos (257-c) vezes
#   c == 128 -> erro no decodificador
# O decodificador exige que o fluxo consuma EXATAMENTE src_bytes e produza
# EXATAMENTE dst_bytes, senao devolve erro. Por isso o gerador resolve as duas
# contas ao mesmo tempo em vez de apenas "comprimir".


def _distribute(total, count, lo=1, hi=128):
    if count == 0:
        if total != 0:
            raise ValueError("nada para distribuir")
        return []
    if total < count * lo or total > count * hi:
        raise ValueError(f"impossivel distribuir {total} em {count} partes [{lo},{hi}]")
    base = total // count
    rem = total - base * count
    return [base + (1 if i < rem else 0) for i in range(count)]


def make_rle(dst_bytes, src_bytes):
    """Fluxo RLE que decodifica exatamente dst_bytes a partir de exatamente
    src_bytes de arquivo."""
    if dst_bytes % 4 or src_bytes < 0:
        raise ValueError("dst_bytes tem de ser multiplo de 4")
    # R tokens de repeticao (128 pixels cada) + L tokens literais.
    #   decoded = 512*R + 4*sum(m)          (m = pixels de cada literal)
    #   arquivo =   5*R + 4*sum(m) + L
    #   =>  arquivo - decoded = L - 507*R
    chosen = None
    for R in range(0, 5_000_000):
        L = 507 * R + (src_bytes - dst_bytes)
        if L < 1:
            continue
        rem_d = dst_bytes - 512 * R
        if rem_d < 0 or rem_d % 4:
            continue
        sum_m = rem_d // 4
        if sum_m < L or sum_m > 128 * L:
            continue
        chosen = (R, L, sum_m)
        break
    if not chosen:
        raise ValueError(f"sem solucao RLE para decoded={dst_bytes} arquivo={src_bytes}")
    R, L, sum_m = chosen
    ms = _distribute(sum_m, L, 1, 128)

    out = bytearray()
    for i in range(R):
        out.append(129)                       # count = 257-129 = 128
        out += bytes(((i * 7) & 0xFF, (i * 13) & 0xFF, (i * 29) & 0xFF, 0x80))
    for i, m in enumerate(ms):
        out.append(m - 1)                     # (c+1)*4 = m*4 bytes
        for j in range(m):
            out += bytes(((i + j * 3) & 0xFF, (i * 5 + j) & 0xFF,
                          (i * 11 + j * 7) & 0xFF, 0x40))
    if len(out) != src_bytes:
        raise ValueError(f"RLE gerou {len(out)} bytes, esperado {src_bytes}")
    return bytes(out)


# ------------------------------------------- RLE de alfa da fonte (1 byte) ----
# Formato diferente, usado por story_font_alpha_decode:
#   c < 128  -> literal: (c+1) pixels, 1 byte de alfa cada
#   c > 128  -> repeticao: (257-c) pixels com o byte de alfa que segue
#   c == 128 -> erro
# Tambem exige consumo e producao exatos.


def make_alpha_rle(pixels, src_bytes):
    if pixels <= 0 or src_bytes < 2:
        raise ValueError("alpha RLE: parametros invalidos")
    # R tokens de repeticao (128 pixels, 2 bytes cada) + L literais somando S.
    #   decoded = 128*R + S
    #   arquivo =   2*R + L + S
    #   =>  arquivo - decoded = L - 126*R
    chosen = None
    for R in range(0, 5_000_000):
        L = 126 * R + (src_bytes - pixels)
        if L < 1:
            continue
        S = pixels - 128 * R
        if S < 0:
            continue
        if S < L or S > 128 * L:
            continue
        chosen = (R, L, S)
        break
    if not chosen:
        raise ValueError(f"sem solucao de alfa para pixels={pixels} arquivo={src_bytes}")
    R, L, S = chosen
    ns = _distribute(S, L, 1, 128)

    out = bytearray()
    for i in range(R):
        out.append(129)                       # 257-129 = 128 pixels
        out.append((i * 37) & 0xFF)
    for i, n in enumerate(ns):
        out.append(n - 1)                     # (c+1) = n pixels
        for j in range(n):
            out.append((i * 11 + j * 3) & 0xFF)
    if len(out) != src_bytes:
        raise ValueError(f"alpha RLE gerou {len(out)} bytes, esperado {src_bytes}")
    return bytes(out)


# ------------------------------------------------------------- LZ4 puro -------
# Compressor LZ4 minimo, sem dependencia externa. Os dados sinteticos sao
# periodicos por construcao, entao um unico match longo cobre o quadro inteiro:
# o fluxo sai com poucas dezenas de bytes, bem abaixo do limite de 8 MB que
# tutorial_pack_load impõe.


def lz4_compress_periodic(data, period):
    """Comprime dados que repetem um bloco de 'period' bytes.

    O fluxo sai como UMA sequencia: 'period' bytes de literais seguidos de um
    match que cobre o resto. No LZ4 os literais e o match dividem o MESMO
    token -- o nibble alto guarda o tamanho dos literais e o baixo guarda
    (match - 4). Emitir dois tokens, um para cada, nao eh LZ4.
    """
    if period <= 0 or period > len(data):
        raise ValueError("period invalido")
    if len(data) % period:
        whole = (len(data) // period) * period
    else:
        whole = len(data)

    out = bytearray()
    lit = period
    match = whole - period                  # bytes cobertos pelo match

    def put_len(value):
        """Nibble + extensao de um tamanho, no formato LZ4.

        O decodificador le bytes de extensao ENQUANTO eles valem 255, e para no
        primeiro diferente. Por isso a extensao comeca direto no primeiro byte
        de tamanho: um 0 inicial faria o laco parar antes da hora.
        """
        ext = bytearray()
        if value < 15:
            return value, ext
        rem = value - 15
        while rem >= 255:
            ext.append(255)
            rem -= 255
        ext.append(rem)
        return 15, ext

    lit_nib, lit_ext = put_len(lit)
    m_nib, m_ext = put_len(match - 4)
    out.append((lit_nib << 4) | m_nib)
    out.extend(lit_ext)
    out.extend(data[:period])
    out.extend(struct.pack("<H", period))    # OFFSET vem antes da extensao
    out.extend(m_ext)

    if whole != len(data):                   # resto parcial: literais puros
        tail = len(data) - whole
        n_nib, n_ext = put_len(tail)
        out.append(n_nib << 4)
        out.extend(n_ext)
        out.extend(data[whole:])
    return bytes(out)


def periodic_rgba(nbytes, period=4096, seed=0):
    """Textura RGBA deterministica e periodica, para o LZ4 acima bastar."""
    base = bytearray(period)
    for i in range(period):
        o = (i // 4) * 4
        base[i] = ((o + i + seed * 7) & 0xFF) if i % 4 != 3 else \
                  (0x00 if (i // 4) % 5 == 0 else 0xFF)
    if nbytes % period:
        return bytes(base) * (nbytes // period) + bytes(base[:nbytes % period])
    return bytes(base) * (nbytes // period)


def make_cfz4(count, frame_bytes, seed=0):
    """Pacote CFZ4: cabecalho + tabela de count entradas + blocos LZ4."""
    header = b"CFZ4" + struct.pack("<III", 1, count, frame_bytes)
    table_size = 16 + count * 8
    blocks = []
    # O periodo nao pode passar do tamanho do quadro: prop pequeno (201x25,
    # por exemplo) tem 10.050 bytes e isso ja' derrubou a geracao com
    # "period invalido". O periodo so' muda o padrao sintetico; o formato do
    # pacote e' o mesmo, e o decodificador do jogo nao sabe de periodo.
    per = 4096 if frame_bytes >= 8192 else max(4, frame_bytes // 2)
    for f in range(count):
        data = periodic_rgba(frame_bytes, per, seed + f)
        blocks.append(lz4_compress_periodic(data, per))

    entries = bytearray()
    blob = bytearray()
    off = table_size
    for b in blocks:
        entries += struct.pack("<II", off, len(b))
        blob += b
        off += len(b)
    if off > 8 * 1024 * 1024:
        raise ValueError(f"pacote CFZ4 com {off} bytes passa do limite de 8 MB")
    return header + bytes(entries) + bytes(blob)


# ------------------------------------------------------------- texturas ------


def rgba_pattern(w, h, seed=0, alpha=0xFF):
    out = bytearray(w * h * 4)
    for y in range(h):
        for x in range(w):
            o = (y * w + x) * 4
            out[o + 0] = (x * 3 + y + seed) & 0xFF
            out[o + 1] = (x + y * 5 + seed) & 0xFF
            out[o + 2] = (x * 7 + y * 2 + seed * 3) & 0xFF
            out[o + 3] = alpha if ((x >> 3) + (y >> 3)) % 3 else 0x00
    return bytes(out)


def pcm(n, freq=440, rate=48000):
    out = bytearray(n)
    half = max(1, rate // freq // 2)
    for i in range(n // 2):
        v = 0x1000 if (i // half) % 2 else -0x1000
        struct.pack_into("<h", out, i * 2, v)
    return bytes(out)


# ------------------------------------------------------------------ assets ----
MENU_CACHE = 2_961_848
MENU_RLE = 1_288_182
FONT_RLE = 38_342
STORY_RLE = 72_402
STORY_TITLES_RLE = 188_635
PRELOAD = MENU_RLE + FONT_RLE + STORY_RLE + STORY_TITLES_RLE

# O blob de alfa da fonte do story mora DENTRO de menu_sfx.pcm, neste offset.
SFX_ALPHA_OFF = 241_920 + 22_656
SFX_ALPHA_LEN = 7_672
SFX_ALPHA_PIXELS = 160 * 120
SFX_TOTAL = 281_088


def build(root, charts_src=None):
    a = os.path.join(root, "FNFPS2", "assets")
    os.makedirs(os.path.join(a, "songs", "tutorial"), exist_ok=True)

    sfx = bytearray(pcm(SFX_TOTAL, 220))
    sfx[SFX_ALPHA_OFF:SFX_ALPHA_OFF + SFX_ALPHA_LEN] = \
        make_alpha_rle(SFX_ALPHA_PIXELS, SFX_ALPHA_LEN)

    files = {
        "menu_cache.rle":
            make_rle(MENU_CACHE, MENU_RLE)
            + make_rle(384 * 192 * 4, FONT_RLE)
            + make_rle(512 * 256 * 4, STORY_RLE)
            + make_rle(306_180, STORY_TITLES_RLE),
        "menu_sfx.pcm": bytes(sfx),
        # O stem do tutorial tem de durar o que o chart dura. Eram 256 KB
        # (65.536 amostras = 1,36 s) e o chart tem 3.225.600 amostras (67 s):
        # o jogo, corretamente, concluia "musica acabou" no quadro 817 e
        # congelava o transporte -- e as primeiras notas (a 9,6 s) nunca
        # chegavam ao receptor. Formato: inst L,R + voices L,R = 8 B/amostra.
        # Audio POR MUSICA, no caminho que o jogo procura primeiro
        # (<root>/songs/<id>/stems.pcm). O audio_stems.pcm plano continua sendo
        # gerado porque e' a reserva que o codigo usa quando o do pendrive
        # nao existe -- e as duas rotas precisam estar cobertas.
        "songs/tutorial/stems.pcm": pcm(3225600 * 8, 196),
        "audio_stems.pcm": pcm(3225600 * 8, 196),
        "freakyMenu_48000.pcm": pcm(1 << 18, 330),

        "font.rgba": rgba_pattern(384, 192, 1),
        "title_alphabet.rgba": rgba_pattern(384, 192, 2),
        "title_prompt_l.rgba": rgba_pattern(512, 192, 3),
        "title_prompt_r.rgba": rgba_pattern(320, 192, 4),
        "title_newgrounds.rgba": rgba_pattern(181, 175, 5),
        "mainstage_tutorial_parallax.rgba": rgba_pattern(512, 256, 6),
        "tutorial_hud_official_320x256_gsalpha.rgba": rgba_pattern(320, 256, 7),

        "title_logoBumpin.frames": rgba_pattern(320, 240, 8),
        "title_gfDanceTitle.frames": rgba_pattern(256, 235, 9),

        "font.widths": bytes([(i % 24) + 4 for i in range(96)]),

        # Tutorial: pacotes CFZ4/LZ4, 43 quadros de BF e 87 de GF.
        "bf_frames.lz4f": make_cfz4(43, 258 * 254 * 4, seed=1),
        "gf_frames.lz4f": make_cfz4(87, 320 * 307 * 2, seed=2),
    }

    for name, data in files.items():
        dest = os.path.join(a, name)
        # Nomes com subpasta (songs/tutorial/stems.pcm) precisam do diretorio.
        os.makedirs(os.path.dirname(dest) or a, exist_ok=True)
        with open(dest, "wb") as f:
            f.write(data)

    # Os charts sao os DE VERDADE, convertidos pelo proprio conversor do
    # repositorio -- nao faz sentido sintetizar chart quando o real existe.
    nchart = 0
    if charts_src and os.path.isdir(charts_src):
        for diff in ("easy", "normal", "hard"):
            src = os.path.join(charts_src, f"tutorial-{diff}.chart")
            if os.path.exists(src):
                with open(src, "rb") as f:
                    data = f.read()
                with open(os.path.join(a, "songs", "tutorial", f"{diff}.chart"), "wb") as f:
                    f.write(data)
                nchart += 1

    # PALCOS: pacotes CFZ4 dos props, um por prop, no caminho que o jogo le'
    # (assets/stages/<palco>/<prop>.cfz4).
    #
    # Sao gerados TODOS os 21 (eram 2, "os que o harness exercita"). O motivo
    # de serem todos e' medido: o sweep (`tools/run_sweep_harness.sh`) toca as
    # 27 musicas, e cada musica mora num palco. Com 2 palcos, 60 das 81
    # combinacoes morriam em "arquivo nao abriu" -- os outros 19 palcos nunca
    # tinham passado pelo caminho pacote -> pool -> tela. O conteudo sintetico
    # nao prova arte; prova o encanamento de todos eles, que era o buraco.
    # Custo: ~157 props de ~28 KB = 4,4 MB (o build/ nao e' versionado).
    npalco = 0
    if charts_src:
        plan = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))),
                            "assets-data", "stage-props.json")
    else:
        plan = None
    if plan and os.path.isfile(plan):
        import json as _json
        dados = _json.load(open(plan))
        for st in dados["stages"]:
            for p in st["props"]:
                destino = os.path.join(a, p["tex"])
                os.makedirs(os.path.dirname(destino), exist_ok=True)
                with open(destino, "wb") as f:
                    f.write(make_cfz4(p["frames"], p["sw"] * p["sh"] * 2, seed=11))
                npalco += 1

    # PERSONAGENS: um pacote CHP4 por personagem, no caminho que o jogo le'
    # (assets/chars/<id>.chp4), com a tabela de animacoes junto dos quadros.
    #
    # O CONTEUDO E' SINTETICO, e o que ele prova e' o encanamento: abrir, tirar
    # a animacao certa da tabela, descomprimir o quadro, achar lugar na VRAM e
    # desenhar -- nos 46 personagens, nao nos dois escritos no codigo. Nao prova
    # arte. O numero de quadros de cada animacao TAMBEM e' sintetico (3, ou o
    # `frameIndices` quando a JSON ja' traz), porque a contagem real vive no
    # atlas; quem a preenche de verdade e' tools/pacote_personagem.py, que le' o
    # clone da arte. O jogo CONFERE a tabela contra os quadros (cp_conferir), e
    # aqui o gerador faz a mesma conferencia em Python antes de escrever.
    nchar = 0
    chars_json = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))),
                              "assets-data", "characters")
    if os.path.isdir(chars_json):
        import json as _json
        sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
        import chp4 as _chp4
        for arq in sorted(os.listdir(chars_json)):
            if not arq.endswith(".json"):
                continue
            dados = _json.load(open(os.path.join(chars_json, arq)))
            # O NOME DO ARQUIVO e' o id do personagem ("bf", "dad",
            # "bf-holding-gf"): e' ele que as musicas e o char_dados usam. O
            # campo `name` da JSON e' o nome de EXIBICAO ("Boyfriend (Holding
            # GF)") -- usar o `name` gerava pacotes que o jogo nunca acharia.
            cid = arq[:-5]
            anims, primeiro = [], 0
            for an in dados.get("animations", []):
                quantos = len(an.get("frameIndices") or []) or 3
                looped = 1 if an.get("looped", True) else 0
                anims.append({"nome": an["name"], "primeiro": primeiro,
                              "quantos": quantos, "fps100": 2400, "looped": looped})
                primeiro += quantos
            if not anims:
                continue
            # Canvas pequeno de proposito: 46 personagens x N quadros precisa
            # caber num build/ de teste, e o que se mede aqui e' o caminho, nao
            # os bytes.
            w, h = 64, 64
            quadros = [bytes(((q * 7 + i) & 0xff) for i in range(w * h * 4))
                       for q in range(primeiro)]
            pacote = _chp4.escrever(quadros, w, h, 32, anims)
            cab = _chp4.ler(pacote)
            _chp4.conferir(cab)
            destino = os.path.join(a, "chars", cid + ".chp4")
            os.makedirs(os.path.dirname(destino), exist_ok=True)
            with open(destino, "wb") as f:
                f.write(pacote)
            nchar += 1

    total = sum(len(v) for v in files.values())
    print(f"assets sinteticos: {len(files)} arquivos + {nchart} charts + "
          f"{npalco} props de palco + {nchar} personagens, {total:,} bytes, em {a}")
    for name in ("menu_cache.rle", "bf_frames.lz4f", "gf_frames.lz4f"):
        print(f"  {name:22s} {len(files[name]):>10,} bytes")
    if not nchart:
        print("  ATENCAO: nenhum chart do tutorial copiado "
              "(rode test/run_tests.sh antes para gera-los)")
    return a


if __name__ == "__main__":
    root = sys.argv[1] if len(sys.argv) > 1 else "build/harness_assets"
    charts = sys.argv[2] if len(sys.argv) > 2 else "build/charts"
    build(root, charts)
