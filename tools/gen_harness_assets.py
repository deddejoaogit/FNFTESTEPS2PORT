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

import json
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


def make_health_bar(w=301, h=10):
    """Moldura da barra: miolo opaco, primeira/ultima linha com alpha parcial e
    uma emenda no meio (a arte oficial tem 601 px e uma emenda em x=300)."""
    out = bytearray(w * h * 4)
    for y in range(h):
        for x in range(w):
            o = (y * w + x) * 4
            lum = 255
            if y == 0 or y == h - 1:
                a = 95
            elif x == 0 or x == w - 1:
                a = 127
            else:
                a = 255
            if y == h - 2 and x > w // 2:
                lum = 191
            out[o + 0] = out[o + 1] = out[o + 2] = lum
            out[o + 3] = a
    return bytes(out)


def splash_regiao():
    """(w, h) da regiao do NOTE SPLASH dentro do atlas do HUD.

    Vem do plano, nao de numero digitado: se `tools/empacotar_splash.py` mudar
    a resolucao, o asset sintetico acompanha e o jogo continua achando o
    arquivo com o tamanho que ele espera.
    """
    caminho = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))),
                           "assets-data", "splash-plan.json")
    if os.path.isfile(caminho):
        d = json.load(open(caminho, encoding="utf-8"))
        return d["regiao"]["w"], d["regiao"]["h"]
    return 224, 124


def make_splash(w, h):
    """Uma splash sintetica com a FORMA da arte: brilho radial, opaco no centro
    e transparente nas bordas.

    A regiao e' uma coluna de 4 quadros (um por linha da faixa), entao cada
    faixa tem de ter conteudo proprio -- um bloco uniforme nao provaria que o
    jogo pega o QUADRO certo do atlas.
    """
    out = bytearray(w * h * 4)
    faixa = h // 4
    for y in range(h):
        quadro = min(3, y // faixa) if faixa else 0
        # o brilho cresce de um ponto para uma cruz, como o da arte
        raio = 0.18 + 0.24 * quadro
        for x in range(w):
            o = (y * w + x) * 4
            nx = (x - w * 0.5) / (w * 0.5)
            ny = ((y % faixa) - faixa * 0.5) / (faixa * 0.5)
            d = (nx * nx + ny * ny) ** 0.5
            a = max(0, min(255, int(255.0 * (1.0 - d / raio + 0.25))))
            out[o + 0] = 255
            out[o + 1] = 200 - quadrant(y, x)
            out[o + 2] = 120
            out[o + 3] = a
    return bytes(out)


def quadrant(y, x):
    return 0 if (x // 7 + y // 5) % 2 else 40


def resultado_folhas():
    """As folhas da tela de RESULTADO, do plano commitado.

    (id, w, h, quadros, fps) de cada uma. Vem de assets-data/resultado-plan.json
    para que o asset sintetico acompanhe o empacotador: se uma folha mudar de
    tamanho, o jogo continua achando o pacote com o tamanho que ele espera.
    """
    caminho = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))),
                           "assets-data", "resultado-plan.json")
    if os.path.isfile(caminho):
        d = json.load(open(caminho, encoding="utf-8"))
        return [(f["id"], f["w"], f["h"], f["quadros"], f["fps"], f["tex"])
                for f in d["folhas"]], d["bg_w"], d["bg_h"]
    return ([("results", 565, 74, 36, 24, "resultado/results.cfz4"),
             ("ratings", 224, 210, 16, 24, "resultado/ratings.cfz4"),
             ("score", 356, 72, 30, 24, "resultado/score.cfz4")], 16, 360)


def make_resultado_pack(w, h, quadros):
    """CFZ4 sintetico com `quadros` quadros de w*h*4 bytes.

    Cada quadro e' um padrao que depende do NUMERO do quadro -- e' o que deixa
    o teste provar que o jogo mostra o quadro certo, e nao o primeiro. Os blocos
    sao LZ4 de literais puros (o mesmo caminho que o empacotador usa quando o
    dado e' incompressivel), entao o decodificador do jogo e' exercitado.
    """
    fb = w * h * 4
    blocos = []
    for q in range(quadros):
        cru = bytes(((q * 53 + i * 11 + 7) & 0xFF) for i in range(fb))
        blocos.append(lz4_literais(cru))
    cab = b"CFZ4" + struct.pack("<III", 1, quadros, fb)
    tabela = bytearray()
    blob = bytearray()
    off = 16 + quadros * 8
    for b in blocos:
        tabela += struct.pack("<II", off, len(b))
        blob += b
        off += len(b)
    return cab + bytes(tabela) + bytes(blob)


def lz4_literais(src):
    """Bloco LZ4 so' de literais: token 0xF0 + extensoes + os bytes."""
    n = len(src)
    out = bytearray()
    if n < 15:
        out.append(n << 4)
    else:
        out.append(0xF0)
        resta = n - 15
        while resta >= 255:
            out.append(255)
            resta -= 255
        out.append(resta)
    return bytes(out) + src


def resultado_ranks():
    """[(arquivo, w, h)] das imagens do rank, do plano commitado."""
    caminho = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))),
                           "assets-data", "resultado-plan.json")
    if not os.path.isfile(caminho):
        return []
    d = json.load(open(caminho, encoding="utf-8"))
    out = []
    vistos = set()
    for r in d.get("ranks", []):
        for tipo in ("vert", "hor"):
            img = r[tipo]
            if img["arquivo"] in vistos:
                continue
            vistos.add(img["arquivo"])
            out.append((img["arquivo"], img["w"], img["h"]))
    return out


def make_rank_img(w, h, vertical):
    """Imagem sintetica do rank: barras de "letra" ao longo do eixo do texto.

    O rank vertical rola em Y e o horizontal em X, entao a arte e' feita de
    faixas perpendiculares -- um bloco uniforme nao provaria que o desenho
    segue o eixo certo.
    """
    out = bytearray(w * h * 4)
    passo = max(4, (h if vertical else w) // 6)
    for y in range(h):
        for x in range(w):
            o = (y * w + x) * 4
            t = y if vertical else x
            dentro = (t % passo) < passo // 2
            out[o + 0] = 255 if dentro else 20
            out[o + 1] = 240 if dentro else 20
            out[o + 2] = 200 if dentro else 30
            out[o + 3] = 255 if dentro else 0
    return bytes(out)


def resultado_score():
    """O atlas do PLACAR, do plano commitado: (arquivo, w, h, frames)."""
    caminho = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))),
                           "assets-data", "resultado-plan.json")
    if not os.path.isfile(caminho):
        return None
    d = json.load(open(caminho, encoding="utf-8"))
    sc = d.get("score")
    if not sc:
        return None
    return sc["atlas"], sc["w"], sc["h"], sc["frames"]


def make_score_atlas(w, h, frames):
    """Atlas sintetico do placar: cada quadro com a COR do (digito, quadro).

    Nao e' so' "tem pixel": o vermelho e' o digito e o verde e' o quadro da
    animacao, entao da' para conferir na captura que o slot mostra o digito
    certo. Fica transparente onde o plano nao poe quadro nenhum.
    """
    out = bytearray(w * h * 4)
    for f in frames:
        for y in range(f["ay"], f["ay"] + f["ah"]):
            for x in range(f["ax"], f["ax"] + f["aw"]):
                o = (y * w + x) * 4
                out[o + 0] = 20 + f["digito"] * 15
                out[o + 1] = 20 + f["quadro"] * 40
                out[o + 2] = 200
                out[o + 3] = 255
    return bytes(out)


def resultado_tallies():
    """O atlas dos contadores de julgamento, do plano: (arquivo, w, h, frames)."""
    caminho = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))),
                           "assets-data", "resultado-plan.json")
    if not os.path.isfile(caminho):
        return None
    d = json.load(open(caminho, encoding="utf-8"))
    tl = d.get("tallies")
    if not tl:
        return None
    return tl["atlas"], tl["w"], tl["h"], tl["frames"]


def make_tally_atlas(w, h, frames):
    """Atlas sintetico dos contadores: cada glifo no seu retangulo.

    O vermelho e' o DIGITO, entao a captura mostra qual numero foi desenhado
    (o branco do original nao daria para distinguir um digito do outro).
    """
    out = bytearray(w * h * 4)
    for f in frames:
        for y in range(f["ay"], f["ay"] + f["ah"]):
            for x in range(f["ax"], f["ax"] + f["aw"]):
                o = (y * w + x) * 4
                out[o + 0] = 30 + f["digito"] * 22
                out[o + 1] = 200
                out[o + 2] = 90
                out[o + 3] = 255
    return bytes(out)


def resultado_clear():
    """Os cinco pedacos do percentual, do plano: (atlases, textos)."""
    caminho = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))),
                           "assets-data", "resultado-plan.json")
    if not os.path.isfile(caminho):
        return None
    d = json.load(open(caminho, encoding="utf-8"))
    return d.get("clear_percent")


def make_cp_atlas(w, h, frames):
    """Atlas sintetico do percentual: cada glifo com o DIGITO no vermelho."""
    out = bytearray(w * h * 4)
    for f in frames:
        for y in range(f["ay"], f["ay"] + f["ah"]):
            for x in range(f["ax"], f["ax"] + f["aw"]):
                o = (y * w + x) * 4
                out[o + 0] = 25 + f["digito"] * 22
                out[o + 1] = 120
                out[o + 2] = 230
                out[o + 3] = 255
    return bytes(out)


def make_cp_img(w, h, tom):
    """Imagem sintetica de texto: um bloco opaco da cor pedida."""
    out = bytearray(w * h * 4)
    for i in range(0, len(out), 4):
        out[i] = tom
        out[i + 1] = tom
        out[i + 2] = 255 - tom
        out[i + 3] = 255
    return bytes(out)


def make_resultado_bg(w, h):
    """O gradiente do fundo, entre as duas cores do ResultState (90 graus)."""
    topo = (0xFE, 0xCC, 0x5C)
    base = (0xFD, 0xC0, 0x5C)
    out = bytearray(w * h * 4)
    for y in range(h):
        t = y / float(h - 1) if h > 1 else 0.0
        r = int(round(topo[0] + (base[0] - topo[0]) * t))
        g = int(round(topo[1] + (base[1] - topo[1]) * t))
        b = int(round(topo[2] + (base[2] - topo[2]) * t))
        for x in range(w):
            o = (y * w + x) * 4
            out[o], out[o + 1], out[o + 2], out[o + 3] = r, g, b, 255
    return bytes(out)


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

        # AUDIO DA TELA DE GAME OVER (mass:/FNFPS2/assets/gameover/).
        # Os nomes sao os que o jogo procura (go_abrir_resolvendo/go_abrir_sfx)
        # e as DURACOES sao curtas de proposito: o harness nao precisa de 38 s
        # de musica para provar que o transporte do game over anda. O que ele
        # cobra e' a CONTA (FADE_TIMER = amostras/336 ms, 2 s de fade para
        # preto, 1 s de volta) e nao uma duracao fixa -- por isso 1 s de
        # gameOverEnd da' ~3 ms de espera e o reinicio fica a 120 quadros.
        # Música: 2 s em loop. Overlay/sfx: 1 s.
        "gameover/gameover.pcm": pcm(2 * 48000 * 4, 262),
        "gameover/gameover-pixel.pcm": pcm(2 * 48000 * 4, 262),
        "gameover/gameover-pico.pcm": pcm(2 * 48000 * 4, 262),
        "gameover/gameover-pixel-pico.pcm": pcm(2 * 48000 * 4, 262),
        "gameover/gameover_end.pcm": pcm(1 * 48000 * 4, 294),
        "gameover/gameover_end-pixel.pcm": pcm(1 * 48000 * 4, 294),
        "gameover/gameover_end-pico.pcm": pcm(1 * 48000 * 4, 294),
        "gameover/gameover_end-pixel-pico.pcm": pcm(1 * 48000 * 4, 294),
        "gameover/loss_sfx.pcm": pcm(1 * 48000 * 4, 196),
        "gameover/loss_sfx-pixel.pcm": pcm(1 * 48000 * 4, 196),
        "gameover/loss_sfx-pico.pcm": pcm(1 * 48000 * 4, 196),
        # `-pico-gutpunch` NAO existe na arte do original (o pico-blazin pede e
        # o Jogo loga erro). Ele NAO e' gerado aqui de proposito: o harness tem
        # de exercitar tambem o caminho "sfx ausente", e um arquivo sintetico
        # com esse nome esconderia a divergencia.
        "gameover/fakeout_death.pcm": pcm(1 * 48000 * 4, 174),
        # MUSICA DA PAUSA (mass:/FNFPS2/assets/pause/). O original toca
        # `breakfast$musicSuffix` em loop, com fade-in de 5 s ate 0,75. Aqui sao
        # 2 s: o harness nao precisa dos 60 s, precisa que o arquivo exista e que
        # o loop e o fade sejam exercitados. `breakfast-pico` existe; o
        # `breakfast-pixel` que o bf-pixel pede NAO existe na arte do original e
        # nao e' gerado de proposito -- o jogo tem de cair no "sem som".
        "pause/breakfast.pcm": pcm(2 * 48000 * 4, 352),
        "pause/breakfast-pico.pcm": pcm(2 * 48000 * 4, 358),

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

        # A BARRA DE VIDA REAL (301x10 RGBA): tools/empacotar_ui.py gera do
        # `shared/images/healthBar.png`. Aqui e' sintetica, mas com a FORMA da
        # moldura do original (borda com alpha parcial, miolo opaco), para o
        # caminho "sobrepor a regiao do atlas" ser exercitado de verdade.
        "health_bar.rgba": make_health_bar(),

        # NOTE SPLASH: o brilho do acerto SICK. Vai para DENTRO do atlas do HUD
        # (na faixa livre abaixo da barra de vida), entao o tamanho tem de ser
        # exatamente o da regiao do plano -- nem um byte a mais.
        "splash.rgba": make_splash(*splash_regiao()),

        # TELA DE RESULTADO: o gradiente do fundo e os CFZ4 das tres folhas.
        # Sem eles o jogo desenha o painel com o texto (o caminho antigo), entao
        # o harness precisa deles para exercitar a arte de verdade.
        "resultado/bg.rgba": make_resultado_bg(*resultado_folhas()[1:]),
        **{"resultado/%s.cfz4" % f[0]: make_resultado_pack(f[1], f[2], f[3])
           for f in resultado_folhas()[0]},
        **{"resultado/%s" % a: make_rank_img(w, h, a.endswith("_vert.rgba"))
           for a, w, h in resultado_ranks()},
        # O atlas do PLACAR: os 52 quadros dos digitos num unico RGBA, no
        # tamanho exato que `g_res_score` espera (sem ele os digitos nao saem).
        **({"resultado/%s" % os.path.basename(resultado_score()[0]):
           make_score_atlas(*resultado_score()[1:])} if resultado_score() else {}),
        # O atlas dos CONTADORES DE JULGAMENTO (sem ele os numeros viram texto).
        **({"resultado/%s" % os.path.basename(resultado_tallies()[0]):
           make_tally_atlas(*resultado_tallies()[1:])}
          if resultado_tallies() else {}),
        # O PERCENTUAL DE CONCLUSAO: tres atlas de digitos e dois textos.
        **({"resultado/%s" % os.path.basename(a["arquivo"]):
           make_cp_atlas(a["w"], a["h"], a["frames"])
           for a in resultado_clear()["atlas"].values()}
          if resultado_clear() else {}),
        **({"resultado/%s" % os.path.basename(t["arquivo"]):
           make_cp_img(t["w"], t["h"], 200)
           for t in resultado_clear()["texto"].values()}
          if resultado_clear() else {}),

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
            # Escala 1,0: a arte sintetica do harness ja' sai no tamanho em que
            # deve ser desenhada, entao canvas = mundo. Um pacote real escreve
            # a escala com que foi reduzido (0,5 nos 46 personagens).
            pacote = _chp4.escrever(quadros, w, h, 32, anims, escala=1.0)
            cab = _chp4.ler(pacote)
            _chp4.conferir(cab)
            destino = os.path.join(a, "chars", cid + ".chp4")
            os.makedirs(os.path.dirname(destino), exist_ok=True)
            with open(destino, "wb") as f:
                f.write(pacote)
            nchar += 1

    # ICONES DE VIDA: um pacote ICP1 por id de icone que o dado pede. Como no
    # resto do harness o CONTEUDO e' sintetico -- o que se prova aqui e' o
    # caminho (abrir, conferir o cabecalho, achar VRAM no pool, subir o quadro e
    # trocar de estado pelo limiar), nao a arte. O TAMANHO e' o de verdade
    # (96x96 com passo 128, ou 32x32 com passo 64 no pixel art), porque e' ele
    # que entra na conta de VRAM.
    nicone = 0
    ids_icone = set()
    if os.path.isdir(chars_json):
        for arq in sorted(os.listdir(chars_json)):
            if not arq.endswith(".json"):
                continue
            d = _json.load(open(os.path.join(chars_json, arq)))
            hi = d.get("healthIcon")
            ids_icone.add((hi or {}).get("id") or "face")
        ids_icone.add("face")
        for iid in sorted(ids_icone):
            pixel = 1 if iid.endswith("-pixel") or iid in (
                "face", "senpai", "senpai-angry", "spirit", "bf-pixel") else 0
            alvo = 32 if pixel else 96
            passo = 64 if pixel else 128
            quadros = 2
            dados = bytearray()
            for q in range(quadros):
                # RGBA5551: opaco, com um tom diferente por quadro para o teste
                # poder ver a troca de estado.
                # RGBA5551 com alfa ligado; o R muda entre os quadros para o
                # harness poder provar que o quadro foi trocado.
                v = 0x8000 | (0x1F if q == 0 else 0x03) | (0x1F << 10)
                dados += (v.to_bytes(2, "little")) * (passo * alvo)
            cab = struct.pack("<4s8H3I", b"ICP1", 1, alvo, alvo, quadros, 16, passo,
                              pixel, 0, passo * alvo * 2, len(dados),
                              sum(dados) & 0xFFFFFFFF)
            destino = os.path.join(a, "icons", iid + ".icp")
            os.makedirs(os.path.dirname(destino), exist_ok=True)
            with open(destino, "wb") as f:
                f.write(cab + bytes(dados))
            nicone += 1

    total = sum(len(v) for v in files.values())
    print(f"assets sinteticos: {len(files)} arquivos + {nchart} charts + "
          f"{npalco} props de palco + {nchar} personagens + {nicone} icones, "
          f"{total:,} bytes, em {a}")
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
