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
mkdir -p "$OUT/FNFPS2/assets/gameover" "$OUT/FNFPS2/assets/pause" "$OUT/FNFPS2/assets/result"
NRES=0
NGO=0
NPAUSA=0
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
        res_*|scroll_menu*|confirm_menu*|cancel_menu*|fav.pcm|unfav.pcm|tickle_fight*)
            cp "$f" "$OUT/FNFPS2/assets/result/"; NRES=$((NRES+1)) ;;
        *) cp "$f" "$OUT/FNFPS2/assets/gameover/"; NGO=$((NGO+1)) ;;
    esac
done
echo "   $NPAUSA PCM(s) de pausa em assets/pause/"
echo "   $NRES PCM(s) da tela de resultado em assets/result/"
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
