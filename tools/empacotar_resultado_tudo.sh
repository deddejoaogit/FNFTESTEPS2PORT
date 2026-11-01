#!/bin/sh
# Empacota TODA a arte da tela de Resultado, na ORDEM CERTA.
#
# Por que este script existe: `empacotar_resultado.py` escreve o plano BASE
# (`assets-data/resultado-plan.json`) e os outros quatro empacotadores o LEEM e
# acrescentam a sua peca. Rodar o `empacotar_resultado.py` DEPOIS dos outros
# apaga as pecas deles do plano -- foi o que aconteceu nesta rodada (o
# `gen_resultado_dados.py` quebrou com `KeyError: 'tallies'`). A ordem abaixo e'
# a unica que funciona:
#
#   1. empacotar_resultado.py     folhas animadas, fundo, rank, placar (base)
#   2. empacotar_tallies.py       os sete contadores de julgamento
#   3. empacotar_clearpercent.py  o percentual (contador grande e pequeno)
#   4. empacotar_topbar.py        a barra do topo e a imagem da dificuldade
#
# Uso:  PYTHONPATH=/tmp/pylibs sh tools/empacotar_resultado_tudo.sh /tmp/fassets
set -e
ASSETS=${1:-${FNF_ASSETS:-/tmp/fassets}}
HERE=$(cd "$(dirname "$0")/.." && pwd)
cd "$HERE"
for t in empacotar_resultado empacotar_tallies empacotar_clearpercent empacotar_topbar; do
    echo "== $t =="
    python3 "tools/$t.py" --assets "$ASSETS"
done
echo "== dados do resultado (src/engine/resultado_dados.h) =="
python3 tools/gen_resultado_dados.py
