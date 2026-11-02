#!/bin/sh
# Empacota TUDO o que o jogo le no pendrive, a partir do clone da arte.
#
#   tools/empacotar_reais.sh [clone-funkin.assets] [saida-base]
#
# Por que existe: `build/` e' estado de build e nao vai para o Git (nem para o
# snapshot do ambiente). Depois de um ambiente novo, cada pasta de pacotes tem
# de ser refeita, e a ordem importa -- `tools/build_pendrive.sh` so' COPIA o que
# ja' existe e avisa quando falta. Sem um driver, a reconstrucao era uma lista
# de 15 comandos escrita a mao, e esquecer um deles nao dava erro: dava um
# pendrive sem aquele pacote.
#
# O que sai (tudo abaixo de build/packs por padrao):
#   chars/     46 personagens  (.cfz4 + .chp4)   ~42 min, o passo longo
#   stages/    21 palcos                          os props de cada palco
#   ui/        barra de vida, splash, fonte
#   icons/     46 icones de vida (.icp)
#   resultado/ placar, tallies, ranks, selo, topo, nome da musica, sfx
#   freeplay/  capsulas, fundo, titulos, DJ (.chp4 + .cfz4)
#   audio/     PCM 48 kHz dos sfx de game over
#
# Requer: clone esparso de github.com/FunkinCrew/funkin.assets com
#   shared/images preload/images preload/data preload/scripts shared/music
#   fonts week1 .. week7 weekend1 weekend2 sserafim tutorial \
#       shared/sounds preload/sounds
#
# Faltar uma dessas pastas NAO da' erro no jogo: da' um pacote a menos ou, pior,
# um palco sem os props (a arte dos palcos e dos personagens das semanas mora em
# `weekN/images`, nao em `preload/images`). O script confere a lista antes de
# comecar. `fonts` tambem e' obrigatorio: sem `fonts/5by7.ttf` o titulo das
# capsulas nao empacota e a Freeplay fica sem titulo nenhum.
# Alem do clone: Pillow + lz4 + numpy (PYTHONPATH, por padrao /tmp/pylibs).
#
# Uso tipico:
#   git clone --filter=blob:none --sparse https://github.com/FunkinCrew/funkin.assets /tmp/fassets
#   git -C /tmp/fassets sparse-checkout set shared/images preload/images \
#       preload/data preload/scripts shared/music shared/sounds preload/sounds \
#       fonts week1 week2 week3 week4 week5 week6 week7 weekend1 weekend2 \
#       sserafim tutorial
#   pip3 install --target /tmp/pylibs pillow lz4 numpy
#   sh tools/empacotar_reais.sh /tmp/fassets
#
# FORCAR=1 reempacota personagens que ja' existem (ver tools/achatar_tudo.sh).
set -e

HERE=$(cd "$(dirname "$0")/.." && pwd)
cd "$HERE" || exit 1

ASSETS=${1:-${FNF_ASSETS:-/tmp/fassets}}
BASE=${2:-build/packs}
PYTHONPATH=${PYTHONPATH:-/tmp/pylibs}
export PYTHONPATH

if [ ! -d "$ASSETS/shared/images" ]; then
    echo "FALHA: $ASSETS/shared/images nao existe -- clone da arte ausente."
    echo "       Veja o cabecalho deste script (sparse-checkout do funkin.assets)."
    exit 1
fi
# A fonte do titulo das capsulas mora em fonts/ no clone, e o sparse-checkout
# padrao das instrucoes antigas nao a trazia: o empacotador da Freeplay parava
# em "falta a fonte .../fonts/5by7.ttf" e a tela ficava sem titulo nenhum.
if [ ! -f "$ASSETS/fonts/5by7.ttf" ]; then
    echo "FALHA: $ASSETS/fonts/5by7.ttf nao existe."
    echo "       git -C $ASSETS sparse-checkout add fonts"
    exit 1
fi
# A arte das semanas. Sem estas pastas os palcos saem SEM props (nao da' erro:
# da' um palco vazio), entao a conferencia e' aqui, antes de empacotar.
FALTANDO=""
for d in week1 week2 week3 week4 week5 week6 week7 weekend1 weekend2; do
    [ -d "$ASSETS/$d/images" ] || FALTANDO="$FALTANDO $d"
done
# Sons: o `fnf_loss_sfx` (Game Over), os dois do menu (scroll/confirm), o
# `fakeout_death` e o `tickleFight`. Sem eles o empacotador de audio termina
# com uma lista de FALTANDO e o jogo fica sem esses sons.
for d in shared/sounds preload/sounds; do
    [ -d "$ASSETS/$d" ] || FALTANDO="$FALTANDO $d"
done
if [ -n "$FALTANDO" ]; then
    echo "FALHA: faltam pastas de arte no clone:$FALTANDO"
    echo "       git -C $ASSETS sparse-checkout add $FALTANDO"
    exit 1
fi
if ! python3 -c "import PIL, lz4, numpy" 2>/dev/null; then
    echo "FALHA: falta Pillow/lz4/numpy em $PYTHONPATH."
    echo "       pip3 install --target $PYTHONPATH pillow lz4 numpy"
    exit 1
fi

mkdir -p "$BASE"

# A ordem e' a mesma da leitura no console: o palco e o personagem sao o que
# custa tempo, entao vao primeiro e o que sobra e' rapido.
echo "== chars (46 personagens) =="
sh tools/achatar_tudo.sh "$ASSETS" "$BASE/chars"

echo "== stages (21 palcos) =="
# Sem `--stages`, os 21 palcos. A pasta de saida vem do `--out` (o script grava
# em <out>/stages e faz merge no plano a cada palco).
python3 tools/pacotar_palcos.py --assets "$ASSETS" --out "$BASE"

echo "== icons (46 icones de vida) =="
python3 tools/empacotar_icones.py --assets "$ASSETS" --out "$BASE/icons"

echo "== ui =="
python3 tools/empacotar_ui.py --assets "$ASSETS" --out "$BASE/ui"
python3 tools/empacotar_splash.py --assets "$ASSETS" --out "$BASE/ui"

echo "== freeplay =="
python3 tools/empacotar_freeplay.py --assets "$ASSETS" --out "$BASE/freeplay"
python3 tools/empacotar_freeplay_dj.py --assets "$ASSETS" --out "$BASE/freeplay"
# Os OITO jingles do rank da Freeplay (`preload/sounds/ranks/*.ogg`).
PYTHONPATH=tools python3 tools/empacotar_rank_sons.py --assets "$ASSETS" --out "$BASE/freeplay"

# O AUDIO VEM ANTES DO RESULTADO, e nao depois: `empacotar_resultado_musica.py`
# decide o `intro` de cada rank olhando se `<packs>/res_<rank>_intro.pcm`
# EXISTE. Com o audio empacotado por ultimo, todos os `intro` saiam vazios em
# silencio -- e o resultado perdia a introducao da musica (o
# `Paths.music('$musicPath/$musicPath-intro')` do ResultState.hx L505).
echo "== audio =="
python3 tools/empacotar_audio.py --assets "$ASSETS" --out "$BASE/audio"

echo "== resultado =="
sh tools/empacotar_resultado_tudo.sh "$ASSETS"

echo
echo "PACOTES PRONTOS em $BASE:"
du -sh "$BASE"/* 2>/dev/null
