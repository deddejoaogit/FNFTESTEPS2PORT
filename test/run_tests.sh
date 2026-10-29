#!/bin/sh
# Offline test suite for the pure-C engine modules.
# These modules have no PS2 dependency on purpose (see each header), so they
# are exercised on the PC against the real data before anything is built for
# the console.
set -e
cd "$(dirname "$0")/.."
CC=${CC:-gcc}
# -Isrc is needed because song_registry.c includes bs_dados.h from src/, and
# -Isrc/engine because it includes its own header and the generated data.
# -Itest/ps2sdk-shim supplies the u8/u16/u32 typedefs the chart loader takes
# from tamtypes.h, so the loader can be tested unmodified on the PC.
FLAGS="-O2 -Wall -Wextra -Werror -std=gnu11 -Isrc -Isrc/engine -Itest/ps2sdk-shim"

# -Werror only catches what the local compiler warns about, and the compilers
# in play here do not agree. glibc marks system() __wur; gcc 12 accepts a cast
# to void in front of it and gcc 13+ does not. So a discarded return value can
# pass the whole suite on one machine and fail the build on another -- which is
# exactly what happened. This grep is compiler-independent: a cast to void in
# front of a call is never right in this codebase, whether or not the local
# gcc notices. It also caught a real one in main.c, where chart_note()'s error
# return was being thrown away and an uninitialised note could be read.
echo "== no discarded return values =="
if grep -rnE '\(void\)[a-zA-Z_][a-zA-Z0-9_]*\(' src test \
        --include=*.c --include=*.h | grep -v ps2sdk-shim; then
    echo
    echo "  A cast to void in front of a call discards the return value."
    echo "  That does not silence warn_unused_result, and on a stricter gcc"
    echo "  the build dies here. Check the value instead."
    exit 1
fi
echo "  ok:   no return value is discarded"

# Verificacao contra os headers REAIS do ps2sdk, fora do console. Roda antes de
# qualquer outra coisa: se um fonte nao compila contra os prototipos verdadeiros,
# nao adianta testar o resto. Falhar aqui aborta a suite.
echo "== compilacao contra os headers reais do ps2sdk =="
./tools/verificar_sdk.sh || exit 1
echo

echo "== vram_cache + io_fila =="
$CC $FLAGS test/test_engine.c src/engine/vram_cache.c src/engine/io_fila.c -o /tmp/fnf_test_engine
/tmp/fnf_test_engine
echo

# Nucleo de gameplay: julgamento, vida, combo e resultado. Cada numero vem de
# Scoring.hx / Constants.hx de v0.8.6, e o teste falha se alguem aproximar.
echo "== gameplay (julgamento, vida, combo, resultado) =="
$CC $FLAGS test/test_gameplay.c src/engine/gameplay.c -o /tmp/fnf_test_gameplay -lm
/tmp/fnf_test_gameplay
echo

# As constantes de julgamento do main.c tem de sair da fonte, nao de numero
# digitado. Esta checagem e' feita por leitura do texto porque o valor vive numa
# macro; o valor em si (160 ms -> 7.680 amostras) esta' preso em
# test/test_gameplay.c, que roda logo acima.
echo "== julgamento derivado de Constants.HIT_WINDOW_MS, sem numero solto =="
if ! grep -q "TUTORIAL_NOTE_LATE ((u32)(FNF_HIT_WINDOW_MS" src/main.c; then
    echo "  FALHA: TUTORIAL_NOTE_LATE nao deriva de FNF_HIT_WINDOW_MS"
    exit 1
fi
if grep -nE "TUTORIAL_NOTE_(LATE|JUDGE_WINDOW)[[:space:]]+[0-9]+u?" src/main.c; then
    echo "  FALHA: a janela voltou a ser um numero digitado"
    exit 1
fi
echo "  ok:   janela de acerto e de nota perdida saem do mesmo 160 ms"
echo

echo "== song_registry =="
$CC $FLAGS test/test_registry.c src/engine/song_registry.c -o /tmp/fnf_test_registry
/tmp/fnf_test_registry
echo

# The committed C must be a rendering of the committed JSON. If someone edits
# song_registry_data.inc by hand, or drops a JSON in without re-running the
# generator, this is where it is caught.
echo "== song_registry is generated from assets-data/ =="
python3 tools/gen_song_registry.py --check
echo

# A tabela de palcos tem de ser a renderizacao do plano commitado, e o plano
# tem de ser o que o empacotador viu nos pixels reais. Sem isto, o C pode
# prometer 15 quadros onde o pacote tem 30 -- e o prop congela no console.
echo "== stage_dados.h is the rendering of the committed stage plan =="
python3 tools/gen_stage_dados.py --check
python3 - <<'PYCHECK'
import json, os, sys
plan = json.load(open("assets-data/stage-props.json"))
bad = 0
nprops = 0
nstages = 0
for st in plan["stages"]:
    nstages += 1
    for p in st["props"]:
        nprops += 1
        if not p["anims"]:
            print("  %s/%s: sem animacao" % (st["id"], p["name"])); bad += 1
            continue
        total = sum(a["count"] for a in p["anims"])
        if total != p["frames"]:
            print("  %s/%s: %d quadros nas animacoes, %d no pacote"
                  % (st["id"], p["name"], total, p["frames"])); bad += 1
        if p["sw"] > 1024 or p["sh"] > 1024:
            print("  %s/%s: %dx%d passa de 1024" % (st["id"], p["name"], p["sw"], p["sh"]))
            bad += 1
        seen = set()
        for a in p["anims"]:
            if a["name"] in seen:
                print("  %s/%s: animacao repetida %r" % (st["id"], p["name"], a["name"]))
                bad += 1
            seen.add(a["name"])
# the same prop name must not appear twice inside one stage, or the pack path
# collides
for st in plan["stages"]:
    names = [p["name"] for p in st["props"]]
    if len(names) != len(set(names)):
        print("  %s: nomes de prop repetidos" % st["id"]); bad += 1
if bad:
    print("  FAIL: %d problema(s) no plano de palcos" % bad)
    sys.exit(1)
print("  ok:   %d palcos, %d props, contagens e limites consistentes"
      % (nstages, nprops))
PYCHECK
echo

echo "== bs_dados.h stage + character tables match the game's own JSON =="
python3 tools/check_bs_dados.py
python3 tools/check_bs_chars.py
python3 tools/vram_budget.py >/dev/null || exit 1
python3 tools/ps2_display.py >/dev/null || exit 1
echo

echo "== stage geometry =="
python3 tools/dump_stage_geometry.py
echo

echo "== chart converter =="
python3 tools/fnf_to_fch3.py --selftest
echo

# The converter's own selftest uses synthetic charts, which is how two bugs
# survived for so long. This converts the REAL charts and loads them with the
# REAL loader, so the two halves of the pipeline are checked against each
# other and against the game's own data.
#
# Both formats are built and both are loaded: v3 is what the PS2 already runs,
# v4 is the lossless one. If they ever disagree about a chart, that is a bug in
# one of them.
echo "== real charts -> FCH3 and FCH4 -> real loader =="
mkdir -p build/charts build/charts-v3
for f in assets-data/charts/*-chart.json; do
    sid=$(basename "$f" -chart.json)
    for diff in easy normal hard; do
        python3 tools/fnf_to_fch3.py \
            --metadata "assets-data/songs/$sid-metadata.json" \
            --chart "$f" \
            --difficulty "$diff" -o "build/charts/$sid-$diff.chart" >/dev/null
        python3 tools/fnf_to_fch3.py --v3 \
            --metadata "assets-data/songs/$sid-metadata.json" \
            --chart "$f" \
            --difficulty "$diff" -o "build/charts-v3/$sid-$diff.chart" >/dev/null
    done
done

echo "-- FCH4 --"
$CC $FLAGS test/test_chart_limits.c src/chart.c src/engine/song_registry.c -o /tmp/fnf_test_chart_limits
/tmp/fnf_test_chart_limits build/charts
echo

echo "-- FCH3 (must stay byte-identical to what the PS2 already loads) --"
/tmp/fnf_test_chart_limits build/charts-v3
echo

# FCH4 must carry every parameter of every event; v3 could not. This is the
# test that would have caught the lossy conversion.
echo "== FCH4 round trip: no parameter is dropped =="
python3 tools/test_fch4.py
echo

# The event module reads the v4 fields the way the original's event classes do,
# checked against the tutorial's own 26 events plus milf's SetCameraBop and
# spaghetti's 15 SetHealthIcon -- the only charts that have them.
echo "== chart_events: FCH4 fields -> the original's semantics =="
$CC $FLAGS test/test_chart_events.c src/chart_events.c src/chart.c src/engine/song_registry.c \
    -o /tmp/fnf_test_chart_events -lm
/tmp/fnf_test_chart_events
echo

$CC $FLAGS test/test_real_chart.c src/chart.c src/engine/song_registry.c \
    -o /tmp/fnf_test_real_chart

# The registry-driven chart path: the same code main.c takes to find a song's
# chart, exercised against a temporary copy of the game's asset layout.
echo "== registry-driven chart paths =="
$CC $FLAGS test/test_chart_paths.c src/chart.c src/engine/song_registry.c \
    -o /tmp/fnf_test_chart_paths
/tmp/fnf_test_chart_paths
echo

for dir in build/charts build/charts-v3; do
    n=0
    for f in $dir/*.chart; do
        base=$(basename "$f" .chart)
        sid=${base%-*}
        diff=${base##*-}
        # the expected note count comes from the JSON itself, not from the test
        want=$(python3 -c "
import json
d = json.load(open('assets-data/charts/$sid-chart.json'))
print(len(d['notes'].get('$diff', [])))")
        if ! /tmp/fnf_test_real_chart "$f" "$want" "$sid" >/dev/null 2>&1; then
            echo "  FAIL: $dir/$base"
            /tmp/fnf_test_real_chart "$f" "$want" "$sid" || true
            exit 1
        fi
        n=$((n+1))
    done
    echo "  $dir: all $n charts load in the real loader"
done
echo

echo
echo "== harness de link + execucao (jogo real + draw real do ps2sdk) =="

# Os assets sinteticos tem de ser validos ANTES de o jogo tentar le-los: um
# asset malformado faria o jogo parar no inicio e o harness passaria sem
# exercitar nada. Este verificador replica cada decodificador do jogo em
# Python e exige que o arquivo consuma exatamente o tamanho previsto e produza
# exatamente o tamanho decodificado.
python3 tools/check_harness_assets.py || {
    echo "  FAIL: os assets sinteticos do harness nao decodificam como o jogo espera"
    exit 1
}

HARNESS_BIN=build/fnf_harness
if [ ! -x "$HARNESS_BIN" ]; then
    echo "  (construindo o harness...)"
    sh tools/run_harness.sh 5 >/dev/null 2>&1 || true
fi
if [ ! -x "$HARNESS_BIN" ]; then
    echo "  SKIP: harness nao construido (precisa do clone do ps2sdk)."
    echo "        Rode 'sh tools/run_harness.sh' para ve-lo em acao."
else
    ROOT=build/harness_assets
    LOG="$ROOT/FNFPS2/FNF_COLHEITA.txt"

    # --- positivo: 1400 quadros, nenhum estouro permitido ------------------
    out=$(PS2H_FRAMES=1400 PS2H_ROOT="$ROOT" PS2H_WATCHDOG=180 \
              PS2H_INPUT="200:CROSS,420:CROSS,600:CROSS,700:CIRCLE,760:CROSS,860:CIRCLE,920:CROSS,1020:CIRCLE,1080:CROSS,1180:CIRCLE,1240:CROSS,1340:CIRCLE" \
              timeout 240 "$HARNESS_BIN" 2>&1) || {
        echo "  FAIL: o harness terminou com codigo de erro em 1400 quadros"
        echo "$out" | tail -30
        exit 1
    }
    echo "$out" | grep -E "^(quadros executados|maior pacote|ESTOUROS|escritas na zona|envios DMA acima|uploads de textura|setup_environment|chamadas de desenho|RESULTADO| *auditoria da faixa| *faixa | *ui_state | *PIOR CASO)" | sed 's/^/    /'
    if ! echo "$out" | grep -q "RESULTADO: PASSOU"; then
        echo "  FAIL: o harness nao passou em 1400 quadros"
        echo "$out" | tail -30
        exit 1
    fi

    # --- cobertura: o jogo TEM de ter chegado ao tutorial -------------------
    # Um teste que passa sem exercitar o codigo arriscado eh um verde falso.
    # O log do proprio jogo registra o quadro de cada troca de estado, entao
    # eh ele que prova a cobertura -- nao uma suposicao.
    reached=""
    for st in 6 7 8; do
        if grep -q "state,$st," "$LOG" 2>/dev/null; then
            reached="$reached $st"
        else
            echo "  FAIL: o jogo nunca entrou no estado $st"
            echo "        (o harness passou, mas sem exercitar menu/story/tutorial)"
            exit 1
        fi
    done
    echo "  cobertura: estados alcancados ->$reached"
    if ! grep -q "tutorial_chart,0," "$LOG" 2>/dev/null; then
        echo "  FAIL: o chart do tutorial nao carregou"
        exit 1
    fi
    # Reentrar no tutorial eh o caminho de free/reload dos pacotes CFZ4, que o
    # proprio codigo comenta ser arriscado. Uma passada so nao exercita isso.
    ncycles=$(grep -c "state,8," "$LOG" 2>/dev/null || echo 0)
    if [ "$ncycles" -lt 3 ]; then
        echo "  FAIL: o tutorial so foi entrado $ncycles vez(es); o ciclo de"
        echo "        free/reload dos pacotes nao foi exercitado"
        exit 1
    fi
    echo "  cobertura: chart carregado, tutorial entrado $ncycles vezes"
    echo "  positivo: OK, 1400 quadros, jogo inteiro, nenhum estouro"

    # --- negativo 1: pacote de 8 quadwords, o estouro TEM de ser pego -----
    # O harness morre com abort() de proposito, e abort() faz o set -e do
    # script cair fora. Entao a secao de testes negativos roda com set +e.
    set +e
    PS2H_FRAMES=1400 PS2H_ROOT="$ROOT" PS2H_QWORDS_CAP=8 PS2H_WATCHDOG=60 \
        timeout 120 "$HARNESS_BIN" > build/harness_neg1.log 2>&1
    rc=$?
    set -e
    out=$(cat build/harness_neg1.log)
    if [ $rc -eq 0 ]; then
        echo "  FAIL: com o pacote em 8 quadwords o harness deveria ter morrido"
        exit 1
    fi
    if ! echo "$out" | grep -q "zona de guarda violada"; then
        echo "  FAIL: morreu, mas nao acusou a zona de guarda"
        echo "$out" | head -20
        exit 1
    fi
    echo "  negativo 1 (pacote de 8 qw): OK, detectou a escrita na zona de guarda"

    # --- negativo 2: DMA acima de QWC_MAX, tambem TEM de ser pego --------
    set +e
    PS2H_FRAMES=1400 PS2H_ROOT="$ROOT" PS2H_FORCE_QWC=70000 PS2H_WATCHDOG=60 \
        timeout 120 "$HARNESS_BIN" > build/harness_neg2.log 2>&1
    rc=$?
    set -e
    out=$(cat build/harness_neg2.log)
    if [ $rc -eq 0 ]; then
        echo "  FAIL: com 70000 quadwords de DMA o harness deveria ter morrido"
        exit 1
    fi
    if ! echo "$out" | grep -q "DMA acima de QWC_MAX"; then
        echo "  FAIL: morreu, mas nao acusou o QWC_MAX"
        echo "$out" | head -20
        exit 1
    fi
    echo "  negativo 2 (DMA com 70000 qw): OK, detectou o excesso sobre QWC_MAX"
fi

echo
# O caminho de ACERTO (julgamento, vida, combo, placar) no jogo real. Separado
# do harness normal de proposito: o roteiro de la' passa ~100 quadros dentro do
# tutorial, tempo de desenhar notas e nunca de uma nota chegar ao receptor, de
# modo que o julgamento nao era executado no PC. Aqui o roteiro aperta durante a
# musica e o teste exige acertos e placar > 0.
echo "== gameplay de ponta a ponta no harness (acerto, placar, combo) =="
if [ -f "${PS2SDK_HEADERS:-/tmp/fnf-ps2sdk-headers}/ee/draw/src/draw.c" ]; then
    # Sem pipe: "sh ... | tail" devolveria o status do tail, e uma falha do
    # jogo passava batido com um ALL SUITES PASSED no fim. Ja' aconteceu.
    # 5800 quadros, e nao 3200: 3200 param no meio da musica e o teste do
    # resultado (tela 9, play_result, rank) nunca chegaria a rodar. O roteiro
    # agora exige tambem que a musica TERMINE -- o Tutorial tem 3.225.600
    # amostras de audio, ou seja ~4.840 quadros so' de reproducao, mais a
    # drenagem do anel e os 90 quadros de espera antes do resultado. Os 5
    # quadros da contagem regressiva (3 s a 100 BPM) tambem entram nessa conta,
    # e 5600 deixava 12 quadros de margem -- margem curta demais para um teste
    # que existe justamente para nao passar por sorte.
    if ! sh tools/run_gameplay_harness.sh 5800 > /tmp/fnf_gameplay.log 2>&1; then
        tail -20 /tmp/fnf_gameplay.log
        echo "  FALHA: o gameplay de ponta a ponta nao passou"
        exit 1
    fi
    tail -6 /tmp/fnf_gameplay.log

    # GAME OVER: o caminho oposto. Aqui o roteiro NAO aperta nada, entao as
    # notas viram MISS, a vida chega a zero e o jogo tem de abrir a tela de
    # game over e voltar a tocar quando o jogador manda reiniciar.
    echo
    echo "== game over no harness (morte, tela, reinicio) =="
    # Sem pipe, pelo mesmo motivo do teste acima.
    if ! sh tools/run_gameover_harness.sh 5000 > /tmp/fnf_gameover.log 2>&1; then
        tail -20 /tmp/fnf_gameover.log
        echo "  FALHA: o game over nao passou"
        exit 1
    fi
    tail -6 /tmp/fnf_gameover.log

    # PAUSA: abrir, congelar (o relogio nao anda), desenhar, e voltar pelos tres
    # caminhos -- Resume do mesmo ponto, Restart Song e Change Difficulty.
    echo
    echo "== pausa no harness (congelar, Resume, Restart, Change Difficulty) =="
    if ! sh tools/run_pause_harness.sh 5800 > /tmp/fnf_pause.log 2>&1; then
        tail -24 /tmp/fnf_pause.log
        echo "  FALHA: a pausa nao passou"
        exit 1
    fi
    tail -6 /tmp/fnf_pause.log

    # FREEPLAY: o item do menu que antes so' dizia "COMING IN NEXT STAGE".
    # Confere lista completa, dificuldade, o chart da musica escolhida e a volta
    # para a Freeplay depois do resultado.
    echo
    echo "== freeplay no harness (lista, dificuldade, tocar, voltar) =="
    if ! sh tools/run_freeplay_harness.sh 6000 > /tmp/fnf_freeplay.log 2>&1; then
        tail -20 /tmp/fnf_freeplay.log
        echo "  FALHA: a freeplay nao passou"
        exit 1
    fi
    tail -6 /tmp/fnf_freeplay.log
else
    echo "  SKIP: precisa do clone do ps2sdk (PS2SDK_HEADERS)"
fi
echo

echo "== harness com ASan + UBSan (todos os buffers, nao so o pacote) =="

# O pacote limitado pega estouro DO PACOTE de desenho. Estes sanitizadores
# pegam estouro, uso apos free e vazamento em TODOS os outros buffers do jogo
# -- a mesma classe de bug, so que em outro lugar. Se o harness comum passa e
# este nao, ha um erro de memoria que o pacote limitado nao ve.
if [ ! -x build/fnf_harness_asan ]; then
    sh tools/run_harness_asan.sh 5 >/dev/null 2>&1 || true
fi
if [ ! -x build/fnf_harness_asan ]; then
    echo "  SKIP: nao consegui construir o harness com sanitizers."
else
    set +e
    PS2H_FRAMES=1400 PS2H_ROOT="$ROOT" \
        PS2H_INPUT="200:CROSS,420:CROSS,600:CROSS,700:CIRCLE,760:CROSS,860:CIRCLE,920:CROSS,1020:CIRCLE,1080:CROSS,1180:CIRCLE,1240:CROSS,1340:CIRCLE" PS2H_WATCHDOG=600 \
        ASAN_OPTIONS=detect_leaks=1:abort_on_error=0:print_stacktrace=1 \
        UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=0 \
        timeout 900 build/fnf_harness_asan > build/harness_asan.log 2>&1
    rc=$?
    set -e
    if [ $rc -ne 0 ]; then
        echo "  FAIL: o harness com sanitizers terminou com codigo $rc"
        grep -E "ERROR|runtime error|SUMMARY|LeakSanitizer" build/harness_asan.log | head -25
        exit 1
    fi
    if grep -qE "ERROR: (Address|Leak)Sanitizer|runtime error:" build/harness_asan.log; then
        echo "  FAIL: os sanitizers acharam erro de memoria ou comportamento indefinido"
        grep -E "ERROR|runtime error|SUMMARY" build/harness_asan.log | head -25
        exit 1
    fi
    grep -E "^(quadros executados|maior pacote|chamadas de desenho)" build/harness_asan.log | sed 's/^/    /'
    echo "  OK: 1401 quadros, jogo inteiro, zero erro de memoria," \
          "zero comportamento indefinido, zero vazamento"
fi

echo
echo "ALL SUITES PASSED"
