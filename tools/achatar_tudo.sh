#!/bin/sh
# Flatten every character in assets-data into a CFZ4 pack.
#
#   tools/achatar_tudo.sh <caminho-do-clone-funkin.assets> [saida]
#
# Um personagem pode estar num de tres layouts no disco, e cada um tem seu
# caminho:
#   Animate (34)      Animation.json + spritemapN.json/png  -> achatar_animate
#   Sparrow (4)       PNG + XML                             -> pacote_personagem
#   Packer  (1)       PNG + .txt                            -> pacote_personagem
# A classificacao NAO e chutada: tools/scope_animate.py mede e imprime. Este
# script usa a mesma regra de deteccao do empacotador, e falha alto se um
# personagem nao puder ser tratado por nenhum dos dois caminhos.
#
# 16 bpp e escala 0.5: a escala em que o original exibe (ver ps2_display.py) e
# a profundidade que faz os personagens caberem em VRAM (ver vram_budget.py).
set -e
HERE=$(cd "$(dirname "$0")/.." && pwd)
cd "$HERE"
ASSETS=${1:?uso: tools/achatar_tudo.sh <clone-funkin.assets> [saida]}
OUT=${2:-build/packs/chars}
PY=${PY:-/home/user/.venv/bin/python}
mkdir -p "$OUT"

ok=0; fail=0
for f in assets-data/characters/*.json; do
    cid=$(basename "$f" .json)
    [ -f "$OUT/$cid.cfz4" ] && continue      # ja empacotado
    apath=$($PY -c "import json,sys;print(json.load(open('$f')).get('assetPath') or '$cid')")
    # resolve like the packers do: package prefix, then engine search order
    stem=$(printf '%s' "$apath" | sed 's|^[^:]*:||')
    base=$(basename "$stem")
    found=""
    for d in shared/images preload/images tutorial/images week1/images \
             week2/images week3/images week4/images week5/images \
             week6/images week7/images weekend1/images sserafim/images; do
        if [ -d "$ASSETS/$d/$stem" ] && ls "$ASSETS/$d/$stem"/spritemap*.png >/dev/null 2>&1; then
            found="$ASSETS/$d/$stem"; break
        fi
        # Os planos declaram assetPath com a pasta dentro ("characters/spirit"),
        # entao o .png fica em <d>/characters/spirit.png. Olhar so <d>/<base>.png
        # errava 12 de 46.
        if [ -f "$ASSETS/$d/$stem.png" ]; then found="$ASSETS/$d/$stem.png"; break; fi
        if [ -f "$ASSETS/$d/$base.png" ]; then found="$ASSETS/$d/$base.png"; break; fi
    done
    if [ -z "$found" ]; then
        echo "$cid: nao encontrado ($apath)"; fail=$((fail+1)); continue
    fi
    if [ -d "$found" ]; then
        if $PY tools/achatar_animate.py --assets "$ASSETS" --character "$cid" \
               --pack "$OUT/$cid.cfz4" --scale 0.5 --bpp 16 >/tmp/achatar_$cid.log 2>&1; then
            printf '%-24s animate  %s\n' "$cid" "$(grep -o 'wrote.*' /tmp/achatar_$cid.log | head -1)"
            ok=$((ok+1))
        else
            echo "$cid: FALHOU (animate) -- veja /tmp/achatar_$cid.log"; fail=$((fail+1))
        fi
    else
        if $PY tools/pacote_personagem.py --assets "$ASSETS" --characters "$cid" \
               --bpp 16 --budget 393216 --out "$OUT" >/tmp/pac_$cid.log 2>&1; then
            printf '%-24s plano    %s\n' "$cid" "$(grep -E '^'"$cid"' ' /tmp/pac_$cid.log | head -1)"
            ok=$((ok+1))
        else
            echo "$cid: FALHOU (plano) -- veja /tmp/pac_$cid.log"; fail=$((fail+1))
        fi
    fi
done
echo
echo "empacotados: $ok   falhas: $fail   em $OUT"
[ "$fail" -eq 0 ]
