#!/usr/bin/env python3
"""Gera src/engine/resultado_dados.h a partir de assets-data/resultado-plan.json.

Mesma regra dos outros geradores do projeto: o header em C e' a RENDERIZACAO do
plano, e o plano e' o que o empacotador viu nos pixels. Nenhum numero de
posicao, tamanho ou tempo e' digitado a mao.

O `--check` compara o texto gerado com o arquivo em disco, entao a suite reprova
se o plano mudar sem o header -- ou se alguem editar o header.

Uso:
    tools/gen_resultado_dados.py            # escreve o header
    tools/gen_resultado_dados.py --check    # so' confere
"""

import argparse
import json
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
PROJECT = os.path.dirname(HERE)
PLANO = os.path.join(PROJECT, "assets-data", "resultado-plan.json")
SAIDA = os.path.join(PROJECT, "src", "engine", "resultado_dados.h")


def gerar():
    d = json.load(open(PLANO, encoding="utf-8"))
    folhas = d["folhas"]
    out = []
    a = out.append
    a("/* GERADO POR tools/gen_resultado_dados.py -- NAO EDITE.")
    a(" *")
    a(" * Fonte: assets-data/resultado-plan.json, escrito por")
    a(" * tools/empacotar_resultado.py depois de recortar as folhas de")
    a(" * shared/images/resultScreen. Posicoes, tempos e fps vem de")
    a(" * source/funkin/play/ResultState.hx (v0.8.6); as linhas estao no plano.")
    a(" */")
    a("#ifndef RESULTADO_DADOS_H")
    a("#define RESULTADO_DADOS_H")
    a("")
    a("/* Escala: o original monta a tela em 1280x720, o port desenha em 640x360. */")
    a("#define RES_ESCALA (%.1ff)" % d["escala"])
    a("#define RES_FOLHAS %du" % len(folhas))
    a("")
    a("/* O fundo: FlxGradient de 90 graus entre estas duas cores. */")
    a("#define RES_BG_W %du" % d["bg_w"])
    a("#define RES_BG_H %du" % d["bg_h"])
    a("#define RES_BG_TOPO_R %du" % d["bg_topo"][0])
    a("#define RES_BG_TOPO_G %du" % d["bg_topo"][1])
    a("#define RES_BG_TOPO_B %du" % d["bg_topo"][2])
    a("#define RES_BG_BASE_R %du" % d["bg_base"][0])
    a("#define RES_BG_BASE_G %du" % d["bg_base"][1])
    a("#define RES_BG_BASE_B %du" % d["bg_base"][2])
    a("")
    a("typedef struct {")
    a("    unsigned short w, h;        /* canvas da folha em 640x360       */")
    a("    unsigned short quadros;     /* quadros da animacao              */")
    a("    unsigned short fps;")
    a("    unsigned char  looped;")
    a("    float          x, y;        /* canto superior esquerdo na tela  */")
    a("    unsigned int   inicio_ms;   /* quando a folha aparece           */")
    a("    const char    *tex;         /* caminho do pacote CFZ4           */")
    a("    const char    *id;")
    a("} res_folha_dado_t;")
    a("")
    a("static const res_folha_dado_t g_res_folha[RES_FOLHAS] = {")
    for f in folhas:
        a('    { %3du, %3du, %2du, %2du, %du, %.1ff, %.1ff, %4du, "%s", "%s" },'
          % (f["w"], f["h"], f["quadros"], f["fps"], 1 if f["looped"] else 0,
             f["x"], f["y"], f["inicio_ms"], f["tex"], f["id"]))
    a("};")
    a("")
    a("#define RES_RANKS %du" % len(d["ranks"]))
    a("")
    a("/* A ARTE DO RANK (ResultState.hx L667-700, Scoring.hx L627-666).")
    a(" * `flash_ms` e' o `getFlashDelay()` de cada rank, contado da abertura da")
    a(" * tela; `*_x`/`*_y`/`*_vel` ja' estao em 640x360 (metade do original). */")
    a("typedef struct {")
    a("    unsigned short w, h;")
    a("    const char    *arquivo;")
    a("} res_img_t;")
    a("")
    a("typedef struct {")
    a("    const char    *sufixo;      /* LOSS, GOOD, GREAT, EXCELLENT, PERFECT */")
    a("    unsigned int   flash_ms;")
    a("    res_img_t      vert, hor;")
    a("    float          vert_x, vert_y, vert_vel;")
    a("    unsigned int   vert_espera_ms;")
    a("    float          hor_x, hor_y0, hor_dy, hor_vel, hor_espaco;")
    a("    unsigned char  hor_n;")
    a("    unsigned int   flicker_ms, flicker_total_ms, flash_dura_ms;")
    a("} res_rank_t;")
    a("")
    a("static const res_rank_t g_res_rank[RES_RANKS] = {")
    for r in d["ranks"]:
        a('    { "%s", %4du,', end=None) if False else None
        a('    { "%s", %4du,                                          /* rank %d */'
          % (r["sufixo"], r["flash_ms"], r["rank"]))
        a('      { %3du, %3du, "%s" }, { %3du, %3du, "%s" },'
          % (r["vert"]["w"], r["vert"]["h"], r["vert"]["arquivo"],
             r["hor"]["w"], r["hor"]["h"], r["hor"]["arquivo"]))
        a('      %.1ff, %.1ff, %.1ff, %4du,'
          % (r["vert_x"], r["vert_y"], r["vert_vel"], r["vert_espera_ms"]))
        a('      %.1ff, %.1ff, %.2ff, %.2ff, %.1ff, %du,'
          % (r["hor_x"], r["hor_y0"], r["hor_dy"], r["hor_vel"],
             r["hor_espaco"], r["hor_n"]))
        a('      %2du, %2du, %2du },'
          % (r["flicker_ms"], r["flicker_total_ms"], r["flash_dura_ms"]))
    a("};")
    a("")
    sc = d["score"]
    a("")
    a("#define RES_SCORE_QUADROS %du" % sc["quadros"])
    a("")
    a("/* O PLACAR (ResultScore.hx): dez slots, cada um um ScoreNum que toca uma")
    a(" * das 12 animacoes de `resultScreen/score-digital-numbers` -- ZERO..NINE")
    a(" * com %d quadros cada, mais DISABLED e GONE com um. Os %d quadros moram"
      % (sc["quadros_por_digito"], sc["quadros"]))
    a(" * num UNICO atlas (os dez digitos aparecem ao mesmo tempo) e cada um tem")
    a(" * seu retangulo nele e o deslocamento com que o Sparrow o posiciona. */")
    a("typedef struct {")
    a("    unsigned char  digito, quadro;")
    a("    unsigned short ax, ay, aw, ah;  /* retangulo no atlas (metade)     */")
    a("    short          ox, oy;          /* -frameX/-frameY do XML (metade) */")
    a("} res_score_frame_t;")
    a("")
    a("static const res_score_frame_t g_res_score_frame[RES_SCORE_QUADROS] = {")
    for f in sc["frames"]:
        a("    { %2du, %du, %4du, %4du, %3du, %3du, %4d, %4d },  /* %-7s %d */"
          % (f["digito"], f["quadro"], f["ax"], f["ay"], f["aw"], f["ah"],
             f["ox"], f["oy"],
             ["ZERO", "ONE", "TWO", "THREE", "FOUR", "FIVE", "SIX", "SEVEN",
              "EIGHT", "NINE", "DISABLED", "GONE"][f["digito"]], f["quadro"]))
    a("};")
    a("")
    a("/* O atlas e a geometria da tela. `inicio_q`, `shuffle_q` e `tween_q` sao")
    a(" * os timers do original em 24 avos de segundo (o mesmo relogio das")
    a(" * animacoes), contados da abertura da tela. */")
    a("typedef struct {")
    a("    unsigned short w, h;")
    a("    const char    *arquivo;")
    a("    unsigned int   vram_bytes;")
    a("    float          x, y, dx;        /* 640x360: 17,5 / 152,5 / 32,5 */")
    a("    unsigned char  slots, quadros_por_digito;")
    a("    unsigned char  digito_disabled, digito_gone;")
    a("    unsigned char  inicio_q, shuffle_q, tween_q, fps;")
    a("} res_score_t;")
    a("")
    a("static const res_score_t g_res_score = {")
    a("    %3du, %3du, \"%s\", %6du," % (sc["w"], sc["h"], sc["atlas"], sc["vram_bytes"]))
    a("    %.1ff, %.1ff, %.1ff, %du," % (sc["x"], sc["y"], sc["dx"], sc["slots"]))
    a("    %du, %du, %du," % (sc["quadros_por_digito"], sc["digito_disabled"],
                            sc["digito_gone"]))
    a("    %2du, %2du, %2du, %2du" % (sc["inicio_q"], sc["shuffle_q"],
                                    sc["tween_q"], sc["fps"]))
    a("};")
    a("")
    tl = d["tallies"]
    a("")
    a("#define RES_TALLIES %du" % len(tl["contadores"]))
    a("#define RES_TALLY_FRAMES %du" % len(tl["frames"]))
    a("")
    a("/* Os CONTADORES DE JULGAMENTO (TallyCounter.hx L14-90 e ResultState.hx")
    a(" * L418-490): dez glifos do atlas `resultScreen/tallieNumber`, um por")
    a(" * digito, e os sete contadores -- posicao, cor e qual numero cada um")
    a(" * mostra. `campo` e' o indice do valor: 0 totalNotesHit, 1 maxCombo,")
    a(" * 2 sick, 3 good, 4 bad, 5 shit, 6 missed (a ordem do `ratingGrp`). */")
    a("typedef struct {")
    a("    unsigned char  digito;")
    a("    unsigned short ax, ay, aw, ah;")
    a("    short          ox, oy;")
    a("} res_tally_frame_t;")
    a("")
    a("static const res_tally_frame_t g_res_tally_frame[RES_TALLY_FRAMES] = {")
    for f in tl["frames"]:
        a("    { %2du, %4du, %4du, %3du, %3du, %4d, %4d },  /* digito %d */"
          % (f["digito"], f["ax"], f["ay"], f["aw"], f["ah"], f["ox"], f["oy"],
             f["digito"]))
    a("};")
    a("")
    a("typedef struct {")
    a("    unsigned short w, h;")
    a("    const char    *arquivo;")
    a("    unsigned int   vram_bytes;")
    a("    float          dx;          /* 43 px por digito, em 1280 -> 21,5 */")
    a("    unsigned int   atraso_base_ms, atraso_passo_ms, tween_ms;")
    a("    unsigned char  ajuste_1000; /* 30 px para a esquerda com 1000+ notas */")
    a("} res_tallies_t;")
    a("")
    a("static const res_tallies_t g_res_tallies = {")
    a("    %3du, %3du, \"%s\", %5du, %.1ff," % (tl["w"], tl["h"],
                                               tl["atlas"], tl["vram_bytes"],
                                               tl["dx"]))
    a("    %4du, %3du, %3du, %2du" % (tl["atraso_base_ms"],
                                     tl["atraso_passo_ms"], tl["tween_ms"],
                                     tl["ajuste_1000"]))
    a("};")
    a("")
    a("typedef struct {")
    a("    unsigned char  campo;       /* qual numero do jogo (ver acima) */")
    a("    float          x, y;")
    a("    unsigned char  r, g, b;     /* a cor do `flavour` do TallyCounter */")
    a("} res_tally_t;")
    a("")
    a("static const res_tally_t g_res_tally[RES_TALLIES] = {")
    for c in tl["contadores"]:
        a("    { %du, %6.1ff, %6.1ff, 0x%02X, 0x%02X, 0x%02X },  /* %s */"
          % (["total_hit", "max_combo", "sick", "good", "bad", "shit",
              "missed"].index(c["campo"]), c["x"], c["y"], c["r"], c["g"],
             c["b"], c["campo"]))
    a("};")
    a("")
    cp = d["clear_percent"]
    g = cp["geometria"]
    a("")
    a("#define RES_CP_ATLASES 3u")
    a("")
    a("/* O PERCENTUAL DE CONCLUSAO (ClearPercentCounter.hx L22-140 e")
    a(" * ResultState.hx L577-646). Tres atlas de digitos -- `Left` e `Right` sao")
    a(" * as duas inclinacoes do glifo grande, `Small` e' o do `%` da barra do")
    a(" * topo -- mais o texto \"CLEAR\" (com o `%` embaixo) e o `%` pequeno.")
    a(" * `fator` converte o tamanho guardado no tamanho DESENHADO (o original pela")
    a(" * metade): os glifos grandes ficaram a 0,4 do original para caber na VRAM,")
    a(" * e sao desenhados a 1,25x. */")
    a("typedef struct {")
    a("    unsigned char  digito;")
    a("    unsigned short ax, ay, aw, ah;")
    a("    short          ox, oy;")
    a("} res_cp_frame_t;")
    a("")
    for ident in ("left", "right", "small"):
        a("static const res_cp_frame_t g_res_cp_%s[10] = {" % ident)
        for f in cp["atlas"][ident]["frames"]:
            a("    { %du, %4du, %4du, %3du, %3du, %4d, %4d },   /* digito %d */"
              % (f["digito"], f["ax"], f["ay"], f["aw"], f["ah"], f["ox"],
                 f["oy"], f["digito"]))
        a("};")
        a("")
    a("typedef struct {")
    a("    unsigned short        w, h;")
    a("    const char           *arquivo;")
    a("    unsigned int          vram_bytes;")
    a("    float                 fator;")
    a("    const res_cp_frame_t *frames;")
    a("} res_cp_atlas_t;")
    a("")
    a("static const res_cp_atlas_t g_res_cp_atlas[RES_CP_ATLASES] = {")
    for i, ident in enumerate(("left", "right", "small")):
        at = cp["atlas"][ident]
        a('    { %3du, %3du, "%s", %6du, %.2ff, g_res_cp_%s },   /* %s */'
          % (at["w"], at["h"], at["arquivo"], at["vram_bytes"], at["fator"],
             ident, ident.upper()))
    a("};")
    a("")
    a("typedef struct {")
    a("    unsigned short w, h;")
    a("    const char    *arquivo;")
    a("    unsigned int   vram_bytes;")
    a("    float          fator;")
    a("} res_cp_img_t;")
    a("")
    for ident, rotulo in (("texto", "o \"CLEAR\" com o `%`"), ("texto_small", "o `%` pequeno")):
        t = cp["texto"][ident]
        a('static const res_cp_img_t g_res_cp_%s = { %3du, %3du, "%s", %5du, %.2ff };  /* %s */'
          % (ident, t["w"], t["h"], t["arquivo"], t["vram_bytes"], t["fator"],
             rotulo))
    a("")
    a("typedef struct {")
    a("    float          x, y;             /* canto do grupo, em 640x360      */")
    a("    float          passo_grande, passo_pequeno;")
    a("    float          y_digitos_grande;")
    a("    float          small_x, small_y, small_x_pos, small_y_passo;")
    a("    unsigned short inicio_q, tween_q;")
    a("    unsigned int   flash_ms, fade_espera_ms, fade_atraso_ms, fade_ms;")
    a("    unsigned char  abaixo;           /* `clearPercentLerp` = alvo - 36 */")
    a("} res_cp_geo_t;")
    a("")
    a("static const res_cp_geo_t g_res_cp_geo = {")
    a("    %.1ff, %.1ff, %.1ff, %.1ff, %.1ff," % (g["x"], g["y"],
                                                g["passo_grande"],
                                                g["passo_pequeno"],
                                                g["y_digitos_grande"]))
    a("    %.1ff, %.1ff, %.1ff, %.1ff," % (g["small_x"], g["small_y"],
                                          g["small_x_pos"], g["small_y_passo"]))
    a("    %2du, %2du," % (g["inicio_q"], g["tween_q"]))
    a("    %3du, %3du, %3du, %3du, %2du" % (g["flash_ms"], g["fade_espera_ms"],
                                           g["fade_atraso_ms"], g["fade_ms"],
                                           g["abaixo"]))
    a("};")
    a("")
    tb = d["topbar"]
    b = tb["barra"]
    dg = tb["dificuldade"]
    a("")
    a("#define RES_DIFS %du" % len(tb["dificuldades"]))
    a("")
    a("/* A BARRA PRETA DO TOPO. Nao ha' PNG: o original a monta em codigo")
    a(" * (`BitmapUtil.createResultsBar`) -- um retangulo preto de %dx%d girado de"
      " %.1f graus. A forma que sobra na tela e' \"tudo acima da reta de"
      " baixo\":" % (b["largura_src"], b["altura_src"], b["angulo"]))
    a(" * `borda_y0` em x = 0 e `borda_y1` em x = 640 (em 640x360). O jogo desenha")
    a(" * faixas pretas, sem textura. A barra entra deslizando de cima: tween de")
    a(" * `-altura` ate' 0 em %d/24 s, quartOut, com %d/24 s de atraso."
      % (b["tween_q"], b["atraso_q"]))
    a(" * `correcao_y` e' `(blackTopBar.height - 148)`, o deslocamento que a fonte")
    a(" * aplica em tudo que fica abaixo da barra. */")
    a("typedef struct {")
    a("    float          borda_y0, borda_y1, altura, correcao_y;")
    a("    unsigned char  atraso_q, tween_q;")
    a("} res_barra_t;")
    a("")
    a("static const res_barra_t g_res_barra = {")
    a("    %.1ff, %.1ff, %.1ff, %.1ff, %du, %du"
      % (b["borda_y0"], b["borda_y1"], b["altura_half"], b["correcao_y"],
         b["atraso_q"], b["tween_q"]))
    a("};")
    a("")
    a("/* A IMAGEM DA DIFICULDADE (`resultScreen/diff_<id>`): x = 555 na fonte,")
    a(" * entrando de cima em 0,5 s com `expoOut` e 0,8 s de atraso. */")
    a("typedef struct {")
    a("    unsigned short w, h;")
    a("    const char    *arquivo;")
    a("    unsigned int   vram_bytes;")
    a("} res_dif_t;")
    a("")
    a("static const res_dif_t g_res_dif[RES_DIFS] = {")
    for nome in ("easy", "normal", "hard", "erect", "nightmare"):
        dd = tb["dificuldades"][nome]
        a('    { %3du, %2du, "%s", %5du },   /* diff_%s */'
          % (dd["w"], dd["h"], dd["arquivo"], dd["vram_bytes"], nome))
    a("};")
    a("")
    a("typedef struct {")
    a("    float        x, y_alvo;")
    a("    unsigned int atraso_ms, tween_ms;")
    a("} res_dif_geo_t;")
    a("")
    a("static const res_dif_geo_t g_res_dif_geo = { %.1ff, %.1ff, %3du, %3du };"
      % (dg["x"], dg["y_alvo"], dg["atraso_ms"], dg["tween_ms"]))
    a("")
    a("#endif /* RESULTADO_DADOS_H */")
    return "\n".join(out) + "\n"


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--check", action="store_true")
    args = ap.parse_args()
    texto = gerar()
    if args.check:
        if not os.path.isfile(SAIDA):
            print("  FALHA: %s nao existe -- rode tools/gen_resultado_dados.py"
                  % os.path.relpath(SAIDA, PROJECT))
            return 1
        if open(SAIDA, encoding="utf-8").read() != texto:
            print("  FALHA: src/engine/resultado_dados.h nao e' a renderizacao do "
                  "plano (rode tools/gen_resultado_dados.py)")
            return 1
        print("  ok:   resultado_dados.h confere com assets-data/resultado-plan.json")
        return 0
    with open(SAIDA, "w", encoding="utf-8") as f:
        f.write(texto)
    print("escrito %s" % os.path.relpath(SAIDA, PROJECT))
    return 0


if __name__ == "__main__":
    sys.exit(main())
