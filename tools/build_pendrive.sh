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
ASSETS=${ASSETS:-/home/user/PS2Projects/google_drive}
ELF=${ELF:-build/FNF_PS2.ELF}

# ==========================================================================
# PRE-REQUISITOS, TODOS DE UMA VEZ
# Antes este script parava no PRIMEIRO arquivo que faltava (`set -e` + exit 1
# dentro do laco), e quem rodava numa maquina nova descobria a lista de
# pendencias uma por execucao -- 15 arquivos de arte, os 46 .chp4, os 21 palcos,
# os 27 stems, o cartao do HUD e o ELF. Agora ele mede TUDO primeiro e diz, de
# uma vez, o que falta e qual comando resolve cada item. Nada e' copiado antes
# disso, entao um pendrive pela metade nao chega a existir.
# ==========================================================================
FALTA_PRE=""
pre_arq() {                       # pre_arq <caminho> <o que e'> <como resolver>
    if [ ! -f "$1" ]; then
        FALTA_PRE="$FALTA_PRE\n  - $2: $1\n      $3"
    fi
}
pre_dir() {
    if [ ! -d "$1" ]; then
        FALTA_PRE="$FALTA_PRE\n  - $2: $1\n      $3"
    fi
}
for f in title_alphabet.rgba title_prompt_l.rgba title_prompt_r.rgba \
         title_logoBumpin.frames title_gfDanceTitle.frames title_newgrounds.rgba \
         font.rgba font.widths menu_sfx.pcm menu_cache.rle \
         bf_frames.lz4f gf_frames.lz4f \
         mainstage_tutorial_parallax.rgba \
         tutorial_hud_official_320x256_gsalpha.rgba \
         audio_stems.pcm freakyMenu_48000.pcm; do
    pre_arq "$ASSETS/$f" "asset de runtime" \
        "aponte ASSETS=<pasta dos assets convertidos> (o LEIA-ME tem o link do Drive)"
done
pre_arq "$ELF" "o binario do jogo" \
    "sh compilar.sh (ou tools/build_pendrive.sh aceita ELF=<caminho>)"

CHARS_SRC=${CHARS_SRC:-build/packs/chars}
CHARTS_SRC=${CHARTS_SRC:-build/charts}
NCH=0
if [ -d "$CHARS_SRC" ]; then
    NCH=$(ls "$CHARS_SRC"/*.chp4 2>/dev/null | wc -l)
fi
if [ "$NCH" -lt 46 ]; then
    FALTA_PRE="$FALTA_PRE\n  - personagens: $NCH de 46 em $CHARS_SRC\n      tools/achatar_tudo.sh <clone funkin.assets> build/packs"
fi
if [ ! -d build/packs/stages ]; then
    FALTA_PRE="$FALTA_PRE\n  - palcos: build/packs/stages nao existe\n      python3 tools/pacotar_palcos.py --out build/packs"
fi
NSTEM=0
if [ -d "${STEMS_SRC:-build/packs/stems}" ]; then
    NSTEM=$(ls "${STEMS_SRC:-build/packs/stems}"/*/stems.pcm 2>/dev/null | wc -l)
fi
if [ "$NSTEM" -lt 27 ]; then
    FALTA_PRE="$FALTA_PRE\n  - audio das musicas: $NSTEM de 27 stems\n      python3 tools/empacotar_stems.py --assets <clone funkin.assets>"
fi
NCUTF=0
CUT_SRC=${STAGES_SRC:-build/packs/stages}/sserafim/cut
for n in main gf bf; do
    for ext in quads cfz4; do
        [ -f "$CUT_SRC/$n.$ext" ] && NCUTF=$((NCUTF+1))
    done
done
# `end1` e `end2` NAO tem .quads: sao UM quadro cada (o `endStuff` do script:
# SEEYOU1 e SEEYOU2). Sem eles o jogo registra `cut_fim_falta` e a sequencia
# do fim aparece sem imagem nenhuma.
for n in end1 end2; do
    [ -f "$CUT_SRC/$n.cfz4" ] && NCUTF=$((NCUTF+1))
done
if [ "$NCUTF" -lt 8 ]; then
    FALTA_PRE="$FALTA_PRE\n  - cutscene do sserafim: $NCUTF de 8 arquivos em $CUT_SRC\n      python3 tools/assar_cutscene_sserafim.py --saida $CUT_SRC"
fi
# O audio das telas (game over, pausa, resultado e os sons do script de palco).
# Nao vem do Drive: e' PCM derivado da arte oficial por `tools/empacotar_audio.py`.
# Antes isto so' falhava DEPOIS de copiar personagens, charts e stems -- agora
# entra na mesma medicao.
GO_SRC=${GO_SRC:-build/packs/audio}
pre_dir "$GO_SRC" "audio das telas (game over/pausa/resultado/palco)" \
    "python3 tools/empacotar_audio.py --assets <clone do funkin.assets>"
NPCM=0
[ -d "$GO_SRC" ] && NPCM=$(ls "$GO_SRC"/*.pcm 2>/dev/null | wc -l)
if [ -d "$GO_SRC" ] && [ "$NPCM" -lt 31 ]; then
    FALTA_PRE="$FALTA_PRE\n  - audio das telas: $NPCM de 31 PCMs em $GO_SRC\n      python3 tools/empacotar_audio.py --assets <clone do funkin.assets>"
fi
NCHART=$(ls "$CHARTS_SRC"/*.chart 2>/dev/null | wc -l)
if [ "$NCHART" -lt 81 ]; then
    FALTA_PRE="$FALTA_PRE\n  - charts: $NCHART de 81 em $CHARTS_SRC\n      sh test/run_tests.sh (gera e valida os 81)"
fi
if [ -n "$FALTA_PRE" ]; then
    echo "== FALTAM PRE-REQUISITOS (nada foi copiado) ==" >&2
    printf "%b\n" "$FALTA_PRE" >&2
    echo >&2
    if [ -e "$OUT" ]; then
        echo "  A pasta $OUT ja' existia de uma rodada anterior e NAO foi tocada." >&2
    fi
    echo "  O pendrive precisa de tudo: os assets de runtime do Drive, os 46" >&2
    echo "  personagens, os 21 palcos (157 props), os 8 arquivos da cutscene, os" >&2
    echo "  81 charts, os 27 stems, o audio das telas (31 PCMs) e o ELF." >&2
    exit 1
fi
echo "== pre-requisitos ok: 16 assets, $NCH personagens, $NSTEM stems, $NCHART charts, $NCUTF arquivos de cutscene, ELF =="

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

echo "== copiando os pedacos de UI (assets/health_bar.rgba) =="
# A barra de vida vem de shared/images/healthBar.png e e' sobreposta a' regiao
# (0,112) do atlas do HUD. Sem este arquivo o jogo desenha a barra do atlas --
# funciona, mas sem a moldura do original (e o log mostra `health_bar_arte`=0).
UI_SRC=${UI_SRC:-build/packs/ui}
if [ -f "$UI_SRC/health_bar.rgba" ]; then
    cp "$UI_SRC/health_bar.rgba" "$OUT/FNFPS2/assets/health_bar.rgba"
    echo "   health_bar.rgba: $(wc -c < "$OUT/FNFPS2/assets/health_bar.rgba") bytes"
else
    echo "   AVISO: $UI_SRC/health_bar.rgba nao existe -- rode tools/empacotar_ui.py" >&2
    echo "          (a barra de vida fica com a arte do atlas)" >&2
fi

# NOTE SPLASH: a regiao do atlas do HUD que faz o brilho do acerto SICK. Vai
# para o mesmo lugar que a barra de vida porque e' o mesmo desenho: o jogo le
# este arquivo e copia para dentro de g_hud_pixels ANTES do upload do tex2.
# Sem ele o jogo toca igual, so' nao aparece o brilho (log: `splash_arte`=0).
if [ -f "$UI_SRC/splash.rgba" ]; then
    cp "$UI_SRC/splash.rgba" "$OUT/FNFPS2/assets/splash.rgba"
    echo "   splash.rgba: $(wc -c < "$OUT/FNFPS2/assets/splash.rgba") bytes (224x124 do atlas do HUD)"
else
    echo "   AVISO: $UI_SRC/splash.rgba nao existe -- rode tools/empacotar_splash.py" >&2
    echo "          (o acerto SICK fica sem brilho)" >&2
fi

# ARTE DA TELA DE RESULTADO: o fundo em gradiente (16x360) e os CFZ4 das tres
# folhas animadas. Fica fora da musica: o jogo os abre quando a tela entra e o
# palco ja' foi solto (o pool tem ~1,2 MB livres ali).
# ATENCAO AO CAMINHO: os empacotadores da tela de Resultado (`--out` de
# empacotar_resultado.py, empacotar_score_digitos.py, empacotar_resultado_rank.py
# e os outros) escrevem em build/packs/resultado, NAO em build/packs/ui (que e'
# so' a barra de vida e o splash do HUD). Ate' agora este bloco procurava em
# $UI_SRC/resultado e nunca achava nada -- o aviso aparecia, a arte ficava de
# fora do pendrive e a tela de Resultado saia so' com texto.
RES_SRC=${RES_SRC:-build/packs/resultado}
if [ -d "$RES_SRC" ]; then
    mkdir -p "$OUT/FNFPS2/assets/resultado"
    cp "$RES_SRC"/* "$OUT/FNFPS2/assets/resultado/"
    echo "   resultado/: $(ls "$RES_SRC" | wc -l) arquivo(s), $(cat "$RES_SRC"/* | wc -c) bytes"
else
    echo "   AVISO: $RES_SRC nao existe -- rode tools/empacotar_resultado.py" >&2
    echo "          (a tela de resultado fica no painel com o texto)" >&2
fi

# ARTE DA TELA DE FREEPLAY: a capsula selecionada, a nao selecionada (o mesmo
# CFZ4 com os dois quadros), o FUNDO (`backingImage`) e os 27 titulos, mais o
# pacote do DJ (`freeplay/freeplay-boyfriend`) que vem em DUAS partes de 8 MB.
# Tudo em assets/freeplay/, que e' o `FP_DIR "freeplay"` do main.c -- sem esta
# copia a tela abre so' com a lista de texto e a telemetria diz
# `freeplay_fundo_falta` / `freeplay_dj_falta`.
FP_SRC=${FP_SRC:-build/packs/freeplay}
if [ -d "$FP_SRC" ]; then
    mkdir -p "$OUT/FNFPS2/assets/freeplay"
    # O `.chp4` do DJ NAO vai para o pendrive: ele e' o mesmo pacote MAIS a
    # tabela de animacoes, e o jogo le' a tabela do `src/engine/freeplay_dj_dados.h`
    # (gerado pelo mesmo empacotador, dos mesmos grupos) e os quadros das PARTES
    # `dj_*.cfz4`. Copiar o `.chp4` seria 12,6 MB de dado morto no pendrive.
    FPB=0
    for f in "$FP_SRC"/*; do
        # O subdiretorio `som/` (os OITO jingles do rank, tools/empacotar_rank_sons.py)
        # e' o unico diretorio aqui: vai inteiro, senao a cinematica de rank fica
        # muda no console (`freeplay_rank_som_falta`).
        if [ -d "$f" ]; then
            cp -r "$f" "$OUT/FNFPS2/assets/freeplay/"
            FPB=$((FPB + $(cat "$f"/* | wc -c)))
            continue
        fi
        [ -f "$f" ] || continue
        case "$(basename "$f")" in
            dj_*.chp4) continue ;;
        esac
        cp "$f" "$OUT/FNFPS2/assets/freeplay/"
        FPB=$((FPB + $(wc -c < "$f")))
    done
    echo "   freeplay/: $(ls "$OUT/FNFPS2/assets/freeplay" | wc -l) arquivo(s), $FPB bytes (sem o .chp4 do DJ)"
else
    echo "   AVISO: $FP_SRC nao existe -- rode tools/empacotar_freeplay.py e" >&2
    echo "          tools/empacotar_freeplay_dj.py (a Freeplay fica so' com texto)" >&2
fi

echo "== copiando o audio da tela de GAME OVER (assets/gameover/) =="
# Os 13 PCMs (musica de game over nas 4 variantes de sufixo, o gameOverEnd de
# cada uma, fnf_loss_sfx nas 4, fakeout_death) e o manifesto AUDIO.txt.
# Sem esta pasta o jogo abre e a tela de game over fica MUDA -- e ate' agora ela
# ficava muda de qualquer jeito, porque o audio nao existia em lugar nenhum do
# repositorio. Vem de tools/empacotar_audio.py (PCM 48 kHz do .ogg do
# funkin.assets), nao do Drive: e' dado derivado da arte oficial.
GO_SRC=${GO_SRC:-build/packs/audio}
if [ ! -d "$GO_SRC" ]; then
    echo "FALTA $GO_SRC -- rode tools/empacotar_audio.py --assets <clone do funkin.assets>" >&2
    exit 1
fi
mkdir -p "$OUT/FNFPS2/assets/gameover" "$OUT/FNFPS2/assets/pause" "$OUT/FNFPS2/assets/result" \
         "$OUT/FNFPS2/assets/stage"
NRES=0
NGO=0
NPAUSA=0
NPALCO=0
for f in "$GO_SRC"/*.pcm; do
    [ -f "$f" ] || continue
    case "$(basename "$f")" in
        # MUSICA DA PAUSA (`breakfast`, `breakfast-pico`) -- vai para
        # assets/pause/, que e' onde PauseSubState procura.
        breakfast*) cp "$f" "$OUT/FNFPS2/assets/pause/"; NPAUSA=$((NPAUSA+1)) ;;
        # A MUSICA E OS SONS DA TELA DE RESULTADO (`res_*`, `scroll_menu`,
        # `confirm_menu`, `tickle_fight`): ResultState procura em
        # `resultScreen/...` no original, mas no port os PCM ficam em
        # assets/result/ (mesma divisao de gameover/ e pause/).
        # OS SONS DO SCRIPT DO PALCO: o trovao do phillyBlazin (`Lightning1..3`,
        # phillyBlazin.hxc L187). Vao para assets/stage/, que e' onde o
        # `palco_som_toca` do main.c procura -- pasta nova porque nao sao som de
        # tela nenhuma, sao evento de palco.
        lightning*|thunder*|train*|doorkick*|startcutscene*|cutend*)
            cp "$f" "$OUT/FNFPS2/assets/stage/"; NPALCO=$((NPALCO+1)) ;;
        res_*|scroll_menu*|confirm_menu*|cancel_menu*|fav.pcm|unfav.pcm|tickle_fight*)
            cp "$f" "$OUT/FNFPS2/assets/result/"; NRES=$((NRES+1)) ;;
        *) cp "$f" "$OUT/FNFPS2/assets/gameover/"; NGO=$((NGO+1)) ;;
    esac
done
echo "   $NPAUSA PCM(s) de pausa em assets/pause/"
echo "   $NRES PCM(s) da tela de resultado em assets/result/"
echo "   $NPALCO PCM(s) de script de palco em assets/stage/"
[ "$NRES" -ge 16 ] || { echo "FALTAM arquivos de audio do resultado em $GO_SRC ($NRES de 16)" >&2; exit 1; }
[ "$NPAUSA" -ge 2 ] || { echo "FALTAM as musicas de pausa em $GO_SRC (breakfast.pcm/breakfast-pico.pcm)" >&2; exit 1; }
[ "$NGO" -ge 13 ] || { echo "FALTAM arquivos de audio do game over em $GO_SRC ($NGO de 13)" >&2; exit 1; }
[ -f "$GO_SRC/AUDIO.txt" ] && cp "$GO_SRC/AUDIO.txt" "$OUT/FNFPS2/assets/gameover/"
echo "   $NGO PCMs + AUDIO.txt, $(du -sh "$OUT/FNFPS2/assets/gameover" | cut -f1)"

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
for f in "$CHARTS_SRC"/*.chart; do
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
echo "   $NCHART charts x 4 layouts = $(( NCHART * 4 )) arquivos"

echo "== copiando o AUDIO das 27 musicas (songs/<id>/stems.pcm) =="
# SEM ISTO SO' O TUTORIAL TEM SOM. O jogo abre `assets/songs/<id>/stems.pcm`
# (`sr_stems_path`) e cai no `assets/audio_stems.pcm` plano -- que e' o audio do
# Tutorial -- quando nao acha o da musica. O pendrive saia sem nenhum dos 27 e
# ninguem notava no harness, onde o PCM e' gerado sinteticamente.
# Formato (o que o mixer le', nao uma escolha): 8 bytes por amostra,
# [InstL, InstR, VoxL, VoxR] int16 little-endian a 48 kHz. Quem gera e'
# `tools/empacotar_stems.py`; o manifesto vai junto para o console poder ser
# conferido sem adivinhar.
STEMS_SRC=${STEMS_SRC:-build/packs/stems}
NSTEM=0
FALTA_STEM=""
for j in assets-data/songs/*-metadata.json; do
    sid=$(basename "$j" -metadata.json)
    if [ -f "$STEMS_SRC/$sid/stems.pcm" ]; then
        cp "$STEMS_SRC/$sid/stems.pcm" "$OUT/FNFPS2/assets/songs/$sid/stems.pcm"
        NSTEM=$((NSTEM+1))
    else
        FALTA_STEM="$FALTA_STEM $sid"
    fi
done
if [ -n "$FALTA_STEM" ]; then
    echo "FALTAM os stems:$FALTA_STEM" >&2
    echo "   rode: python3 tools/empacotar_stems.py --assets <clone do funkin.assets>" >&2
    exit 1
fi
[ -f "$STEMS_SRC/STEMS.txt" ] && cp "$STEMS_SRC/STEMS.txt" "$OUT/FNFPS2/assets/songs/STEMS.txt"
echo "   $NSTEM stems, $(du -sh "$STEMS_SRC" | cut -f1)"

echo "== copiando os 46 personagens (assets/chars/<id>.chp4) =="
# Sem estes arquivos o jogo ABRE e roda, mas nao desenha personagem nenhum:
# cada papel vira `char_sem_pacote` no log. Eles nunca eram copiados aqui, e o
# pendrive saia sem nenhum deles.
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
# O `grep -v '<'` nao e' enfeite: o cabecalho tem, num comentario, o EXEMPLO
# `"stages/<stage>/<name>.cfz4"`, e sem filtrar isso o laco tentava copiar um
# arquivo chamado literalmente `<name>.cfz4`, imprimia
# `FALTA o prop stages/<stage>/<name>.cfz4` e ABORTAVA o pendrive sempre -- os
# 21 palcos nunca chegavam ao cartao. Medido: 158 strings, 157 props de verdade.
for t in $(grep -o '"stages/[^"]*\.cfz4"' src/stage_dados.h | tr -d '"' | grep -v '<' | sort -u); do
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

# A CUTSCENE DO sserafim (assets/stages/sserafim/cut/): os tres `.quads` de
# geometria e os CINCO `.cfz4` de textura (main, gf, bf + `end1`/`end2`, as duas
# imagens de UM quadro do `endStuff`). NAO sao props de `stage_dados.h` -- o
# laco acima so' copia o que o cabecalho do jogo compila --, entao eles tem de
# ser copiados AQUI. Sem eles a `spaghetti` roda, mas o intro de 30 s (a cena
# inteira do script do palco) nao aparece: o jogo registra `cut_falta`.
NCUT=0
for f in main.quads main.cfz4 gf.quads gf.cfz4 bf.quads bf.cfz4 end1.cfz4 end2.cfz4; do
    if [ -f "$STAGES_SRC/sserafim/cut/$f" ]; then
        mkdir -p "$OUT/FNFPS2/assets/stages/sserafim/cut"
        cp "$STAGES_SRC/sserafim/cut/$f" "$OUT/FNFPS2/assets/stages/sserafim/cut/"
        NCUT=$((NCUT+1))
    else
        echo "FALTA a cutscene $STAGES_SRC/sserafim/cut/$f -- rode tools/assar_cutscene_sserafim.py --saida build/packs/stages/sserafim/cut" >&2
        exit 1
    fi
done
echo "   $NCUT arquivo(s) da cutscene em assets/stages/sserafim/cut/"

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
       FNFPS2/assets/songs/<id>/    o chart e o `stems.pcm` de cada musica
                                    (27 stems + STEMS.txt: sem eles so' o
                                    Tutorial tem som)
       FNFPS2/assets/chars/         um .chp4 por personagem (46)
       FNFPS2/assets/stages/        uma pasta por palco, um .cfz4 por prop
       FNFPS2/assets/gameover/      o audio da tela de game over (13 .pcm)
       FNFPS2/assets/pause/          a musica da pausa (breakfast*.pcm)
       FNFPS2/assets/result/         a musica + os 4 sons de menu/resultado
                                     (scrollMenu, confirmMenu, cancelMenu,
                                     tickleFight) e as 12 faixas de musica
       FNFPS2/assets/resultado/      a arte da tela de Resultado (folhas, placar,
                                    rank, a animacao do personagem por rank)
       FNFPS2/assets/freeplay/       as capsulas, o fundo, os titulos e o DJ
                                    (o .chp4 do DJ fica de fora: a tabela vai
                                    compilada no FNF_PS2.ELF)

   O RECORDE fica no MEMORY CARD, nao no pendrive:
       mc0:/FNFPS2/save.dat           1.992 B (placar e tallies por musica e
                                      dificuldade). Sem cartao o jogo roda
                                      igual e o save vale so' na sessao -- o log
                                      diz `save_estado,0,-1`.
   Sem chars/ o jogo abre e roda, mas os personagens nao aparecem. Sem stages/
   os fundos nao aparecem. Sem gameover/ a tela de game over fica sem som
   (musica, o sfx do blue balls e o gameOverEnd do confirmar). Os dois casos deixam evento no log
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
