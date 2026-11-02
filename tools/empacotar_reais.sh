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
# shared/images, preload/images, preload/data, preload/scripts, shared/music --
# e Pillow + lz4 + numpy (PYTHONPATH, por padrao /tmp/pylibs).
#
# Uso tipico:
#   git clone --filter=blob:none --sparse https://github.com/FunkinCrew/funkin.assets /tmp/fassets
#   git -C /tmp/fassets sparse-checkout set shared/images preload/images \
#       preload/data preload/scripts shared/music
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

echo "== resultado =="
sh tools/empacotar_resultado_tudo.sh "$ASSETS"

echo "== freeplay =="
python3 tools/empacotar_freeplay.py --assets "$ASSETS" --out "$BASE/freeplay"
python3 tools/empacotar_freeplay_dj.py --assets "$ASSETS" --out "$BASE/freeplay"

echo "== audio =="
python3 tools/empacotar_audio.py --assets "$ASSETS" --out "$BASE/audio"

echo
echo "PACOTES PRONTOS em $BASE:"
du -sh "$BASE"/* 2>/dev/null
