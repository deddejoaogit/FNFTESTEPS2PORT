#!/usr/bin/env python3
"""Empacota o NOME DA MUSICA da barra do topo da tela de Resultado.

O QUE A FONTE FAZ (`ResultState.hx` L119-122, `FlxBitmapText`)
----------------------------------------------------------------
    final fontLetters = "AaBbCcDdEeFfGgHhiIJjKkLlMmNnOoPpQqRrSsTtUuVvWwXxYyZz:1234567890().-";
    songName = new FlxBitmapText(FlxBitmapFont.fromMonospace(
        Paths.image("resultScreen/tardlingSpritesheet"), fontLetters, FlxPoint.get(49, 61)));
    songName.text = params.title;
    songName.letterSpacing = -15;
    songName.angle = -4.4;

`FlxBitmapFont.fromMonospace` (v5.x) corta a folha em celulas de `charSize`
PREENCHENDO de cima para baixo, da esquerda para a direita: com 512x512 e
celulas de 49x61 sao 10 colunas (512/49) e 8 linhas, e as 67 letras ocupam as
6,7 primeiras linhas. Cada letra avanca `charWidth` = 49 px e o
`letterSpacing` de -15 px deixa o avanco efetivo em **34 px**.

O `FlxBitmapText` NAO desenha letra por letra na tela: ele monta um `BitmapData`
do tamanho do texto (`updateTextBitmap`) e o sprite mostra esse bitmap. A
largura e' `n*49 + (n-1)*(-15)` = `34n + 15` e a altura e' `lineHeight` = 61.

POR QUE AQUI O TEXTO E' PRE-RENDERIZADO, UM ARQUIVO POR MUSICA
--------------------------------------------------------------
O port nao tem um compositor de texto em tempo de execucao (e nao precisa ter:
sao 27 titulos fixos). Cada musica leva o seu `.rgba` pronto, do tamanho exato
do texto, e a tela desenha **um quadrilatero girado** com ele -- o mesmo
caminho que o `FlxSprite` do original faz com o bitmap que montou.

O que se gasta: a VRAM de UM titulo por partida. O orcamento restante da tela
de Resultado depois das outras pecas e' de ~35 KB (`pool_cena,9` = 1.261.568 B
livres menos 1.225.756 B de arte), entao a escala de cada titulo e' a MAIOR que
cabe em `LIMITE_VRAM` bytes: titulos curtos saem a 0,5 (a escala da tela),
titulos longos caem um ou dois degraus. A escala escolhida vai no dado, e o
jogo desenha no tamanho que ela diz -- nao ha' conta escondida no codigo.

Uso:
    PYTHONPATH=/tmp/pylibs tools/empacotar_nome.py --assets /tmp/fassets
"""

import argparse
import json
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
PROJECT = os.path.dirname(HERE)
PLANO = os.path.join(PROJECT, "assets-data", "resultado-plan.json")
REGISTRO = os.path.join(PROJECT, "src", "engine", "song_registry_data.inc")

# A fonte: `FlxPoint.get(49, 61)` e a string de letras, do ResultState.hx.
LETRAS = ("AaBbCcDdEeFfGgHhiIJjKkLlMmNnOoPpQqRrSsTtUuVvWwXxYyZz"
          ":1234567890().-")
CELULA_W, CELULA_H = 49, 61
ESPACO_LETRA = -15          # `songName.letterSpacing = -15`
AVANCO = CELULA_W + ESPACO_LETRA      # 34 px por letra
ANGULO = -4.4               # `songName.angle`

# Escalas candidatas, da maior para a menor. A primeira que couber no orcamento
# e no limite de largura de textura do GS (1024 px) e' a usada.
ESCALAS = (0.5, 0.45, 0.4, 0.36, 0.32, 0.28, 0.25, 0.22, 0.2, 0.18, 0.16)
LIMITE_VRAM = 33000         # ~35 KB livres na tela de Resultado, com folga
MAX_TEX = 1024


def titulos():
    """[(id, titulo)] do registro de musicas do proprio port, na ordem."""
    texto = open(REGISTRO, encoding="utf-8").read()
    saida = []
    for m in re.finditer(r'static const sr_song_t s_[a-z0-9_]+ = \{\s*\n\s*"([a-z0-9\-]+)", "([^"]+)"',
                         texto):
        saida.append((m.group(1), m.group(2)))
    return saida


def largura_altura(titulo, k):
    """O tamanho do bitmap que o `FlxBitmapText` montaria, na escala k."""
    n = max(1, len(titulo))
    w = int(round((AVANCO * n + (-ESPACO_LETRA)) * k))   # 34n + 15, pela escala
    h = int(round(CELULA_H * k))
    return max(1, w), max(1, h)


def escolher_escala(titulo):
    for k in ESCALAS:
        w, h = largura_altura(titulo, k)
        if w <= MAX_TEX and w * h * 4 <= LIMITE_VRAM:
            return k, w, h
    return None


def empacotar(assets, out):
    from PIL import Image

    src = os.path.join(assets, "shared", "images", "resultScreen",
                       "tardlingSpritesheet.png")
    problemas = []
    if not os.path.isfile(src):
        return None, ["nome: falta %s" % src]
    folha = Image.open(src).convert("RGBA")

    colunas = folha.width // CELULA_W
    if colunas <= 0:
        return None, ["nome: a folha %dx%d nao tem uma celula de %dx%d"
                      % (folha.width, folha.height, CELULA_W, CELULA_H)]

    # O glifo de cada letra: `addCharFrame(letters.charCodeAt(letterIndex), ...)`
    # preenche em ordem, andando para a direita e depois para baixo.
    glifos = {}
    for i, letra in enumerate(LETRAS):
        col = i % colunas
        lin = i // colunas
        caixa = (col * CELULA_W, lin * CELULA_H,
                 col * CELULA_W + CELULA_W, lin * CELULA_H + CELULA_H)
        glifos[letra] = folha.crop(caixa)

    saida = {}
    for sid, titulo in titulos():
        escolha = escolher_escala(titulo)
        if not escolha:
            problemas.append("nome: '%s' nao cabe em nenhuma escala" % titulo)
            continue
        k, w, h = escolha
        tela = Image.new("RGBA", (w, h), (0, 0, 0, 0))
        faltando = set()
        for i, letra in enumerate(titulo):
            if letra == " ":
                continue        # `SPACE_CODE`: so' avanca, nao desenha
            g = glifos.get(letra)
            if g is None:
                faltando.add(letra)
                continue
            x = int(round(AVANCO * i * k))
            gl = g.resize((max(1, int(round(CELULA_W * k))),
                           max(1, int(round(CELULA_H * k)))), Image.LANCZOS)
            tela.alpha_composite(gl, (max(0, x), 0))
        if faltando:
            # NAO se inventa glifo: o original tambem nao desenha o que a fonte
            # nao tem (`if (font.charExists(charCode))`). Fica registrado.
            problemas.append("nome: '%s' usa letra fora da fonte: %s"
                             % (titulo, "".join(sorted(faltando))))
        arquivo = "nome_%s.rgba" % sid
        with open(os.path.join(out, arquivo), "wb") as f:
            f.write(tela.tobytes())
        # `fuckedupnumber` e o alvo do tween em y (ResultState.hx L748-750):
        #   -(width * 0.5) * sin(angle) - 10
        # e' o que compensa a rotacao para o texto entrar reto na barra.
        largura_full = AVANCO * max(1, len(titulo)) + (-ESPACO_LETRA)
        import math
        fucked = -(largura_full * 0.5) * math.sin(ANGULO * math.pi / 180.0) - 10.0
        saida[sid] = {
            "id": sid, "titulo": titulo,
            "arquivo": "resultado/" + arquivo,
            "w": w, "h": h, "vram_bytes": w * h * 4,
            "escala": round(k, 4),
            # Em 640x360 (a tela do port).
            "larg": round(w, 1), "alt": round(h, 1),
            "y_alvo": round(((122.0 - 25.0 - fucked) * 0.5), 3),
            "larg_full_half": round(largura_full * 0.5, 3),
        }

    dado = {
        "nomes": saida,
        "letras": LETRAS,
        "celula": [CELULA_W, CELULA_H],
        "espaco_letra": ESPACO_LETRA,
        "angulo": ANGULO,
        "escalas": list(ESCALAS),
        "limite_vram": LIMITE_VRAM,
        # `timerThenSongName()`: entra em y com expoOut depois de 0,9 s; a
        # velocidade volta de 0 num tween de 0,7 s com quadIn que comeca em
        # `timerLength` (1,0 s no create, 3,0 s nos ciclos seguintes).
        "entrada_atraso_ms": 900,
        "entrada_ms": 500,
        "velocidade_ms": 700,
        "ciclo_espera_ms": 3000,
        "ciclo_espera_primeiro_ms": 1000,
        # `showSmallClearPercent()` solta o movimento 2,5 s depois de entrar.
        "movimento_atraso_ms": 2500,
        # `speedOfTween`: (-cos, -sin) do angulo, x60 por segundo.
        "vel_x": round(-math.cos(ANGULO * math.pi / 180.0) * 60.0, 4),
        "vel_y": round(-math.sin(ANGULO * math.pi / 180.0) * 60.0, 4),
        # `songName.x = clearPercentSmall.x + 94` e o corte da esquerda:
        # `clipRect = FlxRect(max(0, 520 - x), ...)`.
        "x_do_pequeno": 94 * 0.5,
        "clip_x": 520 * 0.5,
        # `if (songName.x + songName.width < 100)` -> novo ciclo.
        "ciclo_fim_x": 100 * 0.5,
        "linha": "ResultState.hx L119-122, L736-765, L792-836",
    }
    return dado, problemas


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--assets")
    ap.add_argument("--out",
                    default=os.path.join(PROJECT, "build", "packs", "resultado"))
    args = ap.parse_args()
    if not args.assets:
        print("  FALHA: informe --assets <clone do funkin.assets>")
        return 1
    try:
        from PIL import Image  # noqa: F401
    except ImportError:
        print("  FALHA: falta o Pillow (PYTHONPATH=/tmp/pylibs)")
        return 1
    os.makedirs(args.out, exist_ok=True)
    dado, problemas = empacotar(args.assets, args.out)
    for p in problemas:
        print("  AVISO: " + p)
    if not dado:
        print("  FALHA: nada empacotado")
        return 1
    plano = json.load(open(PLANO, encoding="utf-8"))
    plano["nome"] = dado
    with open(PLANO, "w", encoding="utf-8") as f:
        json.dump(plano, f, indent=1, ensure_ascii=False)
        f.write("\n")
    print("nome da musica: %d titulos (fonte %dx%d, avanco %d, angulo %.1f)"
          % (len(dado["nomes"]), CELULA_W, CELULA_H, AVANCO, ANGULO))
    for sid, n in dado["nomes"].items():
        print("  %-20s %-44s escala %.2f  %3dx%-2d  %6d B"
              % (sid, n["titulo"], n["escala"], n["w"], n["h"],
                 n["vram_bytes"]))
    return 0


if __name__ == "__main__":
    sys.exit(main())
