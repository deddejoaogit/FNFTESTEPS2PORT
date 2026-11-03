#!/usr/bin/env python3
"""Gera `src/stage_criados_dados.h`: os sprites que o SCRIPT do palco cria.

POR QUE ESTE ARQUIVO EXISTE
---------------------------
Os 18 scripts de palco (`preload/scripts/stages/*.hxc`) criam sprites que NAO
estao no JSON do palco -- `new FlxTiledSprite(...)`, `new FlxBackdrop(...)` -- e
o port, que le' o JSON, nunca os desenhou. Medido em `blazin`: o original
desenha **2** camadas no quadro 0 (`phillyForegroundCity` + o `scrollingSky`) e
o port desenhava **1**. O `scrollingSky` e' o FUNDO da fase: 4000x495 de ceu
repetido, rolando a 35 px/s.

O dado mora em `assets-data/stage-criados.json`, escrito a mao a partir da
fonte, com a linha de cada script em `fonte`. Este gerador NAO interpreta
HScript: ele converte um JSON conferido em tabela C, e `--check` falha se a
tabela commitada nao casar com o JSON (mesma regra de `gen_stage_dados.py` e
`gen_song_registry.py`).

AS DUAS ESCALAS, que ja' custaram caro no resto do port:
  src_w/src_h  tamanho NATIVO da imagem (o periodo da repeticao no mundo);
  sw/sh        resolucao do PACOTE = nativo x escala do palco (`stage-props.json`),
               que e' a resolucao que o GS precisa para o bloco sair no tamanho
               certo em 640x360.

Uso:
  python3 tools/gen_stage_criados.py            # escreve src/stage_criados_dados.h
  python3 tools/gen_stage_criados.py --check    # so' compara
"""
import argparse
import glob
import json
import math
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
PROJECT = os.path.dirname(HERE)
DATA = os.path.join(PROJECT, "assets-data")
OUT = os.path.join(PROJECT, "src", "stage_criados_dados.h")
FONTE = os.path.join(DATA, "stage-criados.json")
PLANO = os.path.join(DATA, "stage-props.json")

# openfl.display.BlendMode -> o enum do port (o mesmo que o StageData usa:
# 0 normal, 1 ADD, 2 MULTIPLY, 3 SCREEN). O GS faz NORMAL e ADD; os outros dois
# sao desenhados normal e CONTADOS no log (`palco_blend`), como nos props.
BLEND_PORT = {0: 1, 9: 2, 12: 3, 10: 0}

ALINHAMENTO = 256   # VC_ALINHAMENTO do vram_cache.c
TBW = 64            # largura do buffer de textura do GS, em pixels   # o mesmo do TBP0 (VC_ALINHAMENTO)


def ordem_dos_palcos():
    """Os palcos na ordem de `bs_stage_props[]` (a mesma de `gen_stage_dados`)."""
    arquivos = sorted(glob.glob(os.path.join(DATA, "stages", "*.json")))
    return [os.path.basename(a)[:-5] for a in arquivos]


def escalas():
    """{palco: escala} do plano de empacotamento (o que o GS recebe)."""
    if not os.path.isfile(PLANO):
        return {}, "sem %s: rode tools/pacotar_palcos.py" % os.path.relpath(PLANO, PROJECT)
    plano = json.load(open(PLANO, encoding="utf-8"))
    return {s["id"]: float(s["scale"]) for s in plano.get("stages", [])}, None


def vram_bytes(sw, sh, bpp=16):
    """Bytes que a textura OCUPA no GS -- a mesma conta de gen_stage_vram.py.

    A largura do buffer de textura (TBW do BITBLTBUF) anda de 64 em 64 pixels,
    e o upload do jogo manda `t->width = (sw+63)&~63`; reservar `sw*sh*2` puro
    deixaria a textura do sprite do script escrever por cima do proximo bloco
    do MESMO pool (o `sp_criado` reserva exatamente este numero). E' o erro
    que os props ja' tiveram: 82 dos 157 props passavam por cima do seguinte.
    O total sobe para 256 B, o alinhamento do TBP0 (VC_ALINHAMENTO).
    """
    w = (int(sw) + TBW - 1) // TBW * TBW
    b = w * int(sh) * (bpp // 8)
    return (b + ALINHAMENTO - 1) & ~(ALINHAMENTO - 1)


def monta():
    """-> (linhas C, contagens, problemas)."""
    dados = json.load(open(FONTE, encoding="utf-8"))
    palcos = ordem_dos_palcos()
    esc, erro = escalas()
    problemas = []
    if erro:
        problemas.append(erro)

    entradas = []
    chaos = []
    for sid in palcos:
        for c in dados.get("palcos", {}).get(sid, []):
            e = dict(c)
            e["stage"] = sid
            f = esc.get(sid)
            if f is None:
                problemas.append("%s/%s: o palco nao esta' no plano (sem escala)"
                                 % (sid, e["nome"]))
                continue
            # O pacote sai na escala do palco: e' a resolucao que o bloco ocupa
            # na tela. Arredondar para CIMA em 1 px evitaria a borda de meia
            # coluna, mas mudaria o periodo da repeticao -- e' round, medida.
            e["sw"] = max(1, int(round(e["src_w"] * f)))
            e["sh"] = max(1, int(round(e["src_h"] * f)))
            e["scale"] = f
            # O PISO EM PERSPECTIVA vai para a tabela propria (`bs_chaos[]`):
            # ele nao e' um quad reto, tem matriz afim por quadro. O resto dos
            # campos e' o mesmo (imagem, escala, z).
            if (e.get("tipo") or "quad") == "chao":
                if not e.get("asset_cutscene"):
                    problemas.append("%s/%s: chao sem `asset_cutscene`" %
                                     (sid, e["nome"]))
                if len(e.get("bottom") or []) != 2 or len(e.get("top") or []) != 2:
                    problemas.append("%s/%s: chao sem `bottom`/`top`" %
                                     (sid, e["nome"]))
                if e["sw"] > 1024 or e["sh"] > 1024:
                    problemas.append("%s/%s: %dx%d passa do limite de 1024 px"
                                     % (sid, e["nome"], e["sw"], e["sh"]))
                chaos.append(e)
                continue
            # O periodo da repeticao do FlxBackdrop e' o tamanho do quadro
            # VEZES o `scale` do sprite (dust1 1,5 -> 3072 px do original).
            e["tile_periodo"] = float(e["src_w"]) * float(e.get("escala") or 1.0)
            if e["sw"] > 1024 or e["sh"] > 1024:
                problemas.append("%s/%s: %dx%d passa do limite de 1024 px do GS"
                                 % (sid, e["nome"], e["sw"], e["sh"]))
            if (e.get("tipo") != "backdrop") and e["tile"] and e["w"] < e["src_w"]:
                problemas.append("%s/%s: o quad (%g) e' menor que o tile (%g)"
                                 % (sid, e["nome"], e["w"], e["src_w"]))
            entradas.append(e)

    # Os ids precisam ser unicos: o runtime indexa bs_criados[] pelo primeiro
    # indice da faixa do palco.
    vistos = {}
    for e in entradas + chaos:
        k = (e["stage"], e["nome"])
        if k in vistos:
            problemas.append("%s/%s aparece duas vezes" % k)
        vistos[k] = 1

    w = []
    a = w.append
    a("/* GERADO POR tools/gen_stage_criados.py - NAO EDITAR A MAO.")
    a("   Fonte: assets-data/stage-criados.json (com a linha de cada script) e a")
    a("   escala do palco em assets-data/stage-props.json.")
    a("")
    a("   Sao os sprites que o SCRIPT do palco cria e que o JSON do palco nao")
    a("   tem: `new FlxTiledSprite`/`new FlxBackdrop`. O port nao interpreta")
    a("   HScript -- esta tabela e' a transcricao conferida das linhas citadas.")
    a("")
    a("   src_w/src_h = tamanho nativo da imagem (o periodo da repeticao, no")
    a("   espaco do original). sw/sh = resolucao do pacote (nativo x escala do")
    a("   palco), que e' o que o GS precisa para o bloco sair no tamanho certo.")
    a("   vel_x/vel_y = px/s do `scroll`, no espaco do original. */")
    a("#ifndef STAGE_CRIADOS_DADOS_H")
    a("#define STAGE_CRIADOS_DADOS_H")
    a("")
    a("typedef struct {")
    a("    const char *stage;      /* palco dono (conferido no teste)        */")
    a("    const char *name;       /* variavel do script                     */")
    a("    const char *tex;        /* stages/<palco>/<nome>.cfz4             */")
    a("    float x, y;             /* canto do quad, espaco do original      */")
    a("    float w, h;             /* tamanho do quad (`width`/`height`)     */")
    a("    float src_w, src_h;     /* tamanho nativo = periodo da repeticao  */")
    a("    int sw, sh;             /* resolucao do pacote                    */")
    a("    float scrollx, scrolly; /* scrollFactor (1 = colado na camera)    */")
    a("    int z;                  /* zIndex                                 */")
    a("    float alpha;")
    a("    int blend;              /* 0 normal 1 ADD 2 MULTIPLY 3 SCREEN     */")
    a("    float vel_x, vel_y;     /* px/s do scroll, espaco do original     */")
    a("    int tile;               /* periodo da repeticao, px do original   */")
    a("    unsigned char rgb[3];   /* o `color` do sprite (o GS multiplica)  */")
    a("    int pintado;            /* 1 = a cor acima veio do script         */")
    a("    int backdrop;           /* 1 = FlxBackdrop (emendas na borda)     */")
    a("} bs_criado_t;")
    a("")
    a("/* O PISO EM PERSPECTIVA (`PerspectiveSprite` = FlxSkewedSprite com")
    a("   `transformMatrix`). Nao e' um quad reto: os dois pontos de baixo e de")
    a("   cima sao corrigidos pelo scroll da camera COM scrollFactors proprios e")
    a("   viram uma matriz afim por quadro (PerspectiveSprite.hxc L97-125):")
    a("")
    a("     distX = cima.x - baixo.x")
    a("     distY = cima.y - (baixo.y - H)")
    a("     a=1  b=0  c=-distX/H  d=1-distY/H  tx=distX/2  ty=distY/2")
    a("     sprite.x = baixo.x - W/2   sprite.y = baixo.y - H")
    a("")
    a("   `x/y` aqui sao os do sprite com a camera em (0,0) -- existem para o")
    a("   teste conferir a conta contra a formula da fonte. */")
    a("typedef struct {")
    a("    const char *stage;")
    a("    const char *name;")
    a("    const char *tex;        /* durante a musica                     */")
    a("    const char *tex_cut;    /* o `loadGraphic(floor-cutscene)`       */")
    a("    float bottom_x, bottom_y, top_x, top_y;   /* `setPositions`     */")
    a("    float bsf_x, bsf_y, tsf_x, tsf_y;         /* `setScrollFactors` */")
    a("    float src_w, src_h;     /* H e W da formula (tamanho nativo)     */")
    a("    int sw, sh;             /* resolucao do pacote                   */")
    a("    int z;")
    a("} bs_chao_t;")
    a("")
    if entradas:
        a("static const bs_criado_t bs_criados[] = {")
        for e in entradas:
            asset = (e.get("asset") or e["nome"])
            pacote = asset.rsplit("/", 1)[-1]
            a("  {\"%s\",\"%s\",\"stages/%s/%s.cfz4\"," %
              (e["stage"], e["nome"], e["stage"], pacote))
            a("   %.6ff,%.6ff,%.6ff,%.6ff," %
              (e["x"], e["y"], e["w"], e["h"]))
            a("   %.6ff,%.6ff,%d,%d," %
              (e["src_w"], e["src_h"], e["sw"], e["sh"]))
            a("   %.6ff,%.6ff,%d,%.6ff,%d," %
              (e["scrollx"], e["scrolly"], e["z"], e["alpha"],
               BLEND_PORT.get(int(e["blend"]), 0)))
            cor = e.get("tinta")
            rgb = (0x80, 0x80, 0x80)
            pintado = 0
            if cor:
                v = int(str(cor).replace("0x", ""), 16)
                rgb = ((v >> 16) & 0xFF, (v >> 8) & 0xFF, v & 0xFF)
                # 0x80 = 1,0 na cor do vertice do GS; a conversao e' a mesma
                # dos personagens e dos props pintados pelo script.
                rgb = tuple((c * 0x80 + 127) // 255 for c in rgb)
                pintado = 1
            a("   %.6ff,%.6ff,%d,{%d,%d,%d},%d,%d}," %
              (e["vel_x"], e["vel_y"], int(e.get("tile_periodo") or 0),
               rgb[0], rgb[1], rgb[2], pintado,
               1 if (e.get("tipo") == "backdrop") else 0))
        a("};")
    else:
        a("/* Nenhum sprite de script transcrito ainda. */")
        a("static const bs_criado_t bs_criados[1] = {{0}};")
    a("#define BS_CRIADO_COUNT %d" % len(entradas))
    a("")
    a("/* VRAM de UM quadro (o pacote tem um), alinhada ao TBP0. */")
    a("static const unsigned int bs_criado_vram_bytes[] = {")
    if entradas:
        linha = "  "
        for e in entradas:
            item = "%du, " % vram_bytes(e["sw"], e["sh"])
            if len(linha) + len(item) > 78:
                a(linha.rstrip())
                linha = "  "
            linha += item
        a(linha.rstrip())
    else:
        a("  0u,")
    a("};")
    a("")
    a("/* Faixa de cada palco em bs_criados[], na MESMA ordem de")
    a("   bs_stage_props[] (alfabetica, a de gen_stage_dados.py). */")
    a("static const int bs_stage_criado_first[] = {")
    linhas = []
    por_palco = {}
    for i, e in enumerate(entradas):
        por_palco.setdefault(e["stage"], []).append(i)
    atual = 0
    for sid in palcos:
        lista = por_palco.get(sid, [])
        linhas.append("%d" % (lista[0] if lista else atual))
        if lista:
            atual = lista[-1] + 1
    a("  " + ", ".join(linhas) + ",")
    a("};")
    a("static const int bs_stage_criado_count[] = {")
    a("  " + ", ".join("%d" % len(por_palco.get(sid, [])) for sid in palcos) + ",")
    a("};")
    a("")
    # ---------------- o piso em perspectiva ----------------
    if chaos:
        a("static const bs_chao_t bs_chaos[] = {")
        for e in chaos:
            asset = (e.get("asset") or e["nome"]).rsplit("/", 1)[-1]
            cut = (e.get("asset_cutscene") or "").rsplit("/", 1)[-1]
            a("  {\"%s\",\"%s\",\"stages/%s/%s.cfz4\",\"stages/%s/%s.cfz4\","
              % (e["stage"], e["nome"], e["stage"], asset, e["stage"], cut))
            a("   %.6ff,%.6ff,%.6ff,%.6ff," %
              (e["bottom"][0], e["bottom"][1], e["top"][0], e["top"][1]))
            a("   %.6ff,%.6ff,%.6ff,%.6ff," %
              (e["bottom_sf"][0], e["bottom_sf"][1],
               e["top_sf"][0], e["top_sf"][1]))
            a("   %.6ff,%.6ff,%d,%d,%d}," %
              (e["src_w"], e["src_h"], e["sw"], e["sh"], int(e["z"])))
        a("};")
    else:
        a("/* Nenhum piso em perspectiva transcrito ainda. */")
        a("static const bs_chao_t bs_chaos[1] = {{0}};")
    a("#define BS_CHAO_COUNT %d" % len(chaos))
    a("")
    a("/* VRAM de UM quadro de cada piso (o pacote tem um), alinhada ao TBP0. */")
    a("static const unsigned int bs_chao_vram_bytes[] = {")
    a("  " + ", ".join("%du" % vram_bytes(e["sw"], e["sh"]) for e in chaos) + ("," if chaos else "0u,"))
    a("};")
    a("")
    a("/* Faixa de cada palco em bs_chaos[], na MESMA ordem de bs_stage_props[]. */")
    por_chao = {}
    for i, e in enumerate(chaos):
        por_chao.setdefault(e["stage"], []).append(i)
    primeiro, atual2 = [], 0
    for sid in palcos:
        lista = por_chao.get(sid, [])
        primeiro.append("%d" % (lista[0] if lista else atual2))
        if lista:
            atual2 = lista[-1] + 1
    a("static const int bs_stage_chao_first[] = {")
    a("  " + ", ".join(primeiro) + ",")
    a("};")
    a("static const int bs_stage_chao_count[] = {")
    a("  " + ", ".join("%d" % len(por_chao.get(sid, [])) for sid in palcos) + ",")
    a("};")
    a("")
    a("#endif /* STAGE_CRIADOS_DADOS_H */")
    return "\n".join(w) + "\n", entradas, problemas, chaos


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--check", action="store_true")
    args = ap.parse_args()
    texto, entradas, problemas, chaos = monta()
    for p in problemas:
        print("  " + p)
    if args.check:
        atual = open(OUT, encoding="utf-8").read() if os.path.isfile(OUT) else ""
        if atual != texto:
            print("%s nao casa com assets-data/stage-criados.json" %
                  os.path.relpath(OUT, PROJECT))
            return 1
        print("stage_criados_dados.h matches assets-data/ (%d sprite(s) de "
              "script + %d piso(s) em perspectiva)" % (len(entradas), len(chaos)))
        return 0
    with open(OUT, "w", encoding="utf-8") as f:
        f.write(texto)
    print("wrote %s: %d sprite(s) + %d piso(s) em %d palco(s), %d B de VRAM" %
          (os.path.relpath(OUT, PROJECT), len(entradas), len(chaos),
           len({e["stage"] for e in entradas + chaos}),
           sum(vram_bytes(e["sw"], e["sh"]) for e in entradas + chaos)))
    return 1 if problemas else 0


if __name__ == "__main__":
    sys.exit(main())
