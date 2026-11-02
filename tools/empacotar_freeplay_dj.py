#!/usr/bin/env python3
"""Empacota o DJ da tela de FREEPLAY (`freeplay-boyfriend`).

O QUE O ORIGINAL FAZ
--------------------
`FreeplayState.hx` L421-423 cria o DJ:

    if (currentCharacter?.getFreeplayDJData() != null)
      createFreeplayDJ((CUTOUT_WIDTH * DJ_POS_MULTI) + 640, 366, currentCharacterId);

e o `BaseFreeplayDJ.resetPosition()` (L232-246) posiciona:

    if (playableCharData.useAnimatePosition) { this.x = CUTOUT_WIDTH*DJ_POS_MULTI; this.y = 0; }
    else                                     { this.x = CUTOUT_WIDTH*DJ_POS_MULTI + 640; this.y = 366; }

`CUTOUT_WIDTH = gameCutoutSize.x / 1.5` e' **0** no PS2 (nao existe recorte de
tela de aparelho), entao no bf (`players/bf.json` -> `freeplayDJ` com
`useAnimatePosition: true`) o DJ fica em **(0, 0)** e no pico em **(640, 366)**.

`AnimateAtlasFreeplayDJ.loadAnimations()` carrega as animacoes do
`freeplayDJ.animations` do `players/<dono>.json` com
`FlxAnimationUtil.addTextureAtlasAnimations`, e o MAQUINA DE ESTADOS
(`AnimateAtlasFreeplayDJ.update`, L62-140) escolhe o prefixo por estado:

    Intro      -> 'intro'      (playFlashAnimation(prefix, true))         sem loop
    Idle       -> 'idle'       (playFlashAnimation(prefix, true, false, true))  LOOP
    Confirm    -> 'confirm'                                              sem loop
    FistPump   -> 'fistPump' / 'loss' com os `fistPump` frames do JSON
    NewUnlock  -> 'newUnlock'
    CharSelect -> 'charSelect'

ESTE ARQUIVO
------------
1. le' `preload/data/players/<dono>.json` (o dado real do original),
2. escreve `assets-data/characters/freeplay-dj-<dono>.json` (no formato que o
   `tools/achatar_animate.py --pack` ja' le': `assetPath` + `animations[]`),
3. roda o `--pack` e produz `build/packs/freeplay/dj_<dono>.chp4`,
4. le' o CHP4 gerado e escreve o plano + `src/engine/freeplay_dj_dados.h`.

POR QUE REUSAR O `achatar_animate`
----------------------------------
O DJ e' um atlas do Adobe Animate com hierarquia de simbolos (timeline de cima,
simbolos aninhados, matrizes por instancia). O `achatar_animate` ja' resolve
exatamente isso -- com o algoritmo lido do renderer de verdade (flxanimate) -- e
ja' escreve o CHP4 com a tabela de animacoes que o jogo le'. Escrever um segundo
achatador para a mesma arte seria a segunda copia de uma conta de matrizes.

QUEM E' O DONO
--------------
O estilo da Freeplay vem do personagem JOGAVEL da sessao (`players/<id>.json`).
O port nao tem char select e o personagem de menu e' sempre o bf, entao o que se
empacota e' o `bf`. O `pico` fica com o caminho registrado no plano (e o mesmo
script empacota os dois com `--dono`), mas nao entra no pendrive: arte que nada
seleciona.

Uso:
    PYTHONPATH=/tmp/pylibs tools/empacotar_freeplay_dj.py --assets /tmp/fassets
    tools/empacotar_freeplay_dj.py --ver
"""

import argparse
import json
import os
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
PROJECT = os.path.dirname(HERE)
PLANO = os.path.join(PROJECT, "assets-data", "freeplay-dj-plan.json")
HEADER = os.path.join(PROJECT, "src", "engine", "freeplay_dj_dados.h")
# O JSON intermediario que o `achatar_animate --pack` le'. Ele NAO fica em
# `assets-data/characters/`: aquele diretorio e' a lista de personagens do JOGO
# (`tools/gen_char_dados.py` gera `char_dados.h` a partir dela) e um DJ de tela
# de menu entraria como personagem de gameplay. Fica no scratch do build.
CHARS = os.path.join(PROJECT, "build", "freeplay-dj")

# `BaseFreeplayDJ`: os estados que tocam animacao e o loop de cada um.
# `AnimateAtlasFreeplayDJ.update` L62-140 -- a 4. chamada e' o `Loop`.
LOOPED = {
    "idle": True,
}

# Posicao do DJ: `resetPosition()` L232-246 com CUTOUT_WIDTH = 0.
POS_ANIMATE = (0.0, 0.0)      # `useAnimatePosition: true` (bf)
POS_PADRAO = (640.0, 366.0)   # `else` da fonte (pico)


def ler_player(assets, dono):
    caminho = os.path.join(assets, "preload", "data", "players", dono + ".json")
    if not os.path.isfile(caminho):
        return None, "falta %s" % caminho
    with open(caminho, encoding="utf-8-sig") as f:
        doc = json.load(f)
    dj = doc.get("freeplayDJ")
    if not dj:
        return None, "%s nao tem freeplayDJ" % dono
    return dj, None


def escreve_char_json(dj, dono):
    """O JSON no formato que o `achatar_animate --pack` ja' entende.

    `name`  = o nome do estado no `players/<dono>.json` ('intro', 'idle', ...),
              que e' exatamente o que o jogo pede por estado.
    `prefix`= o rotulo da timeline ('Intro', 'Idle', 'Fist Pump', ...).
    Nada e' deduzido: as duas colunas vem do JSON do original.
    """
    anims = []
    faltando = []
    for a in dj.get("animations") or []:
        nome, pre = a.get("name"), a.get("prefix")
        if not nome or not pre:
            faltando.append(a)
            continue
        anims.append({"name": nome, "prefix": pre,
                      "looped": bool(LOOPED.get(nome, False))})
    doc = {
        "version": "1.0.0",
        "name": "Freeplay DJ (%s)" % dono,
        "renderType": "animateatlas",
        "assetPath": dj.get("assetPath"),
        "animations": anims,
    }
    os.makedirs(CHARS, exist_ok=True)
    destino = os.path.join(CHARS, "freeplay-dj-%s.json" % dono)
    with open(destino, "w", encoding="utf-8") as f:
        json.dump(doc, f, indent=1, ensure_ascii=False)
        f.write("\n")
    return doc, faltando


def escreve_header(plano):
    L = []
    a = L.append
    a("/* GENERATED by tools/empacotar_freeplay_dj.py -- do not edit by hand.")
    a(" *")
    a(" * O DJ da tela de FREEPLAY: o pacote CHP4 do `freeplay-boyfriend`, a")
    a(" * tabela de animacoes que veio do proprio pacote e a posicao de")
    a(" * `BaseFreeplayDJ.resetPosition()`. Toda constante tem linha na fonte")
    a(" * v0.8.6 e no `preload/data/players/<dono>.json` do original.")
    a(" */")
    a("#ifndef FREEPLAY_DJ_DADOS_H")
    a("#define FREEPLAY_DJ_DADOS_H")
    a("")
    a("#define FP_DJ_TEX        \"%s\"" % plano["tex"])
    a("#define FP_DJ_W          %d" % plano["w"])
    a("#define FP_DJ_H          %d" % plano["h"])
    a("#define FP_DJ_PSM        %d" % plano["psm"])
    a("#define FP_DJ_VRAM       %du     /* %d B por quadro */"
      % (plano["vram_bytes"], plano["vram_bytes"]))
    a("#define FP_DJ_QUADROS    %d" % plano["quadros"])
    a("#define FP_DJ_PARTES_N   %d" % len(plano["partes"]))
    a("#define FP_DJ_X          %.3ff   /* CUTOUT_WIDTH e' 0 no PS2 */"
      % plano["x"])
    a("#define FP_DJ_Y          %.3ff" % plano["y"])
    a("/* Os arquivos do pacote, na ordem: `write_packs` corta em 8 MB. */")
    a("static const char *const fp_dj_partes[FP_DJ_PARTES_N] = {")
    for pt in plano["partes"]:
        a("    \"%s\"," % pt["arquivo"])
    a("};")
    a("")
    fp = plano.get("fistPump") or {}
    a("/* As janelas do `fistPump` do `players/<dono>.json`: `fistPumpIntro`")
    a(" * repete [ini..introFim] e `fistPump` toca [loopIni..loopFim] uma vez.")
    a(" * `loopEndFrame` -1 quer dizer \"sem fim\" (toca uma vez e volta ao Idle). */")
    a("#define FP_DJ_FIST_INI          %d" % fp.get("introStartFrame", 0))
    a("#define FP_DJ_FIST_INTRO_FIM    %d" % fp.get("introEndFrame", -1))
    a("#define FP_DJ_FIST_LOOP_INI     %d" % fp.get("loopStartFrame", 0))
    a("#define FP_DJ_FIST_LOOP_FIM     %d" % fp.get("loopEndFrame", -1))
    a("#define FP_DJ_FIST_INI_BAD      %d" % fp.get("introBadStartFrame", 0))
    a("#define FP_DJ_FIST_INTRO_FIM_BAD %d" % fp.get("introBadEndFrame", -1))
    a("#define FP_DJ_FIST_LOOP_INI_BAD %d" % fp.get("loopBadStartFrame", 0))
    a("#define FP_DJ_FIST_LOOP_FIM_BAD %d" % fp.get("loopBadEndFrame", -1))
    a("")
    a("#define FP_DJ_ANIMS_N    %d" % len(plano["anims"]))
    a("")
    a("/* A tabela do CHP4, na ordem do pacote. */")
    a("typedef struct { const char *nome; int primeiro, quantos, fps100, looped; }")
    a("               fp_dj_anim_t;")
    a("static const fp_dj_anim_t fp_dj_anims[FP_DJ_ANIMS_N] = {")
    for an in plano["anims"]:
        # Sem preenchimento: o runtime compara o nome com `strcmp` e um nome
        # com espacos sobrando nunca casa (custou um build com o DJ parado no
        # Idle e sem desenhar).
        a("    { \"%s\", %4d, %4d, %5d, %d },"
          % (an["nome"], an["primeiro"], an["quantos"], an["fps100"],
             an["looped"]))
    a("};")
    a("")
    a("#endif /* FREEPLAY_DJ_DADOS_H */")
    with open(HEADER, "w", encoding="utf-8") as f:
        f.write("\n".join(L) + "\n")


def empacotar(assets, dono, escala, bpp, out):
    dj, erro = ler_player(assets, dono)
    if not dj:
        return None, [erro]
    doc, faltando = escreve_char_json(dj, dono)
    problemas = ["freeplay-dj: %d animacao(oes) sem nome/prefixo no JSON" % len(faltando)] \
        if faltando else []

    os.makedirs(out, exist_ok=True)
    # ORFAOS: um pacote antigo com outro nome de parte (era `dj_bf.chp4.part1`)
    # ficaria na pasta e iria para o pendrive para sempre, sem ninguem abrir.
    import glob
    for velho in glob.glob(os.path.join(out, "dj_%s.*" % dono)):
        os.remove(velho)
    # O `--pack` escreve as PARTES em CFZ4 (que e' o que `pm_abrir`/`pst_abrir`
    # leem no console) e, ao lado, um CHP4 com a mesma coisa mais a tabela.
    # Os dois NAO podem ter o mesmo nome: antes o CHP4 era escrito por cima da
    # parte 0 (`dj_bf.chp4`), e o jogo recebia um arquivo com o magic "CHP4" no
    # lugar do "CFZ4" -- `pm_abrir` o recusaria e o DJ nunca apareceria no
    # console. Os nomes agora sao distintos e as partes sao CONFERIDAS.
    destino = os.path.join(out, "dj_%s.cfz4" % dono)
    # `--char-dir`: o JSON do DJ nao mora em `assets-data/characters/` (aquele
    # diretorio e' a lista de personagens do jogo), e sem isto o
    # `achatar_animate` procura o arquivo la' e estoura com FileNotFoundError.
    cmd = [sys.executable, os.path.join(HERE, "achatar_animate.py"),
           "--assets", assets, "--char-dir", CHARS,
           "--character", "freeplay-dj-%s" % dono,
           "--pack", destino, "--scale", str(escala), "--bpp", str(bpp)]
    r = subprocess.run(cmd, capture_output=True, text=True)
    sys.stdout.write(r.stdout)
    if r.returncode != 0:
        sys.stderr.write(r.stderr)
        return None, problemas + ["freeplay-dj: o achatar_animate falhou"]

    import chp4
    caminho_chp4 = os.path.splitext(destino)[0] + ".chp4"
    dados = open(caminho_chp4, "rb").read()
    # As partes que o `write_packs` criou (o limite de 8 MB por arquivo): o
    # leitor do console abre TODAS, na ordem, e trata os quadros como um
    # fluxo so'. O plano registra os nomes e o tamanho de cada uma.
    partes = []
    i = 0
    while True:
        nome = os.path.basename(destino) if i == 0 else \
            os.path.basename(destino) + ".part%d" % i
        caminho = os.path.join(out, nome)
        if not os.path.isfile(caminho):
            break
        with open(caminho, "rb") as fb:
            if fb.read(4) != b"CFZ4":
                return None, problemas + [
                    "freeplay-dj: %s nao tem magic CFZ4 (o leitor do console o "
                    "recusaria)" % nome]
        partes.append({"arquivo": nome, "bytes": os.path.getsize(caminho)})
        i += 1
    cab = chp4.ler(dados)
    chp4.conferir(cab)
    anims = [{"nome": a["nome"], "primeiro": a["primeiro"],
              "quantos": a["quantos"], "fps100": a["fps100"],
              "looped": int(a["looped"])} for a in cab["anims"]]
    x, y = POS_ANIMATE if dj.get("useAnimatePosition") else POS_PADRAO
    plano = {
        "dono": dono,
        "assetPath": dj.get("assetPath"),
        "tex": "freeplay/dj_%s.chp4" % dono,
        "w": cab["w"], "h": cab["h"], "psm": cab["psm"],
        "escala": escala,
        "quadros": cab["n_quadros"],
        "vram_bytes": cab["w"] * cab["h"] * (cab["psm"] // 8),
        "bytes": len(dados),
        # `resetPosition()`: metade da tela do port.
        "x": round(x * 0.5, 3),
        "y": round(y * 0.5, 3),
        "useAnimatePosition": bool(dj.get("useAnimatePosition")),
        "partes": partes,
        "anims": anims,
        "fistPump": dj.get("fistPump"),
        "linha": "FreeplayState.hx L421-423; BaseFreeplayDJ.hx L232-246; "
                 "AnimateAtlasFreeplayDJ.hx L62-140",
    }
    return plano, problemas


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--assets")
    ap.add_argument("--dono", default="bf")
    ap.add_argument("--escala", type=float, default=0.5)
    ap.add_argument("--bpp", type=int, default=16, choices=(16, 32))
    ap.add_argument("--out", default=os.path.join(PROJECT, "build", "packs", "freeplay"))
    ap.add_argument("--ver", action="store_true")
    args = ap.parse_args()

    if args.ver:
        if not os.path.isfile(PLANO):
            print("  FALHA: nao ha' plano em %s" % PLANO)
            return 1
        d = json.load(open(PLANO, encoding="utf-8"))
        print("  %s: %d animacoes, canvas %dx%d, %d quadros, %d B em disco, "
              "%d B por quadro na VRAM, em (%.1f, %.1f)"
              % (d["tex"], len(d["anims"]), d["w"], d["h"], d["quadros"],
                 d["bytes"], d["vram_bytes"], d["x"], d["y"]))
        for an in d["anims"]:
            print("    %-20s quadros %3d..%-3d  %5.2f fps  %s"
                  % (an["nome"], an["primeiro"],
                     an["primeiro"] + an["quantos"] - 1, an["fps100"] / 100.0,
                     "loop" if an["looped"] else "uma vez"))
        return 0

    if not args.assets:
        print("  FALHA: informe --assets <clone do funkin.assets> (ou --ver)")
        return 1
    plano, problemas = empacotar(args.assets, args.dono, args.escala, args.bpp,
                                 args.out)
    for p in problemas:
        print("  AVISO: " + p)
    if not plano:
        print("  FALHA: nada empacotado")
        return 1
    with open(PLANO, "w", encoding="utf-8") as f:
        json.dump(plano, f, indent=1, ensure_ascii=False)
        f.write("\n")
    escreve_header(plano)
    print("freeplay-dj: %s  canvas %dx%d, %d quadros, %d animacoes"
          % (plano["tex"], plano["w"], plano["h"], plano["quadros"],
             len(plano["anims"])))
    print("  %.2f MB em disco, %d B por quadro na VRAM, desenhado em (%.1f, %.1f)"
          % (plano["bytes"] / 1048576.0, plano["vram_bytes"], plano["x"],
             plano["y"]))
    return 0


if __name__ == "__main__":
    sys.exit(main())
