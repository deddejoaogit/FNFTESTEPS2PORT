#!/usr/bin/env python3
"""Valida os assets REAIS (os que estao no Drive e vao para o memory card do
PS2) contra os decodificadores do jogo.

Por que isso importa: o harness roda com assets SINTETICOS que eu gerei. Se o
asset real tiver um byte diferente do que o decodificador espera, o jogo nao
trava -- ele desenha lixo ou trava mais tarde, longe da causa. Este verificador
pega isso ANTES de ir para o console.

Uso:  python3 tools/check_real_assets.py [diretorio-dos-assets]
"""
import os
import struct
import sys

REPO = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))


def die(msg):
    print("FALHOU: " + msg)
    sys.exit(1)


# ---------------------------------------------------------------- RLE do menu
# O jogo le menu_cache.rle de uma vez so para g_menu_compressed, com
# PRELOAD_RLE_BYTES bytes, e depois decodifica 4 blocos RLE emendados, cada um
# com seu proprio tamanho de origem e de destino (src/main.c):
#   menu_decode_step   : MENU_RLE_BYTES            -> MENU_CACHE_BYTES
#   menu_font_restore  : FONT_RLE_BYTES            -> FONT_W*FONT_H*4
#   story_ui_restore   : STORY_RLE_BYTES           -> 512*256*4
#   story_titles_restore: STORY_TITLES_RLE_BYTES   -> STORY_TITLES_RAW_BYTES
MENU_RLE_BYTES = 1288182
FONT_RLE_BYTES = 38342
STORY_RLE_BYTES = 72402
STORY_TITLES_RLE_BYTES = 188635
PRELOAD_RLE_BYTES = (MENU_RLE_BYTES + FONT_RLE_BYTES +
                     STORY_RLE_BYTES + STORY_TITLES_RLE_BYTES)

MENU_CACHE_BYTES = 2961848          # MENU_CACHE_BYTES
FONT_RAW = 384 * 192 * 4            # FONT_W*FONT_H*4
STORY_RAW = 512 * 256 * 4           # 512*256*4
STORY_TITLES_RAW = 306180           # STORY_TITLES_RAW_BYTES


def rle_block(src, want, label):
    """Replica de rle_decode/menu_decode_step para UM bloco: exige consumo e
    producao exatos, senao o jogo nao liga g_menu_cache_ready / devolve erro."""
    si = 0
    di = 0
    n = len(src)
    while si < n and di < want:
        c = src[si]
        si += 1
        if c < 128:
            k = (c + 1) * 4
            if si + k > n or di + k > want:
                die("%s: literal estoura o bloco em %d" % (label, si))
            di += k
            si += k
        elif c > 128:
            k = (257 - c) * 4
            if si + 4 > n or di + k > want:
                die("%s: repeat estoura o bloco em %d" % (label, si))
            di += k
            si += 4
        else:
            die("%s: token 128 (invalido) no offset %d" % (label, si - 1))
    if si != n:
        die("%s: sobrou %d bytes no bloco (consumo exato obrigatorio)"
            % (label, n - si))
    if di != want:
        die("%s: produziu %d de %d bytes" % (label, di, want))


# ------------------------------------------------- LZ4 / CFZ4 dos pacotes
def lz4_decompress_block(src, expected):
    """Replica do descompressor LZ4 do jogo: token -> extensao de literais ->
    literais -> offset de 2 bytes -> extensao de match. Sem byte marcador."""
    out = bytearray()
    si = 0
    n = len(src)
    while si < n:
        tok = src[si]
        si += 1
        lit = tok >> 4
        if lit == 15:
            while True:
                if si >= n:
                    die("lz4: extensao de literais passou do fim")
                b = src[si]
                si += 1
                lit += b
                if b != 255:
                    break
        if si + lit > n:
            die("lz4: literais passam do fim do bloco")
        out += src[si:si + lit]
        si += lit
        if si == n:
            break
        if si + 2 > n:
            die("lz4: offset passou do fim do bloco")
        off = src[si] | (src[si + 1] << 8)
        si += 2
        if off == 0:
            die("lz4: offset zero")
        mlen = tok & 15
        if mlen == 15:
            while True:
                if si >= n:
                    die("lz4: extensao de match passou do fim")
                b = src[si]
                si += 1
                mlen += b
                if b != 255:
                    break
        mlen += 4
        if off > len(out):
            die("lz4: offset %d maior que a saida %d" % (off, len(out)))
        start = len(out) - off
        for i in range(mlen):
            out.append(out[start + i])
    if len(out) != expected:
        die("lz4: bloco produziu %d bytes, esperado %d" % (len(out), expected))
    return bytes(out)


def check_lz4f(path, count, frame_bytes, label):
    data = open(path, "rb").read()
    if len(data) < 16:
        die("%s: arquivo curto demais para o cabecalho CFZ4" % label)
    if data[:4] != b"CFZ4":
        die("%s: magia '%s' em vez de 'CFZ4'" % (label, data[:4]))
    ver, cnt = struct.unpack_from("<II", data, 4)
    if ver != 1:
        die("%s: versao %d, o jogo so aceita 1" % (label, ver))
    if cnt != count:
        die("%s: cabecalho diz %d quadros, o jogo espera %d" % (label, cnt, count))
    table = 16 + 8 * cnt
    if len(data) < table:
        die("%s: tabela de offsets estoura o arquivo" % label)
    total_frames = 0
    for i in range(cnt):
        off, size = struct.unpack_from("<II", data, 16 + 8 * i)
        if off + size > len(data):
            die("%s: quadro %d aponta para fora do arquivo (off=%d size=%d, "
                "arquivo=%d)" % (label, i, off, size, len(data)))
        if size == 0:
            die("%s: quadro %d tem tamanho zero" % (label, i))
        lz4_decompress_block(data[off:off + size], frame_bytes)
        total_frames += 1
    if len(data) >= 8 * 1024 * 1024:
        die("%s: %d bytes, acima do limite de 8 MB que tutorial_pack_load impoe"
            % (label, len(data)))
    print("  %-28s %10d B, %d quadros de %d B -- OK"
          % (label, len(data), total_frames, frame_bytes))


# ------------------------------------------------- RLE de alfa do story
def story_font_alpha_decode(blob, expected_si, expected_di):
    """Replica de story_font_alpha_decode: 1 byte de controle por bloco.
    c<128 -> literal (c+1) px; c>128 -> repeat (257-c) px."""
    si = 0
    di = 0
    n = len(blob)
    while si < n:
        c = blob[si]
        si += 1
        if c == 128:
            die("story_font_alpha: controle 128 (invalido) no offset %d" % (si - 1))
        if c < 128:
            k = c + 1
            if si + k > n:
                die("story_font_alpha: literal estoura o blob")
            si += k
            di += k
        else:
            k = 257 - c
            if si + 1 > n:
                die("story_font_alpha: repeat estoura o blob")
            si += 1
            di += k
    if si != expected_si:
        die("story_font_alpha: consumiu %d bytes, o jogo exige %d" % (si, expected_si))
    if di != expected_di:
        die("story_font_alpha: produziu %d px, o jogo exige %d" % (di, expected_di))


def check_size(path, expected, label):
    got = os.path.getsize(path)
    if got != expected:
        die("%s: %d bytes, o jogo espera %d" % (label, got, expected))
    print("  %-28s %10d B -- OK" % (label, got))


def main():
    root = sys.argv[1] if len(sys.argv) > 1 else os.path.join(REPO, "google_drive")
    print("== validando os assets reais em %s ==" % root)
    for f in ("menu_cache.rle", "bf_frames.lz4f", "gf_frames.lz4f", "menu_sfx.pcm",
              "font.rgba", "font.widths", "title_alphabet.rgba",
              "title_prompt_l.rgba", "title_prompt_r.rgba",
              "title_logoBumpin.frames", "title_gfDanceTitle.frames",
              "title_newgrounds.rgba", "mainstage_tutorial_parallax.rgba",
              "tutorial_hud_official_320x256_gsalpha.rgba"):
        p = os.path.join(root, f)
        if not os.path.exists(p):
            die("nao achei %s" % p)

    # texturas cruas: o tamanho tem de bater exato, senao o upload estoura
    check_size(os.path.join(root, "font.rgba"), 384 * 192 * 4, "font.rgba")
    check_size(os.path.join(root, "font.widths"), 96, "font.widths")
    check_size(os.path.join(root, "title_alphabet.rgba"), 384 * 192 * 4, "title_alphabet.rgba")
    check_size(os.path.join(root, "title_prompt_l.rgba"), 512 * 192 * 4, "title_prompt_l.rgba")
    check_size(os.path.join(root, "title_prompt_r.rgba"), 320 * 192 * 4, "title_prompt_r.rgba")
    check_size(os.path.join(root, "title_newgrounds.rgba"), 181 * 175 * 4, "title_newgrounds.rgba")
    check_size(os.path.join(root, "mainstage_tutorial_parallax.rgba"), 512 * 256 * 4,
               "mainstage_tutorial_parallax.rgba")
    check_size(os.path.join(root, "tutorial_hud_official_320x256_gsalpha.rgba"),
               320 * 256 * 4, "tutorial_hud_..._gsalpha.rgba")
    check_size(os.path.join(root, "title_logoBumpin.frames"), 320 * 240 * 4 * 15,
               "title_logoBumpin.frames")
    check_size(os.path.join(root, "title_gfDanceTitle.frames"), 256 * 235 * 4 * 30,
               "title_gfDanceTitle.frames")

    # menu_cache.rle: 4 blocos RLE emendados, cada um com tamanho de origem e de
    # destino proprios. O jogo le EXATAMENTE PRELOAD_RLE_BYTES; se o arquivo for
    # maior, o resto e ignorado -- e se for menor, read_exact_path devolve -2 e o
    # jogo morre em CACHE_RLE_FAIL antes de desenhar um quadro.
    mc = open(os.path.join(root, "menu_cache.rle"), "rb").read()
    print("  %-28s %10d B (o jogo espera %d)" %
          ("menu_cache.rle", len(mc), PRELOAD_RLE_BYTES))
    if len(mc) < PRELOAD_RLE_BYTES:
        die("menu_cache.rle tem %d bytes, MENOS que os %d que o jogo le: "
            "read_exact_path devolve -2 e o jogo morre em CACHE_RLE_FAIL"
            % (len(mc), PRELOAD_RLE_BYTES))
    if len(mc) > PRELOAD_RLE_BYTES:
        print("     AVISO: %d bytes a mais no fim do arquivo; o jogo le so os "
              "primeiros %d e ignora o resto" % (len(mc) - PRELOAD_RLE_BYTES,
                                                 PRELOAD_RLE_BYTES))
    blocks = [
        (0, MENU_RLE_BYTES, MENU_CACHE_BYTES, "bloco menu (atlas)"),
        (MENU_RLE_BYTES, FONT_RLE_BYTES, FONT_RAW, "bloco fonte"),
        (MENU_RLE_BYTES + FONT_RLE_BYTES, STORY_RLE_BYTES, STORY_RAW,
         "bloco story ui"),
        (MENU_RLE_BYTES + FONT_RLE_BYTES + STORY_RLE_BYTES, STORY_TITLES_RLE_BYTES,
         STORY_TITLES_RAW, "bloco story titulos"),
    ]
    for off, size, raw, label in blocks:
        rle_block(mc[off:off + size], raw, label)
        print("     %-22s offset %8d, %7d B -> %8d px -- OK"
              % (label, off, size, raw))

    # pacotes CFZ4/LZ4
    check_lz4f(os.path.join(root, "bf_frames.lz4f"), 43, 258 * 254 * 4, "bf_frames.lz4f")
    check_lz4f(os.path.join(root, "gf_frames.lz4f"), 87, 320 * 307 * 2, "gf_frames.lz4f")

    # o RLE de alfa mora DENTRO de menu_sfx.pcm, no offset 264576
    ms = open(os.path.join(root, "menu_sfx.pcm"), "rb").read()
    check_size(os.path.join(root, "menu_sfx.pcm"), 281088, "menu_sfx.pcm")
    off = 264576
    if len(ms) < off + 7672:
        die("menu_sfx.pcm: o blob de alfa nao caberia no offset %d" % off)
    story_font_alpha_decode(ms[off:off + 7672], 7672, 160 * 120)
    print("  %-28s blob no offset %d, 7672 B -> 19200 px -- OK"
          % ("menu_sfx.pcm (alfa)", off))

    print("TODOS EXATOS")


main()
