#!/usr/bin/env python3
"""Empacota a ANIMACAO DO PERSONAGEM NA TELA DE RESULTADO (`results[rank]`).

O QUE O ORIGINAL FAZ
--------------------
`ResultState.hx` L201-330 le' `players/<dono>.json` -> `results` e, para o rank
da partida (`PlayableCharacter.getResultsAnimationDatas`, L102-126), monta uma
lista de sprites Animate/Sparrow:

    perfectGold -> results.perfectGold      excellent -> results.excellent
    perfect     -> results.perfect          great     -> results.great
    good        -> results.good             SHIT      -> results.loss

Cada entrada tem `offsets` (em 1280x720), `zIndex` (499..501), `scale`,
`delay` (segundos), e a forma de LOOP:
    `loopFrameLabel` -> ao terminar, toca o rotulo de novo, em loop;
    `loopFrame`      -> ao terminar, recomeca no QUADRO n do mesmo anim;
    sem nenhum dos dois, e `looped = false` -> pausa no ultimo quadro.
`startFrameLabel` diz em que rotulo a animacao comeca (so' o `bed` usa).

`animData.filter` e' filtrado por `Preferences.naughtyness` (L215-219):
    if (Preferences.naughtyness && filter != 'naughty' || !naughtyness && filter != 'safe') continue;
O port usa o padrao do jogo (`save/Save.hx` L101: `naughtyness: true`), logo
entram as entradas `naughty` e `both` e as `safe` NAO sao empacotadas --
empacotar arte que nada seleciona gasta pendrive e VRAM para nada.

O `scriptClass` (BFBedPerfectResults, BFShitResults) nao traz `assetPath`: ele
esta' no script do original (`preload/scripts/players/results/bf/*.hxc`,
`loadTextureAtlas("resultScreen/results-bf/resultsPERFECT/bed", "shared")`).
Este empacotador LE' o script em vez de adivinhar o caminho.

QUANDO TUDO COMECA
------------------
`afterRankTallySequence()` (L704) e' chamado no timer do `getBFDelay()`
(`Scoring.hx`, 95/24 s; 97/24 no EXCELLENT) e cada entrada soma o seu `delay`.
O plano leva `inicio_ms` = bf_ms + delay*1000 e o runtime nao recalcula nada.

Uso:
    PYTHONPATH=/tmp/pylibs:tools python3 tools/empacotar_resultado_rank.py \\
        --assets /tmp/fassets [--escala 0.5 --bpp 16] [--so perfect]
"""

import argparse
import json
import os
import re
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
PROJECT = os.path.dirname(HERE)
sys.path.insert(0, HERE)

PLANO = os.path.join(PROJECT, "assets-data", "resultado-rank-plan.json")
HEADER = os.path.join(PROJECT, "src", "engine", "resultado_rank_dados.h")
CHARS = os.path.join(PROJECT, "build", "resultado-rank")
PACKS = os.path.join(PROJECT, "build", "packs", "resultado")

# `PlayableCharacter.getResultsAnimationDatas` L102-126: chave do JSON do
# personagem por rank do jogo (`Scoring.hx`).
RANKS = [("PERFECT_GOLD", "perfectGold"),
         ("PERFECT", "perfect"),
         ("EXCELLENT", "excellent"),
         ("GREAT", "great"),
         ("GOOD", "good"),
         ("SHIT", "loss")]

# `save/Save.hx` L101: naughtyness = true.
PREF_NAUGHTY = True

# Espaco livre na tela de Resultado, MEDIDO no harness depois do atlas do
# placar ir para 16 bpp (`resultado_vram,350208,186224`). O empacotador recusa
# um rank que nao caiba: orcamento medido, nao estimado.
VRAM_LIVRE = 350208

# `getBFDelay()` por rank, em ms (Scoring.hx L530-604): 95/24, e 97/24 no
# EXCELLENT. Estes numeros ja' estao em `src/engine/resultado_dados.h`; repetir
# aqui so' para o plano sair completo e conferivel.
BF_MS = {"SHIT": 3958, "GOOD": 3958, "GREAT": 3958, "EXCELLENT": 4042,
         "PERFECT": 3958, "PERFECT_GOLD": 3958}

# `getHighscoreDelay()` por rank, em ms (`Scoring.hx` L587-604): 140/24 no
# PERFECT_GOLD/PERFECT/EXCELLENT, 129/24 no GREAT, 127/24 no GOOD e 207/24 no
# SHIT. Em ms inteiros, truncando -- 140/24 = 5833,33 -> 5833, o mesmo criterio
# do `getFlashDelay` (107/24 = 4458,33 -> 4458 em `resultado_dados.h`).
#
# O TIMER e' do jogo; o que nao existe e' o DADO que ele testa
# (`params.isNewHighscore`, que vem de `Save.instance.isNewHighscore(...)`) e o
# port nao tem save: o desenho do selo NAO acontece (e' o `else` da fonte, L424)
# e o caso vai para o log como dado ausente.
HS_MS = {"SHIT": 8625, "GOOD": 5292, "GREAT": 5375, "EXCELLENT": 5833,
         "PERFECT": 5833, "PERFECT_GOLD": 5833}


def ler_player(assets, dono):
    caminho = os.path.join(assets, "preload", "data", "players", dono + ".json")
    if not os.path.isfile(caminho):
        return None, ["falta %s" % caminho]
    with open(caminho, encoding="utf-8-sig") as f:
        doc = json.load(f)
    r = doc.get("results")
    if not r:
        return None, ["%s nao tem `results`" % dono]
    return r, []


def asset_do_script(assets, dono, classe):
    """O `assetPath` de uma entrada que so' declara `scriptClass`.

    Le' o script do original e devolve o caminho do `loadTextureAtlas`, que e'
    o dado de verdade. Sem script, devolve None -- e' dado AUSENTE, nao um
    caminho escolhido por mim.
    """
    base = os.path.join(assets, "preload", "scripts", "players", "results", dono)
    for ext in (".hxc", ".hx"):
        caminho = os.path.join(base, classe + ext)
        if not os.path.isfile(caminho):
            continue
        txt = open(caminho, encoding="utf-8", errors="replace").read()
        m = re.search(r'loadTextureAtlas\(\s*"([^"]+)"', txt)
        if m:
            return m.group(1).strip(), "script %s" % os.path.basename(caminho)
    return None, None


def rotulos_do_atlas(assets, asset):
    """Os rotulos da timeline do atlas Animate (nomes das animacoes)."""
    import achatar_animate as aa
    at = aa.Atlas(assets, asset)
    return list(at.animations().keys()), at


def rotulos_do_sparrow(assets, asset):
    """O prefixo comum dos quadros de um Sparrow (PNG+XML): 1 animacao."""
    import pacote_personagem as pp
    quadros, erro = pp.atlas_nomeado(assets, asset)
    if not quadros:
        return [], erro
    nomes = [q[0] for q in quadros]
    # O prefixo e' o nome sem os digitos finais ("Girlfriend Good Anim0007").
    m = re.match(r"^(.*?)\d*$", nomes[0])
    pre = (m.group(1) if m else nomes[0]).rstrip()
    if not pre:
        return [], "sem prefixo"
    return [pre], None


def escreve_char_json(asset, anims, nome):
    doc = {
        "version": "1.0.0",
        "name": nome,
        "renderType": "animateatlas",
        "assetPath": asset,
        "animations": [{"name": a, "prefix": a, "looped": True} for a in anims],
    }
    os.makedirs(CHARS, exist_ok=True)
    caminho = os.path.join(CHARS, nome + ".json")
    with open(caminho, "w", encoding="utf-8") as f:
        json.dump(doc, f, indent=1, ensure_ascii=False)
        f.write("\n")
    return doc, os.path.basename(caminho)[:-5]


def empacota(assets, asset, anims, slug, escala, bpp):
    """Roda o `achatar_animate --pack` e devolve o que ele mediu."""
    _doc, char = escreve_char_json(asset, anims, slug)
    os.makedirs(PACKS, exist_ok=True)
    destino = os.path.join(PACKS, "rank_%s.cfz4" % slug)
    cmd = [sys.executable, os.path.join(HERE, "achatar_animate.py"),
           "--assets", assets, "--character", char, "--char-dir", CHARS,
           "--pack", destino, "--scale", str(escala), "--bpp", str(bpp)]
    r = subprocess.run(cmd, capture_output=True, text=True)
    if r.returncode != 0:
        return None, ["%s: o achatar_animate falhou:\n%s" % (slug, r.stderr[-2000:])]
    out = r.stdout
    m = re.search(r"out canvas: (\d+)x(\d+) at scale", out)
    o = re.search(r"origem do canvas: (-?[\d.]+),(-?[\d.]+)", out)
    if not m or not o:
        return None, ["%s: nao achei o canvas na saida do achatar" % slug]
    w, h = int(m.group(1)), int(m.group(2))
    origem = (float(o.group(1)), float(o.group(2)))
    partes = re.findall(r"^\s+(\S+)\s+(\d+) B\s+frames (\d+)\.\.(\d+)$", out, re.M)
    if not partes:
        return None, ["%s: nenhuma parte escrita" % slug]
    for p in partes:
        with open(os.path.join(PACKS, p[0]), "rb") as fb:
            if fb.read(4) != b"CFZ4":
                return None, ["%s: %s nao tem magic CFZ4" % (slug, p[0])]
    import chp4
    # O `--pack` escreve as PARTES em CFZ4 (o formato que o console le') e um
    # CHP4 ao lado, com a mesma coisa mais a tabela de animacoes. Nomes
    # distintos de proposito -- ver o comentario em empacotar_freeplay_dj.py.
    caminho_chp4 = os.path.splitext(destino)[0] + ".chp4"
    dados = open(caminho_chp4, "rb").read()
    cab = chp4.ler(dados)
    chp4.conferir(cab)
    return {
        "arquivo": os.path.basename(destino),
        "partes": [{"arquivo": p[0], "bytes": int(p[1])} for p in partes],
        "w": w, "h": h, "psm": bpp,
        "quadros": cab["n_quadros"],
        "vram_bytes": w * h * (bpp // 8),
        "origem": origem,
        "anims": [{"nome": a["nome"].strip(), "primeiro": a["primeiro"],
                   "quantos": a["quantos"], "fps100": a["fps100"]}
                  for a in cab["anims"]],
        "bytes": len(dados),
    }, []


def quadro_do_label(pack, label):
    for a in pack["anims"]:
        if a["nome"] == label:
            return a["primeiro"]
    return -1


def empacotar(assets, so=None, escala=0.5, bpp=16, dono="bf"):
    res, problemas = ler_player(assets, dono)
    if res is None:
        return None, problemas
    cache = {}
    plano = {"dono": dono, "escala": escala, "bpp": bpp, "ranks": [],
             "linha": "ResultState.hx L201-330/L704-740; Scoring.hx L530-604",
             "preferencia": "naughty" if PREF_NAUGHTY else "safe"}
    for rank, chave in RANKS:
        if so and so not in (rank, chave):
            continue
        entradas = res.get(chave) or []
        saida, usados, descartados = [], 0, 0
        for i, e in enumerate(entradas):
            filtro = e.get("filter", "both")
            if filtro != "both":
                if PREF_NAUGHTY and filtro != "naughty":
                    descartados += 1
                    continue
                if not PREF_NAUGHTY and filtro != "safe":
                    descartados += 1
                    continue
            asset, de_onde = e.get("assetPath"), "json"
            if asset:
                asset = asset.replace("shared:", "").strip()
            elif e.get("scriptClass"):
                asset, de_onde = asset_do_script(assets, dono, e["scriptClass"])
                if not asset:
                    problemas.append("%s[%d]: scriptClass %s sem assetPath e "
                                     "sem script no pacote de dados"
                                     % (chave, i, e["scriptClass"]))
                    continue
            else:
                problemas.append("%s[%d]: sem assetPath e sem scriptClass"
                                 % (chave, i))
                continue
            tipo = e.get("renderType", "animateatlas")
            base = os.path.basename(asset)
            slug = ("%s_%d_%s" % (chave, i, base)).replace("/", "_")
            if tipo == "sparrow":
                anims, erro = rotulos_do_sparrow(assets, asset)
            else:
                try:
                    anims, _at = rotulos_do_atlas(assets, asset)
                except FileNotFoundError as exc:
                    anims, erro = [], str(exc)
            # So' os rotulos que esta entrada TOCA: `startFrameLabel` (onde
            # comeca) e `loopFrameLabel` (onde recomeca). Sem isso o `bed`
            # levava tambem as animacoes `INTRO 2`/`LOOP 2` do filtro `safe` --
            # 97 quadros que nada desenha.
            # ATENCAO ao que "comeca do zero" quer dizer: sem
            # `startFrameLabel` a animacao toca desde o QUADRO 0 da primeira
            # animacao do atlas, entao TODOS os rotulos ate' o loop sao
            # precisos (o SHIT tem `Intro` 0..148 e so' entao `Loop Start`
            # -- cortar o `Intro` faria o BF cair no Loop sem a queda). Com
            # `startFrameLabel` (o `bed`: INTRO) so' os dois rotulos usados
            # entram, e os do filtro `safe` (`INTRO 2`/`LOOP 2`) ficam fora.
            if e.get("startFrameLabel"):
                usados_lab = [x for x in (e.get("startFrameLabel"),
                                          e.get("loopFrameLabel")) if x]
                so_esses = [x for x in anims if x in usados_lab]
                if so_esses:
                    anims = so_esses
            if not anims:
                problemas.append("%s[%d]: atlas %s sem rotulo (%s)"
                                 % (chave, i, asset, erro))
                continue
            s_pack = escala
            # `PERFECT` e `PERFECT_GOLD` usam a MESMA arte (o `bed` e os
            # `hearts`): empacotar duas vezes poria 1,6 MB repetidos no
            # pendrive. O cache e' por (asset, rotulos, escala).
            chave_cache = (asset, tuple(anims), s_pack, bpp)
            if chave_cache in cache:
                pac = dict(cache[chave_cache])
                problemas.append("%s reusa o pacote de %s (mesma arte)"
                                 % (chave, pac["arquivo"]))
            else:
                pac, erros = empacota(assets, asset, anims, slug, s_pack, bpp)
                if not pac:
                    problemas.extend(erros)
                    continue
                cache[chave_cache] = dict(pac)
            off = e.get("offsets") or [0, 0]
            esc = float(e.get("scale", 1.0))
            # 1280x720 -> 640x360 e' metade. Com o pacote em `s_pack`, cada px
            # do canvas vale `0.5/s_pack` px de tela na escala do JSON, entao:
            #   tamanho na tela = w * esc * (0.5/s_pack)
            # Encolher o PACOTE (degradacao de imagem) nao muda o tamanho na
            # tela -- e' o que separa fidelidade de imagem de fidelidade de
            # jogo. Ver `--degradar` abaixo.
            k = 0.5 / s_pack
            x = off[0] * 0.5 + pac["origem"][0] * k * esc
            y = off[1] * 0.5 + pac["origem"][1] * k * esc
            esc = esc * k
            delay_ms = int(round(float(e.get("delay", 0.0)) * 1000.0))
            entrada = {
                "rank": rank, "indice": i, "tipo": tipo,
                "asset": asset, "asset_de": de_onde,
                "arquivo": pac["arquivo"], "partes": pac["partes"],
                "w": pac["w"], "h": pac["h"], "psm": pac["psm"],
                "quadros": pac["quadros"], "vram_bytes": pac["vram_bytes"],
                "bytes": pac["bytes"],
                "x": round(x, 3), "y": round(y, 3),
                "escala": esc, "z": int(e.get("zIndex", 500)),
                "delay_ms": delay_ms,
                "inicio_ms": BF_MS.get(rank, 3958) + delay_ms,
                "anims": pac["anims"],
                "loop_frame": e.get("loopFrame"),
                "loop_label": e.get("loopFrameLabel"),
                "start_label": e.get("startFrameLabel") or "",
                "looped": bool(e.get("looped", True)),
            }
            # O quadro em que o loop recomeca, resolvido AQUI (o runtime nao
            # precisa procurar rotulo em tempo de execucao).
            if entrada["loop_label"]:
                f = quadro_do_label(pac, entrada["loop_label"])
                if f < 0:
                    problemas.append("%s[%d]: rotulo de loop %r fora da tabela"
                                     % (chave, i, entrada["loop_label"]))
                    continue
                entrada["loop_ini"] = f
            elif entrada["loop_frame"] is not None:
                entrada["loop_ini"] = int(entrada["loop_frame"])
            else:
                entrada["loop_ini"] = -1
            ini = 0
            if entrada["start_label"]:
                f = quadro_do_label(pac, entrada["start_label"])
                if f < 0:
                    problemas.append("%s[%d]: rotulo inicial %r fora da tabela"
                                     % (chave, i, entrada["start_label"]))
                    continue
                ini = f
            entrada["inicio"] = ini
            usados += pac["vram_bytes"]
            entrada["s_pack"] = s_pack
            entrada["_off"] = list(off)
            entrada["_esc_json"] = float(e.get("scale", 1.0))
            saida.append(entrada)
        if descartados:
            problemas.append("%s: %d entrada(s) com filtro 'safe' nao "
                             "empacotada(s) (o jogo usa naughtyness = true)"
                             % (chave, descartados))
        # Se o rank nao cabe, DEGRADA a MAIOR entrada (pacote menor, mesmo
        # tamanho na tela) e refaz a conta -- medindo, nao estimando.
        tentativas = 0
        while usados > VRAM_LIVRE and tentativas < 4 and saida:
            tentativas += 1
            maior = max(saida, key=lambda z: z["vram_bytes"])
            novo = round(maior["s_pack"] * 0.85, 4)
            anims_m = [x["nome"] for x in maior["anims"]]
            pac2, erros2 = empacota(assets, maior["asset"], anims_m,
                                    os.path.basename(maior["arquivo"])[5:-5],
                                    novo, bpp)
            if not pac2:
                problemas.extend(erros2)
                break
            antes = maior["vram_bytes"]
            usados -= antes
            off = list(maior["_off"])
            esc_json = maior["_esc_json"]
            k = 0.5 / novo
            maior.update({
                "partes": pac2["partes"], "w": pac2["w"], "h": pac2["h"],
                "quadros": pac2["quadros"], "vram_bytes": pac2["vram_bytes"],
                "bytes": pac2["bytes"], "anims": pac2["anims"],
                "s_pack": novo,
            })
            maior["x"] = round(off[0] * 0.5 + pac2["origem"][0] * k * esc_json, 3)
            maior["y"] = round(off[1] * 0.5 + pac2["origem"][1] * k * esc_json, 3)
            maior["escala"] = round(esc_json * k, 4)
            # os quadros de inicio/loop sao indices dentro do NOVO pacote
            maior["inicio"] = quadro_do_label(pac2, maior["start_label"]) \
                if maior["start_label"] else 0
            if maior["loop_label"]:
                maior["loop_ini"] = quadro_do_label(pac2, maior["loop_label"])
            usados += pac2["vram_bytes"]
            problemas.append("%s: %s degradado para escala %.3f (%d -> %d B "
                             "por quadro) para o rank caber em %d B"
                             % (chave, os.path.basename(maior["arquivo"]),
                                novo, antes, pac2["vram_bytes"], VRAM_LIVRE))
        if usados > VRAM_LIVRE:
            problemas.append("%s: %d B de VRAM contra %d B livres na tela"
                             % (chave, usados, VRAM_LIVRE))
        plano["ranks"].append({"rank": rank, "chave": chave,
                               "entradas": saida, "vram_bytes": usados})
    # Os campos que comecam com `_` sao de trabalho (offsets/escala do JSON
    # para a degradacao); nao vao para o plano nem para o header.
    for r in plano["ranks"]:
        for e in r["entradas"]:
            for k in [k for k in e if k.startswith("_")]:
                del e[k]
    return plano, problemas


def escreve_header(plano):
    L = []
    a = L.append
    a("/* GENERATED by tools/empacotar_resultado_rank.py -- do not edit by hand.")
    a(" *")
    a(" * A ANIMACAO DO PERSONAGEM NA TELA DE RESULTADO (`players/<dono>.json`")
    a(" * -> `results[rank]`). Cada rank tem uma ou duas entradas (Animate ou")
    a(" * Sparrow), com posicao, z, escala, atraso e a forma de loop que o")
    a(" * `ResultState.hx` L201-330 monta e o L704-740 dispara.")
    a(" */")
    a("#ifndef RESULTADO_RANK_DADOS_H")
    a("#define RESULTADO_RANK_DADOS_H")
    a("")
    total = max([r["vram_bytes"] for r in plano["ranks"]] or [0])
    a("#define RES_RR_RANKS      %d" % len(plano["ranks"]))
    a("#define RES_RR_MAX_ENTRADAS %d"
      % max([len(r["entradas"]) for r in plano["ranks"]] or [0]))
    a("/* Pior rank em VRAM (as entradas de um rank ficam residentes juntas):")
    a(" * %d B, contra %d B medidos livres na tela. */" % (total, VRAM_LIVRE))
    a("")
    a("typedef struct {")
    a("    const char   *nome;      /* rotulo da timeline (`INTRO`, `LOOP`) */")
    a("    unsigned short primeiro; /* quadro global no pacote               */")
    a("    unsigned short quantos;")
    a("    unsigned short fps100;")
    a("} res_rr_anim_t;")
    a("")
    a("typedef struct {")
    a("    const char        *arquivo;                 /* parte 0 do pacote  */")
    a("    const char *const *partes;                  /* todas, na ordem    */")
    a("    unsigned char      n_partes;")
    a("    unsigned short     w, h;")
    a("    unsigned short     quadros;")
    a("    unsigned char      psm;")
    a("    unsigned int       vram_bytes;")
    a("    float              x, y;      /* canto do canvas em 640x360        */")
    a("    float              escala;    /* tamanho na tela = w * escala      */")
    a("    int                z;         /* zIndex do original (499..501)     */")
    a("    unsigned short     inicio_ms; /* getBFDelay() + delay              */")
    a("    short              inicio;    /* primeiro quadro (`startFrameLabel`)*/")
    a("    short              loop_ini;  /* -1 = nao repete                   */")
    a("    unsigned char      n_anims;")
    a("    const res_rr_anim_t *anims;")
    a("} res_rr_entrada_t;")
    a("")
    a("typedef struct {")
    a("    const char         *rank;")
    a("    unsigned char       n;")
    a("    unsigned short      highscore_ms;  /* getHighscoreDelay() */")
    a("    const res_rr_entrada_t *entradas;")
    a("} res_rr_rank_t;")
    a("")

    # Uma tabela de animacoes + um vetor de partes por entrada, e o vetor de
    # entradas por rank.
    nomes = []
    for r in plano["ranks"]:
        for k, e in enumerate(r["entradas"]):
            tag = "%s_%d" % (r["rank"], k)
            nomes.append((tag, e))
            a("static const res_rr_anim_t res_rr_anims_%s[%d] = {"
              % (tag, max(1, len(e["anims"]))))
            for an in e["anims"]:
                a("    { \"%s\", %4d, %3d, %4d },"
                  % (an["nome"], an["primeiro"], an["quantos"], an["fps100"]))
            if not e["anims"]:
                a("    { \"\", 0, 0, 0 },")
            a("};")
            a("static const char *const res_rr_partes_%s[%d] = {"
              % (tag, len(e["partes"])))
            for pt in e["partes"]:
                a("    \"%s\"," % pt["arquivo"])
            a("};")
            a("")
    a("")
    for r in plano["ranks"]:
        tag = r["rank"]
        a("static const res_rr_entrada_t res_rr_entradas_%s[%d] = {"
          % (tag, max(1, len(r["entradas"]))))
        for k, e in enumerate(r["entradas"]):
            a("    { \"%s\", res_rr_partes_%s_%d, %d, %d, %d, %d, %d, %du,"
              % (e["arquivo"], tag, k, len(e["partes"]), e["w"], e["h"],
                 e["quadros"], e["psm"], e["vram_bytes"]))
            a("      %.2ff, %.2ff, %.3ff, %d, %d, %d, %d, %d,"
              % (e["x"], e["y"], e["escala"], e["z"], e["inicio_ms"],
                 e["inicio"], e["loop_ini"], len(e["anims"])))
            a("      res_rr_anims_%s_%d }," % (tag, k))
        if not r["entradas"]:
            a("    { \"\", 0, 0, 0, 0, 0, 0, 0u, 0.0f, 0.0f, 1.0f, 0, 0, 0, -1, 0, 0 },")
        a("};")
        a("")
    a("/* O `getHighscoreDelay()` do rank, em ms -- o timer do selo de recorde")
    a(" * (`ResultState.hx` L416). O selo em si NAO e' desenhado: o port nao tem")
    a(" * save, entao `params.isNewHighscore` e' DADO AUSENTE, nunca um palpite.")
    a(" * Ver `res_rr_highscore_ms` e o evento `resultado_highscore` no log. */")
    a("static const res_rr_rank_t g_res_rr[RES_RR_RANKS] = {")
    for r in plano["ranks"]:
        a("    { \"%s\", %d, %du, res_rr_entradas_%s },"
          % (r["rank"], len(r["entradas"]), HS_MS[r["rank"]], r["rank"]))
    a("};")
    a("")
    a("#endif /* RESULTADO_RANK_DADOS_H */")
    with open(HEADER, "w", encoding="utf-8") as f:
        f.write("\n".join(L) + "\n")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--assets")
    ap.add_argument("--dono", default="bf")
    ap.add_argument("--escala", type=float, default=0.5)
    ap.add_argument("--bpp", type=int, default=16, choices=(16, 32))
    ap.add_argument("--so", default=None, help="um rank so' (para depurar)")
    ap.add_argument("--ver", action="store_true", help="so' mostra o plano")
    args = ap.parse_args()
    if args.ver:
        if not os.path.isfile(PLANO):
            print("  sem plano em %s" % PLANO)
            return 1
        d = json.load(open(PLANO, encoding="utf-8"))
        for r in d["ranks"]:
            print("== %s (%d B de VRAM)" % (r["rank"], r["vram_bytes"]))
            for e in r["entradas"]:
                print("   %-28s %4dx%-4d %3d quadros  z%-3d x,y %.1f,%.1f "
                      "esc %.2f  inicio %4d ms  loop %d"
                      % (os.path.basename(e["arquivo"]), e["w"], e["h"],
                         e["quadros"], e["z"], e["x"], e["y"], e["escala"],
                         e["inicio_ms"], e["loop_ini"]))
        return 0
    if not args.assets:
        print("  FALHA: informe --assets <clone do funkin.assets>")
        return 1
    plano, problemas = empacotar(args.assets, so=args.so, escala=args.escala,
                                 bpp=args.bpp, dono=args.dono)
    for p in problemas:
        print("  " + p)
    if plano is None:
        return 1
    if args.so:
        # Roda parcial nao sobrescreve o plano commitado.
        print("  (--so: plano NAO gravado)")
        return 0
    os.makedirs(os.path.dirname(PLANO), exist_ok=True)
    with open(PLANO, "w", encoding="utf-8") as f:
        json.dump(plano, f, indent=1, ensure_ascii=False)
        f.write("\n")
    escreve_header(plano)
    print()
    for r in plano["ranks"]:
        print("== %s: %d entrada(s), %d B de VRAM"
              % (r["rank"], len(r["entradas"]), r["vram_bytes"]))
        for e in r["entradas"]:
            print("   %-30s %4dx%-4d %3d quadros  %.3f MB de pacote"
                  % (os.path.basename(e["arquivo"]), e["w"], e["h"],
                     e["quadros"], e["bytes"] / 1e6))
    print("\nplano: %s" % os.path.relpath(PLANO, PROJECT))
    print("header: %s" % os.path.relpath(HEADER, PROJECT))
    return 0


if __name__ == "__main__":
    sys.exit(main())
