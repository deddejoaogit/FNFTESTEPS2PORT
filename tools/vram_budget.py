#!/usr/bin/env python3
"""VRAM budget for every stage, computed from MEASURED image dimensions.

WHAT CHANGED AND WHY
--------------------
The first version of this tool assumed the port uploads a whole atlas per prop,
at 32 bpp, padded to a power of two. Every one of those three assumptions turned
out to be wrong for the port that actually runs:

  * the atlas is never resident -- src/main.c keeps the frames LZ4-compressed in
    main RAM and uploads ONE decoded frame (`tutorial_pack_load` /
    `tutorial_pack_frame`), so VRAM pays for one frame, not for the sheet;
  * bpp is 16 where it matters -- the girlfriend's atlas is already uploaded as
    GS_PSM_16, and tools/pacote_personagem.py shows 16 bpp is what lets the
    characters stay at their native size;
  * the GS needs no power-of-two extent for these textures -- the port uploads
    320x307 and 384x192 today.

A budget built on the wrong model is worse than no budget: it reported numbers
that looked settled. This one is computed from assets-data/image-sizes.json,
which tools/measure_assets.py fills from the real pixels, and it reports its
inputs rather than hiding them.

WHAT IS CHARGED
    2 framebuffers          2 x 512 x 384 x 2   (PSMCT16, measured to fit)
    font atlas              384 x 192 x 4
    HUD + note atlas        2 x 320 x 256 x 2
    three characters        one frame each, at the largest frame of the atlas
    the stage               one frame per unique prop, at scale f

The stage scale f is the answer the tool prints: the layout and the scroll
factors are preserved, only the pixels shrink.

Run from the project root: python3 tools/vram_budget.py [--bpp 16]
"""
import argparse
import glob
import json
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
PROJECT = os.path.dirname(HERE)
STAGES = os.path.join(PROJECT, "assets-data", "stages")
SIZES = os.path.join(PROJECT, "assets-data", "image-sizes.json")

# SCPH-90006, 4 MB of GS VRAM.
VRAM_TOTAL = 4194304

# The framebuffers the port ACTUALLY allocates (src/engine/vram_layout.c): two
# 640x448 PSMCT16. The previous version of this tool modelled 512x384, which the
# port never used -- a budget computed on a screen the game does not have is a
# budget for another game. Two 640x448 PSMCT16 = 1.146.880 B, and that is what
# the console shows in the boot log (vram_livre 1.269.760 after every slot).
FB_W, FB_H = 640, 448
FB_COUNT = 2
FB_PSM_BITS = 16

FONT_W, FONT_H = 384, 192
HUD_W, HUD_H = 320, 256
NOTE_W, NOTE_H = 320, 256

# The three character slots the port always fills (bf, gf, dad).
CHAR_SLOTS = 3


def load_sizes():
    if not os.path.isfile(SIZES):
        sys.exit("missing %s -- run tools/measure_assets.py first" % SIZES)
    return json.load(open(SIZES))["images"]


def frame_dims(entry):
    """The largest single frame of an atlas -- what one resident frame costs."""
    if not entry:
        return None
    w = entry.get("frame_w") or entry.get("w")
    h = entry.get("frame_h") or entry.get("h")
    if not w or not h:
        return None
    return (w, h)


def stage_props(sid):
    """-> [(assetPath, (w, h))] unique props, in zIndex order, gaps dropped."""
    d = json.load(open(os.path.join(STAGES, sid + ".json")))
    out = []
    seen = set()
    for p in d.get("props") or []:
        ap = p.get("assetPath")
        if not ap or ap.startswith("#") or ap in seen:
            continue
        seen.add(ap)
        e = frame_dims(SIZES_CACHE.get(ap))
        if e:
            out.append((ap, e))
    return out


def stage_bytes(sid, f, bpp):
    return sum(max(1, int(w * f)) * max(1, int(h * f)) * (bpp // 8)
               for _, (w, h) in stage_props(sid))


def max_scale(sid, budget, bpp):
    """Largest uniform scale whose total stays inside `budget`."""
    if stage_bytes(sid, 1.0, bpp) <= budget:
        return 1.0
    lo, hi = 0.0, 1.0
    for _ in range(50):
        mid = (lo + hi) / 2.0
        if stage_bytes(sid, mid, bpp) <= budget:
            lo = mid
        else:
            hi = mid
    return lo


# ---------------------------------------------------------------------------
# Orcamento de RAM (o que o VRAM nao conta)
#
# O empacotador dimensionava tudo por VRAM e nunca olhou a RAM principal. O
# pacote CFZ4 inteiro de um palco fica na memoria do Emotion Engine enquanto a
# musica toca (tutorial_pack_load), e o EE tem 32 MiB no total.
#
# Cada parcela abaixo tem uma fonte, e nenhuma e' chute:
#   EE_RAM_TOTAL       32 MiB do Emotion Engine.
#   ELF_IMAGE_BYTES    tamanho do FNF_PS2.ELF da build 0.2.116, medido em
#                      docs/DIAGNOSTICO_TRAVA_CONSOLE.md. Cresce com o jogo,
#                      por isso o check_stage_budget compara com o ELF atual
#                      quando ele existe em build/.
#   RAM_BUFFERS_BYTES  soma dos buffers que o jogo aloca, lida do PROPRIO jogo:
#                      evento ram_buffers do log (7.899.109 B no boot da
#                      0.2.117). Nao e' uma estimativa de planilha.
#   RAM_STAGING_BYTES  janela de trabalho: um quadro descomprimido do maior prop
#                      + o bloco LZ4 + o anel de audio. 4 MiB, com folga.
# ---------------------------------------------------------------------------
EE_RAM_TOTAL = 32 * 1024 * 1024
ELF_IMAGE_BYTES = 5278492
RAM_BUFFERS_BYTES = 7899109
RAM_STAGING_BYTES = 4 * 1024 * 1024


def ram_limit_per_stage(elf_bytes=ELF_IMAGE_BYTES, buffers_bytes=RAM_BUFFERS_BYTES,
                        staging_bytes=RAM_STAGING_BYTES):
    """Quanto o pacote de UM palco pode ocupar na RAM principal."""
    return EE_RAM_TOTAL - elf_bytes - buffers_bytes - staging_bytes


def stage_ram_bytes(props):
    """Pacote CFZ4 inteiro de um palco: sw*sh*2 por quadro, por prop."""
    return sum(p["sw"] * p["sh"] * p["frames"] * 2 for p in props)


def stage_stream_ram_bytes(props):
    """RAM de um palco com o leitor por quadro (engine/pack_stream.c).

    O pacote NAO fica na RAM: ficam a TABELA de (offset, tamanho) de cada
    quadro -- 8 bytes por quadro, que o `pst_abrir` copia para dois vetores de
    u32, ou seja 16 B por quadro no pior caso -- mais UM bloco comprimido, cujo
    tamanho real so' o empacotador conhece. Aqui o bloco entra pelo PIOR caso,
    frame_bytes: dado incompressivel. E' limite superior, nao estimativa.
    """
    tabela = sum(16 * p["frames"] for p in props)
    bloco = max([p["sw"] * p["sh"] * 2 for p in props] or [0])
    return tabela + bloco


def ram_scale_limit(native, frames, budget):
    """Maior escala uniforme cujo pacote inteiro cabe em `budget` bytes.

    `native` e' [(w, h)] do canvas de cada prop e `frames` a lista paralela com
    o numero de quadros de cada um. A VRAM so' paga UM quadro por prop; a RAM
    paga TODOS, e por isso a conta e' outra -- foi exatamente essa diferenca que
    deixou o spookyMansion com 59 MB de pacote.
    """
    if not native:
        return 1.0

    def total(f):
        return sum(max(1, int(round(w * f))) * max(1, int(round(h * f))) * 2 * n
                   for (w, h), n in zip(native, frames))

    if total(1.0) <= budget:
        return 1.0
    lo, hi = 0.0001, 1.0
    for _ in range(60):
        mid = (lo + hi) / 2.0
        if total(mid) <= budget:
            lo = mid
        else:
            hi = mid
    return lo


# ---------------------------------------------------------------------------
# A CONTA EXATA, que e' a do RUNTIME
#
# O pool aloca `sp_prop_bytes(prop)` = `sw*sh*2` arredondado para 256 B (o
# alinhamento do TBP0), e o MESMO numero esta' em
# src/engine/stage_vram_data.h, que tools/gen_stage_vram.py gera deste plano.
# O orcamento usava `cp_tex_bytes`, que TAMBEM arredonda a LARGURA para 64 (o
# TBW do GS) -- e cobrava de 33 a 173 KB a mais por palco do que o jogo aloca.
# Um orcamento que nao e' o do jogo nao decide nada: era isso que fazia
# `tools/orcamento_vram.py` reprovar tres musicas que o runtime roda.
# ---------------------------------------------------------------------------
VC_ALINHAMENTO = 256


def _alinhar(v, a):
    return (v + (a - 1)) & ~(a - 1)


def prop_vram_bytes(sw, sh):
    """Bytes que o pool reserva para UM quadro de um prop (sp_prop_bytes)."""
    return _alinhar(int(sw) * int(sh) * 2, VC_ALINHAMENTO)


def splash_bytes():
    """VRAM que o NOTE SPLASH custa: ZERO, e isso e' medido, nao suposto.

    A splash nao e' textura nova -- ela e' uma regiao da faixa vazia do atlas do
    HUD (`tex2`, 320x256), logo abaixo da barra de vida. As notas ocupam
    (lane*56, 0) 56x56 e a barra (0,112) 301x10; de y=128 para baixo nao ha'
    nada. `tools/empacotar_splash.py` escolhe a maior resolucao que cabe ali
    (224x124 no divisor 4,75) e `src/engine/splash_dados.h` guarda a regiao.

    A funcao existe para que a pergunta "quanto a splash custa?" tenha UMA
    resposta no projeto, e ela e' zero. Se um dia a splash ganhar textura
    propria, e' aqui que o numero entra -- e o orcamento das 27 musicas vai
    reprovar antes do console.
    """
    return 0


def fixos_bytes():
    """O que fica FORA do pool (orcamento_vram.py e src/main.c, iguais).

    O NOTE SPLASH nao aparece nesta soma de proposito: ver splash_bytes().
    """
    # fb_words() do orcamento_vram.py conta PALAVRAS de um framebuffer (o GS
    # endereca em 32 bits); os dois framebuffers PSMCT16 de 640x448 sao 2 x
    # 143.360 palavras x 4 = 1.146.880 B.
    fb = 2 * _alinhar(FB_W * (FB_H // 2), 2048) * 4
    tex2 = _alinhar(HUD_W * HUD_H, 64) * 4             # notas + HUD, PSMCT32
    fonte = _alinhar(FONT_W * FONT_H, 64) * 4
    gf = _alinhar(320 * (64 // 2), 64) * 4             # GF do tutorial, PSMCT16
    return fb + tex2 + fonte + gf


# Dois icones de vida por musica, medidos nos pacotes reais (build/packs/icons).
ICONES_BYTES_PADRAO = 49152


def _chp4_bytes(caminho):
    import struct
    with open(caminho, "rb") as f:
        cab = f.read(36)
    if len(cab) < 36 or cab[:4] != b"CHP4":
        return 0
    _v, _nq, _bpq, w, h, _na, psm, _r = struct.unpack("<8I", cab[4:36])
    passo = (w + 63) & ~63
    return _alinhar(passo * h * (2 if psm == 16 else 4), VC_ALINHAMENTO)


def reserva_por_palco(packs_dir=None, songs_dir=None):
    """{palco: (bytes_dos_3_personagens, bytes_dos_2_icones)} no PIOR caso.

    O limite do palco tem de ser o da musica MAIS PESADA que usa aquele palco:
    reservar o pior caso global encolheria palcos que nao precisavam. Sem os
    pacotes de personagem o dicionario volta vazio e o chamador usa
    `char_frame_bytes` (o comportamento antigo, mais generoso).
    """
    P = PROJECT
    packs_dir = packs_dir or os.path.join(P, "build", "packs")
    songs_dir = songs_dir or os.path.join(P, "assets-data", "songs")
    chars_dir = os.path.join(packs_dir, "chars")
    icons_dir = os.path.join(packs_dir, "icons")
    if not os.path.isdir(chars_dir) or not os.path.isdir(songs_dir):
        return {}

    def icone_bytes(cid):
        base = os.path.join(P, "assets-data", "characters", (cid or "face") + ".json")
        iid = "face"
        if os.path.isfile(base):
            try:
                hi = json.load(open(base, encoding="utf-8-sig")).get("healthIcon")
                if isinstance(hi, dict) and hi.get("id"):
                    iid = hi["id"]
            except (ValueError, OSError):
                pass
        caminho = os.path.join(icons_dir, iid + ".icp")
        if not os.path.isfile(caminho):
            return 0
        import struct
        with open(caminho, "rb") as f:
            cab = f.read(32)
        w, h, _q, _psm, passo = struct.unpack("<5H", cab[6:16])
        return _alinhar(passo * h * 2, VC_ALINHAMENTO)

    por_palco = {}
    for nome in sorted(os.listdir(songs_dir)):
        if not nome.endswith("-metadata.json"):
            continue
        try:
            d = json.load(open(os.path.join(songs_dir, nome), encoding="utf-8-sig"))
        except (ValueError, OSError):
            continue
        pd = d.get("playData") or {}
        chars = pd.get("characters") or {}
        palco = pd.get("stage")
        if not palco:
            continue
        pessoas = 0
        for papel in ("player", "girlfriend", "opponent"):
            cid = chars.get(papel)
            if not cid:
                continue
            pessoas += _chp4_bytes(os.path.join(chars_dir, cid + ".chp4"))
        icones = sum(icone_bytes(chars.get(papel)) for papel in ("player", "opponent"))
        atual = por_palco.get(palco, (0, ICONES_BYTES_PADRAO))
        por_palco[palco] = (max(atual[0], pessoas), max(atual[1], icones))
    return por_palco


def stage_limit_exato(palco=None, reservas=None, fixos=None,
                      icones=ICONES_BYTES_PADRAO):
    """Quanto o palco pode pedir de VRAM para a musica MAIS PESADA dele."""
    fixos = fixos_bytes() if fixos is None else fixos
    pessoas, ic = (0, icones)
    if palco and reservas and palco in reservas:
        pessoas, ic = reservas[palco]
    return VRAM_TOTAL - fixos - pessoas - ic


def char_frame_bytes(bpp):
    """Sum of one frame for each character slot, at native size."""
    # bf, gf and dad are the three the port always loads; measured native
    # frames are 435x461, 513x369 and 368x418 (see pacote_personagem.py).
    dims = [(435, 461), (513, 369), (368, 418)]
    return sum(w * h * (bpp // 8) for w, h in dims)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--bpp", type=int, default=16, choices=(16, 32))
    ap.add_argument("--quiet", action="store_true")
    args = ap.parse_args()
    bpp = args.bpp

    global SIZES_CACHE
    SIZES_CACHE = load_sizes()

    # Os framebuffers sao de 16 bits na build que roda; --bpp 32 so' serve para
    # comparar com o passado.
    fb_bytes = FB_W * FB_H * (FB_PSM_BITS // 8)
    textures = VRAM_TOTAL - FB_COUNT * fb_bytes
    fixed = (FONT_W * FONT_H * 4 + HUD_W * HUD_H * (bpp // 8)
             + NOTE_W * NOTE_H * (bpp // 8))
    chars = char_frame_bytes(bpp)
    limit = textures - fixed - chars

    if limit <= 0:
        print("FAIL: %d bpp does not even fit the framebuffers and the fixed "
              "textures (%d B over)" % (bpp, -limit))
        return 1

    rows = []
    for f in sorted(glob.glob(os.path.join(STAGES, "*.json"))):
        sid = os.path.basename(f)[:-5]
        props = stage_props(sid)
        if not props:
            rows.append((sid, None, 0, 0, None))
            continue
        sc = max_scale(sid, limit, bpp)
        rows.append((sid, sc, stage_bytes(sid, sc, bpp), len(props),
                     max(w * h for _, (w, h) in props)))
    rows.sort(key=lambda r: -(r[2] or 0))

    if not args.quiet:
        print("SCPH-90006: %d B VRAM; %d x %dx%d PSMCT%d framebuffers = %d B"
              % (VRAM_TOTAL, FB_COUNT, FB_W, FB_H, bpp, FB_COUNT * fb_bytes))
        print("texture region: %d B" % textures)
        print("  fixed textures (font + HUD + notes): %d B" % fixed)
        print("  3 characters at native size:         %d B" % chars)
        print("  left for the stage:                  %d B" % limit)
        print()
        print("%-24s %7s %12s %6s %10s  %s"
              % ("stage", "scale", "stage VRAM", "props", "biggest", "verdict"))
        print("-" * 78)
        for sid, sc, cost, nprop, big in rows:
            if sc is None:
                print("%-24s %7s %12s %6d %10s  %s"
                      % (sid, "n/a", "-", 0, "-", "no measured props"))
                continue
            v = "fits at native size" if sc >= 0.999 else \
                "reduced to %.0f%%" % (sc * 100.0)
            if cost > limit:
                v = "OVER by %d B" % (cost - limit)
            print("%-24s %7.3f %12d %6d %10d  %s"
                  % (sid, sc, cost, nprop, big, v))
        worst = max((r for r in rows if r[1] is not None),
                    key=lambda r: r[2], default=None)
        if worst:
            print()
            print("worst stage: %s needs %d B of its %d B allowance, so it is "
                  "drawn at scale %.3f." % (worst[0], worst[2], limit, worst[1]))
    return 0


SIZES_CACHE = {}

if __name__ == "__main__":
    sys.exit(main())
