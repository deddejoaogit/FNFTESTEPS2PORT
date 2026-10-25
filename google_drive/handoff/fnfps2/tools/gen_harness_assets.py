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
    for f in range(count):
        data = periodic_rgba(frame_bytes, 4096, seed + f)
        blocks.append(lz4_compress_periodic(data, 4096))

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
        "audio_stems.pcm": pcm(1 << 18, 196),
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
        with open(os.path.join(a, name), "wb") as f:
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

    total = sum(len(v) for v in files.values())
    print(f"assets sinteticos: {len(files)} arquivos + {nchart} charts, "
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
