#!/bin/sh
# Monta a pasta completa para copiar no pendrive.
#
# Layout que o jogo espera (src/main.c):
#   mass:/FNFPS2/assets/...                -> FNFPS2/assets/... na raiz do pendrive
#   mass:/FNFPS2/assets/songs/<id>/<d>.chart
#   mass:/FNFPS2/FNF_COLHEITA.txt          (criado pelo jogo, nao precisa existir)
#
# Uso: tools/build_pendrive.sh [dir-de-saida]
set -e
REPO=$(cd "$(dirname "$0")/.." && pwd)
cd "$REPO"

OUT=${1:-build/pendrive}
ASSETS=/home/user/PS2Projects/google_drive
ELF=build/FNF_PS2.ELF

rm -rf "$OUT"
mkdir -p "$OUT/FNFPS2/assets/songs"

echo "== copiando os 16 assets de runtime =="
for f in title_alphabet.rgba title_prompt_l.rgba title_prompt_r.rgba \
         title_logoBumpin.frames title_gfDanceTitle.frames title_newgrounds.rgba \
         font.rgba font.widths menu_sfx.pcm menu_cache.rle \
         bf_frames.lz4f gf_frames.lz4f \
         mainstage_tutorial_parallax.rgba \
         tutorial_hud_official_320x256_gsalpha.rgba \
         audio_stems.pcm freakyMenu_48000.pcm; do
    if [ ! -f "$ASSETS/$f" ]; then
        echo "FALTA $f em $ASSETS" >&2
        exit 1
    fi
    cp "$ASSETS/$f" "$OUT/FNFPS2/assets/$f"
done

# menu_cache.rle corrigido.
# A copia do Drive JA eh a corrigida: tools/fix_menu_cache_block4.py reconstruiu
# so o bloco 4 (story titulos, que precisa da fonte VCR) e preservou os blocos
# 1 a 3 byte a byte. O /tmp/mc_fixed.rle que era copiado aqui era de uma
# rodada anterior da ferramenta e ficou velho -- copiar de /tmp entregava o
# bloco 4 antigo no pendrive mesmo com a raiz de assets certa. Agora a fonte
# unica eh a raiz de assets, e ela eh verificada antes de seguir.
if [ ! -f "$ASSETS/menu_cache.rle" ]; then
    echo "FALTA $ASSETS/menu_cache.rle" >&2
    exit 1
fi
WANT=$(python3 - <<'EOF'
import struct
# PRELOAD_RLE_BYTES: o main.c le exatamente esse tanto e ignora o resto, entao
# um arquivo maior nao falha aqui -- falha depois, dentro de story_titles_restore.
print(1587561)
EOF
)
HAVE=$(wc -c < "$ASSETS/menu_cache.rle")
if [ "$HAVE" != "$WANT" ]; then
    echo "   AVISO: menu_cache.rle tem $HAVE bytes, esperado $WANT" >&2
    echo "           (bloco 4 dos story titulos pode estar na versao errada)" >&2
else
    echo "   menu_cache.rle: $HAVE bytes, versao corrigida (bloco 4 story titulos)"
fi

echo "== copiando os charts =="
# O jogo tenta QUATRO caminhos, em ordem (sr_chart_layout_count()):
#   0: songs/<id>/<d>.chart      -- a arvore do jogo original
#   1: songs/<id>-<d>.chart      -- plano, um nivel so
#   2: charts/<id>-<d>.chart     -- plano, outra pasta
#   3: charts/<id>/<d>.chart     -- arvore, outra pasta
# Os planos existem porque num pendrive de verdade o usbhdfsd do PS2 nao
# conseguiu abrir songs/tutorial/normal.chart, mesmo com o arquivo la. O
# harness offline abre o layout 0 sem problema, entao so o console pode dizer
# qual vale -- por isso os quatro vao no pendrive e o jogo escolhe.
for f in build/charts/*.chart; do
    b=$(basename "$f")            # tutorial-easy.chart
    diff=${b##*-}                 # easy.chart
    diff=${diff%.chart}           # easy
    id=${b%-$diff.chart}          # tutorial
    mkdir -p "$OUT/FNFPS2/assets/songs/$id" "$OUT/FNFPS2/assets/charts/$id"
    cp "$f" "$OUT/FNFPS2/assets/songs/$id/$diff.chart"
    cp "$f" "$OUT/FNFPS2/assets/songs/$id-$diff.chart"
    cp "$f" "$OUT/FNFPS2/assets/charts/$id-$diff.chart"
    cp "$f" "$OUT/FNFPS2/assets/charts/$id/$diff.chart"
done
echo "   $(ls build/charts | wc -l) charts x 4 layouts = $(( $(ls build/charts | wc -l) * 4 )) arquivos"

echo "== copiando os 46 personagens (assets/chars/<id>.chp4) =="
# Sem estes arquivos o jogo ABRE e roda, mas nao desenha personagem nenhum:
# cada papel vira `char_sem_pacote` no log. Eles nunca eram copiados aqui, e o
# pendrive saia sem nenhum deles.
CHARS_SRC=${CHARS_SRC:-build/packs/chars}
if [ ! -d "$CHARS_SRC" ]; then
    echo "FALTA $CHARS_SRC -- rode tools/achatar_tudo.sh <clone-funkin.assets> $CHARS_SRC" >&2
    exit 1
fi
mkdir -p "$OUT/FNFPS2/assets/chars"
NCHAR=0
FALTA_CHAR=""
for j in assets-data/characters/*.json; do
    id=$(basename "$j" .json)
    if [ -f "$CHARS_SRC/$id.chp4" ]; then
        cp "$CHARS_SRC/$id.chp4" "$OUT/FNFPS2/assets/chars/$id.chp4"
        NCHAR=$((NCHAR+1))
    else
        FALTA_CHAR="$FALTA_CHAR $id"
    fi
done
if [ -n "$FALTA_CHAR" ]; then
    echo "FALTAM pacotes .chp4:$FALTA_CHAR" >&2
    exit 1
fi
echo "   $NCHAR personagens, $(du -sh "$OUT/FNFPS2/assets/chars" | cut -f1)"

echo "== copiando os 21 palcos (assets/stages/<palco>/<prop>.cfz4) =="
STAGES_SRC=${STAGES_SRC:-build/packs/stages}
if [ ! -d "$STAGES_SRC" ]; then
    echo "FALTA $STAGES_SRC -- rode tools/pacotar_palcos.py --out build/packs" >&2
    exit 1
fi
mkdir -p "$OUT/FNFPS2/assets/stages"
NPROP=0
# A lista de props vem do PROPRIO cabecalho que o jogo compila
# (src/stage_dados.h): o que faltar aqui e' o que faltaria na tela.
for t in $(grep -o '"stages/[^"]*\.cfz4"' src/stage_dados.h | tr -d '"' | sort -u); do
    if [ -f "$STAGES_SRC/${t#stages/}" ]; then
        mkdir -p "$OUT/FNFPS2/assets/$(dirname "$t")"
        cp "$STAGES_SRC/${t#stages/}" "$OUT/FNFPS2/assets/$t"
        NPROP=$((NPROP+1))
    else
        echo "FALTA o prop $t em $STAGES_SRC" >&2
        exit 1
    fi
done
echo "   $NPROP props em $(ls -d "$OUT/FNFPS2/assets/stages"/*/ | wc -l) palcos, $(du -sh "$OUT/FNFPS2/assets/stages" | cut -f1)"

echo "== copiando o ELF =="
if [ ! -f "$ELF" ]; then
    echo "FALTA $ELF -- baixe do Drive (id 17-GBt7YM8fkc4ZBP9Xo5g-ksAy_qN9Jl)" >&2
    exit 1
fi
cp "$ELF" "$OUT/FNF_PS2.ELF"

# Um LEIA-ME na raiz do pendrive. A build 0.2.108 foi dada como "nao abriu" e
# a installacao errada explica isso sozinha: o jogo le mass:/FNFPS2/assets/...,
# entao se a pasta FNFPS2 nao estiver na RAIZ do pendrive, o jogo nao acha asset
# nenhum, dorme para sempre e nao escreve log onde se espera. Este arquivo tira
# essa duvida.
cat > "$OUT/LEIA-ME.txt" <<'FIM'
FNF PS2 -- COMO INSTALAR
========================

1. Descompacte este ZIP DIRETO NA RAIZ DO PENDRIVE.
   Ao terminar, a raiz do pendrive tem de ficar assim:

       FNF_PS2.ELF
       FNFPS2/
       LEIA-ME.txt

   A pasta FNFPS2 tem de estar na RAIZ, nao dentro de outra pasta.
   Se ficar mass:/FNFPS2/FNFPS2/... o jogo NAO abre.

   Dentro dela tem de existir TUDO isto:
       FNFPS2/assets/               os 16 arquivos soltos + songs/ + charts/
       FNFPS2/assets/chars/         um .chp4 por personagem (46)
       FNFPS2/assets/stages/        uma pasta por palco, um .cfz4 por prop
   Sem chars/ o jogo abre e roda, mas os personagens nao aparecem. Sem stages/
   os fundos nao aparecem. Os dois casos deixam evento no log
   (char_sem_pacote / prop_sem_pacote) em vez de tela vazia sem explicacao.

2. Rode FNF_PS2.ELF pelo seu lancador (uLE, OPL, ESR, etc).

3. Para eu poder diagnosticar qualquer coisa so pelo log, mande de volta:

       mass:/FNFPS2/FNF_COLHEITA.txt

   Se esse arquivo nao existir, procure na RAIZ do pendrive:
       mass:/FNF_COLHEITA.txt
   (o jogo tenta os dois lugares; se a pasta FNFPS2 nao estiver
   na raiz, ele escreve na raiz.)

   O log cresce a cada partida -- ele eh somado, nao substituido.
   Para uma colheita limpa, apague o FNF_COLHEITA.txt antes de testar.

O log tem uma linha por sprite por quadro, com a geometria ja recortada.
Com ele da para ver, sem voce descrever nada: sprite fora da faixa visivel,
fundo esticado, item do menu deslocado, e em qual quadro o jogo parou.
FIM

echo
echo "== conteudo =="
find "$OUT" -type f | wc -l
du -sh "$OUT"
echo
echo "pronto em $OUT"
echo "copie o CONTEUDO de $OUT para a raiz do pendrive"
