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

# Mapa de VRAM: o total tem de caber em 4 MiB e cada endereco tem de bater com
# a conta de graph_vram_size() do ps2sdk. Roda antes do resto porque uma
# alocacao que estoura devolve -1 no console e o jogo desenha num endereco
# invalido sem quebrar nada visivel aqui.
echo "== mapa de VRAM (vram_layout) =="
$CC $FLAGS test/test_vram_layout.c src/engine/vram_layout.c -o /tmp/fnf_test_vram
/tmp/fnf_test_vram
echo

# Todo modulo de engine que o main.c chama tem de estar no EE_OBJS do Makefile.
# O harness compila os .c por nome, entao ele liga mesmo que o Makefile esqueca
# um objeto -- e o console nao liga. Aconteceu com vram_layout.o: o jogo passou
# a chamar vl_address() e o link do console teria morrido.
echo "== Makefile: todo modulo usado pelo jogo esta' em EE_OBJS =="
# Fechamento transitivo, e nao so' o que o main.c chama direto: stage_pool.c
# chama vc_* e o vram_cache.o nao estava em EE_OBJS. O link do console teria
# morrido, e a checagem antiga (so' main.c) nao via isso -- o teste do PC liga
# os .c por nome e passava. Agora quem entra no jogo arrasta quem ele chama.
python3 - <<'PYEOF2' || exit 1
import os, re, sys
mk = open("Makefile").read()
m = re.search(r"EE_OBJS\s*=\s*(.+)", mk)
if not m:
    sys.exit("  FALHA: nao achei EE_OBJS no Makefile")
objs = m.group(1).split()
modulos = {}
for nome in sorted(os.listdir("src/engine")):
    if not nome.endswith(".c"):
        continue
    texto = open("src/engine/" + nome).read()
    defs = set(re.findall(r"^[a-zA-Z_][\w \t\*]*?\b([a-z_][a-z0-9_]*)\s*\(", texto, re.M))
    modulos[nome] = (defs, texto)
# estado inicial: o que o main.c chama
main = open("src/main.c").read()
chamadas = set(re.findall(r"\b([a-z_][a-z0-9_]*)\s*\(", main))
em_jogo = set()
mudou = True
while mudou:
    mudou = False
    for nome, (defs, texto) in modulos.items():
        if nome in em_jogo:
            continue
        if defs & (chamadas | set().union(*[modulos[o][0] for o in em_jogo]) if em_jogo else chamadas):
            em_jogo.add(nome)
            mudou = True
faltando = []
for nome in sorted(em_jogo):
    obj = "src/engine/" + nome[:-2] + ".o"
    if obj not in objs:
        faltando.append("  %s entra no jogo (por chamada direta ou indireta) e %s "
                        "nao esta' em EE_OBJS" % (nome, obj))
if faltando:
    print("\n".join(faltando))
    sys.exit(1)
print("  ok:   %d modulos de engine no jogo, todos no link" % len(em_jogo))
PYEOF2
echo

# Orcamento de VRAM e de RAM de cada palco, contra o plano commitado. Sem o
# clone da arte o empacotador nao roda aqui, mas o plano (assets-data/
# stage-props.json) tem sw/sh e frames de cada prop, que e' tudo que a conta
# precisa. Era nao-strict porque 15 dos 21 palcos apareciam acima do orcamento
# contra um modelo de VRAM que o jogo NAO usa. Com o orcamento medido (o
# `vram_livre` do proprio log = 2.056.192 B, o mesmo que test_stage_pool.c
# prova) e com o streaming do pacote (engine/pack_stream.c, que troca "pacote
# inteiro na RAM" por "tabela + 1 bloco"), os 21 passam nos dois orcamentos --
# entao agora e' --strict: deixa de ser relatorio e vira porta.
echo "== orcamento dos palcos (VRAM e RAM) =="
if ! python3 tools/check_stage_budget.py --strict > /tmp/fnf_orcamento.txt; then
    tail -20 /tmp/fnf_orcamento.txt
    echo "  FALHA: orcamento dos palcos (VRAM/RAM)"
    exit 1
fi
grep -E "^(VRAM|RAM|plano conferido)" /tmp/fnf_orcamento.txt | sed "s/^/  /"
echo

# stage_vram_data.h e' gerado do plano; se ficar velho, o pool reserva o tamanho
# errado para um prop e o upload estoura a VRAM -- em silencio.
echo "== stage_vram_data.h confere com o plano =="
python3 tools/gen_stage_vram.py --check
echo

# Pool de texturas do palco: a demanda real dos 21 palcos contra o orcamento de
# VRAM que sobra depois dos framebuffers e das texturas fixas.
echo "== pool de palco (stage_pool) =="
$CC $FLAGS test/test_stage_pool.c src/engine/stage_pool.c src/engine/stage_layout.c \
    src/engine/vram_cache.c -o /tmp/fnf_test_pool
/tmp/fnf_test_pool
echo

# A CUTSCENE do sserafim: o pacote de quadros ASSADOS (o leitor, a geometria e
# os cortes) e a linha do tempo dos ~20 FlxTimers de `sserafim.hxc` L567-930.
# Roda sem libm, como o modulo: o render nao puxa `sqrt`/`sin` do sistema.
echo "== cutscene do sserafim (quadros assados + linha do tempo) =="
$CC $FLAGS test/test_ss_cutscene.c src/engine/ss_cutscene.c -o /tmp/fnf_test_sscut
/tmp/fnf_test_sscut
echo

# Mixer de duas faixas: o `stems.pcm` traz instrumental e vozes em 4 canais
# intercalados e o jogo soma as duas com saturacao. Era nesta conta que a saida
# saia com o dobro do tamanho (audio no dobro da velocidade), entao ela tem
# teste proprio, com os bytes conferidos um por um.
echo "== mixer de duas faixas (audio_mix) =="
$CC $FLAGS test/test_audio_mix.c src/engine/audio_mix.c -o /tmp/fnf_test_audio_mix
/tmp/fnf_test_audio_mix
echo

# Layout dos palcos: quais props aparecem, onde, em que ordem de z. E' a conta
# que o estado 8 do main.c faz a mao para um palco so'; aqui ela roda nos 21 e o
# teste falha se as faixas de props de um palco pegarem o prop de outro.
echo "== palcos (stage_layout) =="
$CC $FLAGS test/test_stage_layout.c src/engine/stage_layout.c src/engine/char_layout.c \
    src/engine/vram_layout.c src/engine/stage_script.c \
    -o /tmp/fnf_test_stage -lm
/tmp/fnf_test_stage
echo

# O SCRIPT DO PALCO: o relampago do phillyBlazin (timers, tweens, animacao do
# `strike`, cor dos personagens). A logica e' pura e nao depende de pacote, entao
# o teste roda em segundos -- e' a unica prova possivel de que o evento dura o
# que o original dura.
echo "== script do palco (relampago) =="
$CC $FLAGS test/test_stage_script.c src/engine/stage_script.c \
    -o /tmp/fnf_test_stage_script -lm
/tmp/fnf_test_stage_script
echo

# Personagens: os 46 do jogo, suas animacoes, e a conta de posicao/camera do
# palco. E' o modulo que substitui as faixas de quadro escritas a mao no estado
# 8 do main.c, entao ele e' conferido contra o bs_dados.h (mesma fonte, outro
# gerador) e contra o foco de camera (888,50 ; 589,00) que a captura do console
# aprovou.
# O decodificador dos pacotes contra o codificador do empacotador. Roda antes
# dos testes que abrem pacote: se o LZ4 estiver errado, todo o resto le' lixo.
echo "== lz4 dos pacotes (decodificador do jogo x codificador do empacotador) =="
python3 tools/lz4py.py
python3 tools/gen_lz4_vetores.py --check
$CC $FLAGS -Itest test/test_lz4.c src/engine/lz4_bloco.c -o /tmp/fnf_test_lz4
/tmp/fnf_test_lz4
echo

echo "== personagens (char_layout) =="
if ! python3 tools/gen_char_dados.py --check; then
    exit 1
fi
$CC $FLAGS test/test_char_layout.c src/engine/char_layout.c -o /tmp/fnf_test_char -lm
/tmp/fnf_test_char
echo

# E qual QUADRO cada um mostra: a taxa de quadro, o loop, o canto voltando a
# dancar no fim do singTime e o danceLeft/danceRight de quem nao tem idle.
echo "== animacao dos personagens (char_anim) =="
$CC $FLAGS test/test_char_anim.c src/engine/char_anim.c src/engine/char_layout.c \
    src/engine/song_registry.c -o /tmp/fnf_test_char_anim -lm
/tmp/fnf_test_char_anim
echo

# Icone de vida do HUD: o leitor do pacote ICP1 (que e' arte de verdade, vinda
# de preload/images/icons) e a conta de estado, posicao e pulo do original
# (HealthIcon.hx L124-L168, L284, L337).
echo "== icone de vida do HUD (health_icon) =="
$CC $FLAGS test/test_health_icon.c src/engine/health_icon.c \
    -o /tmp/fnf_test_health_icon
/tmp/fnf_test_health_icon
echo

# Pacote de personagem (CHP4): o leitor recusa com erro nomeado todo arquivo que
# nao fecha, e um quadro descomprimido sai byte a byte igual ao empacotado.
# NOTE SPLASH: o brilho do acerto SICK. O header C e' a renderizacao do plano
# (como stage_dados.h), a regiao tem de caber na faixa livre do atlas do HUD e
# a animacao tem de durar o que o original dura.
echo "== note splash: dado, regiao e tempo =="
python3 tools/gen_splash_dados.py --check || exit 1
python3 tools/empacotar_splash.py --check || exit 1
$CC $FLAGS test/test_splash.c src/engine/splash.c -o /tmp/fnf_test_splash -lm
/tmp/fnf_test_splash
echo

echo "== tremor de camera (shake) =="
$CC $FLAGS test/test_shake.c src/engine/shake.c -o /tmp/fnf_test_shake -lm
/tmp/fnf_test_shake
echo

echo "== pacote de personagem (char_pack) =="
$CC $FLAGS test/test_char_pack.c src/engine/char_pack.c src/engine/lz4_bloco.c \
    -o /tmp/fnf_test_char_pack
/tmp/fnf_test_char_pack
echo

# A maquina de estado de UM personagem em cena: danca pela batida, canta por
# singTime, erro usa `miss` e cai no nome-base, e o quadro so' e' pedido quando
# muda (que e' o custo: decodificar LZ4 e subir para a VRAM).
echo "== cena: um personagem em cena (char_cena) =="
$CC $FLAGS test/test_char_cena.c src/engine/char_cena.c src/engine/char_pack.c \
    src/engine/char_anim.c src/engine/char_layout.c src/engine/song_registry.c \
    src/engine/lz4_bloco.c -o /tmp/fnf_test_char_cena -lm
/tmp/fnf_test_char_cena
echo

# O que um NOTE KIND faz: o dado lido dos scripts do original (35 kinds) e as
# duas formas de acao -- canto com sufixo e animacao nomeada.
echo "== note kinds: o dado dos scripts e o canto com sufixo =="
$CC $FLAGS test/test_notekind.c src/engine/char_cena.c src/engine/char_pack.c \
    src/engine/char_anim.c src/engine/char_layout.c src/engine/song_registry.c \
    src/engine/lz4_bloco.c src/engine/notekind.c -o /tmp/fnf_test_notekind -lm
/tmp/fnf_test_notekind
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
# O dado dos note kinds cobre todo kind que aparece nos 81 charts? Um kind
# usado numa musica e ausente da tabela passaria calado, e o jogo o trataria
# como "nada a fazer" -- que e' o bug que este dado existe para consertar.
python3 tools/gen_notekinds.py --check
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
# All three formats are built and all three are loaded. v3 is what the PS2
# already runs, v4 is the lossless event format, and v5 (the converter's
# default) adds the note kind per note. If two of them ever disagree about a
# chart, that is a bug in one of them.
echo "== real charts -> FCH5 (default), FCH4 and FCH3 -> real loader =="
mkdir -p build/charts build/charts-v4 build/charts-v3
for f in assets-data/charts/*-chart.json; do
    sid=$(basename "$f" -chart.json)
    for diff in easy normal hard; do
        python3 tools/fnf_to_fch3.py \
            --metadata "assets-data/songs/$sid-metadata.json" \
            --chart "$f" \
            --difficulty "$diff" -o "build/charts/$sid-$diff.chart" >/dev/null
        python3 tools/fnf_to_fch3.py --format v4 \
            --metadata "assets-data/songs/$sid-metadata.json" \
            --chart "$f" \
            --difficulty "$diff" -o "build/charts-v4/$sid-$diff.chart" >/dev/null
        python3 tools/fnf_to_fch3.py --v3 \
            --metadata "assets-data/songs/$sid-metadata.json" \
            --chart "$f" \
            --difficulty "$diff" -o "build/charts-v3/$sid-$diff.chart" >/dev/null
    done
done

echo "-- FCH5 (converter default) --"
$CC $FLAGS test/test_chart_limits.c src/chart.c src/engine/song_registry.c -o /tmp/fnf_test_chart_limits
/tmp/fnf_test_chart_limits build/charts
echo

echo "-- FCH4 (the event format before the note kinds) --"
/tmp/fnf_test_chart_limits build/charts-v4
echo

echo "-- FCH3 (must stay byte-identical to what the PS2 already loads) --"
/tmp/fnf_test_chart_limits build/charts-v3
echo

# `k` used to be dropped by the converter. Now it travels JSON -> FCH5 -> the
# loader, checked note by note against the count in the JSON itself.
echo "== note kinds (\`k\`): JSON -> FCH5 -> loader =="
$CC $FLAGS test/test_note_kinds.c src/chart.c src/engine/song_registry.c \
    -o /tmp/fnf_test_note_kinds
nk=0
for pair in "spookeez:easy:noanim" "spaghetti:easy:non_scoreable" "cocoa:easy:mom" \
            "blazin:easy:weekend-1-taunt" "2hot:easy:weekend-1-firegun"; do
    sid=${pair%%:*}; rest=${pair#*:}; diff=${rest%%:*}; kind=${rest#*:}
    want=$(python3 -c "
import json
d = json.load(open('assets-data/charts/$sid-chart.json'))
print(sum(1 for n in d['notes'].get('$diff', []) if n.get('k') == '$kind'))")
    if ! /tmp/fnf_test_note_kinds "build/charts/$sid-$diff.chart" "$kind" "$want"; then
        echo "  FAIL: $sid/$diff kind $kind (expected $want)"
        exit 1
    fi
    nk=$((nk+1))
done
# A chart with no `k` anywhere must come back with "no kind" on every note: the
# absence of a kind is a state, not an empty string.
/tmp/fnf_test_note_kinds build/charts/tutorial-easy.chart --all-none 0 || exit 1
# The old formats have nowhere to put a kind, so they must answer "none" too.
/tmp/fnf_test_note_kinds build/charts-v4/spookeez-easy.chart --all-none 0 || exit 1
echo "  $nk chart(s) with kinds + 2 without: all correct"
echo

# FCH4 must carry every parameter of every event; v3 could not. This is the
# test that would have caught the lossy conversion.
# O leitor de pacote por quadro: o palco le' do pendrive em vez de manter o
# pacote inteiro na RAM (era o teto de 4 MB que barrava 8 dos 21 palcos).
echo "== pack_stream: pacote lido quadro a quadro, com erro nomeado =="
$CC $FLAGS test/test_pack_stream.c src/engine/pack_stream.c src/engine/lz4_bloco.c \
    -o /tmp/fnf_test_pack_stream
/tmp/fnf_test_pack_stream || exit 1
echo

# O pacote em PARTES: o DJ da Freeplay tem 12,6 MB (678 quadros de 370x375 a
# 16 bpp) e o empacotador corta em arquivos de no maximo 8 MB. Uma parte
# faltando tem de ser erro nomeado, e o quadro na TRAVESSIA de uma parte para a
# outra tem de sair igual ao empacotado.
echo "== pack_multi: pacote CFZ4 em partes (o DJ da Freeplay) =="
$CC $FLAGS test/test_pack_multi.c src/engine/pack_multi.c src/engine/pack_stream.c \
    src/engine/lz4_bloco.c -o /tmp/fnf_test_pack_multi
/tmp/fnf_test_pack_multi || exit 1
echo

# A tela de FREEPLAY: a geometria das capsulas (intendedX/Y), os desvios das
# pontas de `changeSelection`, o lerp de posicao, a entrada em 7 passos de 1/24 s
# e o `textAppear`. Tudo funcao pura de src/engine/freeplay.c.
echo "== freeplay: a geometria e o tempo das capsulas"
$CC $FLAGS test/test_freeplay.c src/engine/freeplay.c -o /tmp/fnf_test_freeplay -lm
/tmp/fnf_test_freeplay || exit 1
echo

# A CINEMATICA DE RANK da Freeplay (volta do resultado com rank novo): os
# quatro instantes dos timers, os zooms das duas cameras com o easing de cada
# tween, o `rankBg`, o `fade`, a capsula no centro e o pouso em `originalPos`,
# o selo 20 -> 0,9, as faiscas de 5 quadros, a vinheta, o `IntervalShake` dos
# dois tremores e dos vizinhos, e o rastro do `fadeAnim`. Funcao pura de
# src/engine/freeplay_rank.c.
echo "== freeplay: a cinematica de rank (a volta do resultado)"
$CC $FLAGS test/test_freeplay_rank.c src/engine/freeplay_rank.c src/engine/freeplay.c -o /tmp/fnf_test_freeplay_rank -lm
/tmp/fnf_test_freeplay_rank || exit 1
echo

# O AUDIO DA FREEPLAY: a janela do `loadPartial` (0 -> 20 % do arquivo, em
# bytes e alinhada a uma amostra de 4 canais), o ganho do `fadeIn(2)` /
# `fadeOut(2)` a 0,7 com o caso degenerado da janela curta, os OITO jingles do
# rank (um por rank, sem queda de `switch`) e a agenda da cinematica (600 /
# 1100 / 2200 ms, sem pular instante num quadro longo). Funcao pura de
# src/engine/freeplay_audio.c.
echo "== freeplay: o audio da tela (preview e jingles do rank)"
$CC $FLAGS test/test_freeplay_audio.c src/engine/freeplay_audio.c \
    src/engine/freeplay_rank.c src/engine/freeplay.c -o /tmp/fnf_test_freeplay_audio -lm
/tmp/fnf_test_freeplay_audio || exit 1
echo

# O ANEL DE AUDIO NO MODO PREVIEW, com a thread de verdade (kernel falso em
# pthread: test/ps2sdk-shim). E' o unico caminho em que o produtor le' um
# TRECHO do arquivo e volta para o comeco do TRECHO -- o `loadPartial` do
# `FreeplayState.hx` L2967-3046 --, joga fora os 4 canais de voz do
# `stems.pcm`, aplica o ganho do `fadeIn(2)`/`fadeOut(2)` e soma os jingles
# DEPOIS. O harness do PC nao chega no fim de uma janela de 5 MB; aqui o
# arquivo e' sintetico e a janela tem 4 s. O teste pegou dois erros reais no
# `src/audio_ring.c`: o ganho era interpolado de uma ponta a outra do bloco de
# 4096 quadros (cortava o bico do `min` em 2 %) e o marcador de posicao da
# janela misturava AMOSTRAS com BYTES (a janela ficava 8x mais longa).
echo "== audio: o anel no preview da Freeplay (a thread de verdade)"
$CC $FLAGS -Itest/ps2sdk-shim test/test_audio_ring.c src/audio_ring.c \
    src/engine/audio_mix.c src/engine/freeplay_audio.c \
    src/engine/freeplay_rank.c src/engine/freeplay.c \
    -o /tmp/fnf_test_audio_ring -lm -lpthread
/tmp/fnf_test_audio_ring || exit 1
echo

# A tela de RESULTADO: as folhas e o relogio delas (res_quadro), o RANK
# (res_flash_alpha / res_flash_alpha_tally / res_flash_cor / res_rank_pisca /
# res_rank_scroll_*), o PLACAR
# (res_score_estado), os contadores, o percentual e a barra do topo
# (res_barra_y / res_dif_y), com pacotes CFZ4 de verdade montados pelo teste.
# Sem esta linha o modulo existia sem teste: `test/test_resultado.c` estava no
# repositorio e nao era compilado por ninguem.
# `-lm`: `res_dif_y` usa `powf` para o `FlxEase.expoOut`, o mesmo caminho que
# `chart_events.c` ja' usa no binario do PS2 (newlib fornece a libm la').
echo "== resultado: folhas, rank, placar, percentual e barra do topo =="
$CC $FLAGS test/test_resultado.c src/engine/resultado.c src/engine/pack_stream.c \
    src/engine/lz4_bloco.c -o /tmp/fnf_test_resultado -lm
/tmp/fnf_test_resultado || exit 1
echo

# A ANIMACAO DO PERSONAGEM POR RANK (`players/<dono>.json` -> `results[rank]`):
# o instante em que cada entrada fica visivel (getBFDelay + delay), o rotulo
# inicial, o loop por rotulo/por quadro e a pausa no ultimo quadro. Tambem
# confere a TABELA gerada: todo rank tem indice, toda entrada cabe no proprio
# pacote e o pior rank cabe nos 350.208 B medidos livres na tela de Resultado.
# O SAVE: o recorde por musica e dificuldade. Sem ele o selo de recorde do
# ResultState nao tem dado (`params.isNewHighscore`), e o port ficava com o
# `else` da fonte para sempre. Aqui se prova a regra do original (primeiro
# resultado E' recorde; empate nao; o save guarda o maior placar com os tallies
# do melhor rank), o formato (ida e volta) e a recusa inteira de arquivo
# corrompido. `gameplay.c` entra porque o rank do registro sai da MESMA funcao
# que julgou a musica (`fnf_play_rank`).
echo "== save: recorde por musica e dificuldade (o dado do selo de recorde) =="
$CC $FLAGS test/test_save.c src/engine/save.c src/engine/gameplay.c \
    src/engine/lz4_bloco.c -o /tmp/fnf_test_save -lm
/tmp/fnf_test_save || exit 1
echo

echo "== resultado: a animacao do personagem por rank =="
$CC $FLAGS test/test_resultado_rank.c src/engine/resultado_rank.c \
    src/engine/pack_multi.c src/engine/pack_stream.c \
    src/engine/lz4_bloco.c -o /tmp/fnf_test_resultado_rank -lm
/tmp/fnf_test_resultado_rank || exit 1
echo

echo "== FCH4 round trip: no parameter is dropped =="
python3 tools/test_fch4.py
echo

# O empacotador CHP4: quadro por animacao, escala no campo 32, dados que faltam
# reportados. Sem PIL/numpy o teste se declara PULADO (e diz por que).
echo "== empacotador CHP4: quadro, escala e dado que falta =="
python3 tools/test_pack_chp4.py || exit 1
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

for dir in build/charts build/charts-v4 build/charts-v3; do
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
python3 tools/check_char_packs.py || {
    echo "  FALHA: os pacotes de personagem gerados nao passaram na conferencia"
    exit 1
}
python3 tools/check_harness_assets.py || {
    echo "  FAIL: os assets sinteticos do harness nao decodificam como o jogo espera"
    exit 1
}

# Os pacotes de VERDADE (46 personagens, 21 palcos) nao estao no repositorio --
# saem de tools/achatar_tudo.sh + tools/pacotar_palcos.py a partir da arte. Se
# estao no disco, o harness roda o jogo inteiro com eles: e' a unica forma de
# provar que empacotador e leitor combinam (os sinteticos tem 64x64 e um prop
# por palco).
if [ -d build/packs/chars ] && [ -d build/packs/stages ]; then
    echo
    echo "== pacotes reais no harness (46 personagens, 21 palcos) =="
    sh tools/run_realpacks_harness.sh 600 || {
        echo "  FALHA: o jogo com os pacotes de verdade nao passou"
        exit 1
    }
else
    echo
    echo "  SKIP dos pacotes reais: build/packs/chars ou build/packs/stages ausente"
    echo "       (rode tools/achatar_tudo.sh e tools/pacotar_palcos.py)"
fi

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
    # 7600 e nao 5800: o nome da musica so' RECIRCULA duas vezes depois de
    # ~27 s de tela de resultado (o ciclo medido e' de 12,87 s), e o gate cobra
    # os dois recomecos. 5800 quadros terminavam antes do primeiro.
    if ! sh tools/run_gameplay_harness.sh 7600 > /tmp/fnf_gameplay.log 2>&1; then
        tail -20 /tmp/fnf_gameplay.log
        echo "  FALHA: o gameplay de ponta a ponta nao passou"
        exit 1
    fi
    tail -6 /tmp/fnf_gameplay.log

    # O RANK DA TELA DE RESULTADO. O roteiro normal chega sempre no GREAT, e
    # metade da tela depende do rank: o par de arte do rankText, os quatro
    # atrasos (getMusicDelay/getFlashDelay/getHighscoreDelay/getBFDelay) e o par
    # musica+intro. Aqui rodam os dois extremos: o LOSS (arte menor, musica em
    # 2/24 s, `resultsSHIT` COM intro) e o PERFECT (arte maior, musica em 95/24
    # s, `resultsPERFECT` SEM intro). A varredura completa dos seis e'
    # tools/run_resultado_rank_harness.sh.
    for rk in 0 4; do
        echo
        echo "== resultado no harness: rank $rk forcado =="
        if ! FNF_RANK=$rk sh tools/run_gameplay_harness.sh 7600 > /tmp/fnf_rank_$rk.log 2>&1; then
            tail -20 /tmp/fnf_rank_$rk.log
            echo "  FALHA: a tela de resultado com o rank $rk forcado nao passou"
            exit 1
        fi
        grep -E "^  (rank na tela|musica do resultado|ticks / audio)" /tmp/fnf_rank_$rk.log
    done

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

    # PRACTICE MODE: a quinta entrada da pausa (`Enable Practice Mode`, com
    # `filter: !isPracticeMode`), a vida podendo zerar sem Game Over
    # (`PlayState.hx` L1255) e a musica seguindo ate' o resultado. A entrada
    # ficou de fora por muito tempo com a desculpa de que "o modo inteiro nao
    # existe"; o que existe no original e' exatamente isto.
    echo
    echo "== practice mode no harness (menu de 5, vida zerada sem morrer) =="
    if ! sh tools/run_practice_harness.sh 5800 > /tmp/fnf_practice.log 2>&1; then
        tail -20 /tmp/fnf_practice.log
        echo "  FALHA: o practice mode nao passou"
        exit 1
    fi
    tail -6 /tmp/fnf_practice.log

    # FREEPLAY: o item do menu que antes so' dizia "COMING IN NEXT STAGE".
    # Confere lista completa, dificuldade, o chart da musica escolhida e a volta
    # para a Freeplay depois do resultado.
    echo
    echo "== freeplay no harness (lista, dificuldade, tocar, voltar) =="
    if ! sh tools/run_freeplay_harness.sh 7000 > /tmp/fnf_freeplay.log 2>&1; then
        tail -20 /tmp/fnf_freeplay.log
        echo "  FALHA: a freeplay nao passou"
        exit 1
    fi
    tail -6 /tmp/fnf_freeplay.log

    # CINEMATICA DE RANK: a volta do resultado com rank pessoal (o
    # `rankAnimStart`/`rankDisplayNew`/`rankAnimSlam` da FreeplayState.hx
    # L1069-1344). As fases na ordem, os zooms, as posicoes da capsula e o fim
    # em 2,2 s. O roteiro e' o mesmo da Freeplay: o `FNF_RANK_ANIM` e'
    # a porta do binario de teste, porque a cinematica NAO roda em practice.
    echo
    echo "== cinematica de rank no harness (fases, zooms, posicoes) =="
    if ! sh tools/run_rank_anim_harness.sh 7000 > /tmp/fnf_rank_anim.log 2>&1; then
        tail -20 /tmp/fnf_rank_anim.log
        echo "  FALHA: a cinematica de rank nao passou"
        exit 1
    fi
    tail -6 /tmp/fnf_rank_anim.log

    # SAVE: gravar no "cartao", ler de volta e ver o selo de rank na Freeplay.
    # O harness mapeia `mc0:/` para um diretorio quando FNF_CARD=1, entao o
    # formato de 1.960 B, o CRC e o caminho de leitura deixam de ser so' codigo
    # revisado: rodam. O CARTAO do PS2 continua so' do console.
    echo
    echo "== save no harness (gravar, conferir o CRC, ler e o selo de rank) =="
    if ! sh tools/run_save_harness.sh > /tmp/fnf_save.log 2>&1; then
        tail -20 /tmp/fnf_save.log
        echo "  FALHA: o save nao passou"
        exit 1
    fi
    tail -4 /tmp/fnf_save.log

    # SWEEP: as 27 musicas x 3 dificuldades, uma por uma. Antes disto o harness
    # provava UMA musica (o Tutorial) e a Freeplay do port so' toca a primeira
    # da lista: 54 dos 81 charts nunca tinham sido carregados pelo jogo. Um
    # caminho de chart errado, um BPM exotico ou um palco que nao desenha so'
    # apareceria no console, com o jogo parado.
    echo
    echo "== sweep: 27 musicas x 3 dificuldades no jogo real =="
    if ! sh tools/run_sweep_harness.sh 1200 > /tmp/fnf_sweep.log 2>&1; then
        grep -E "FALHA|falharam" /tmp/fnf_sweep.log | head -20
        echo "  FALHA: o sweep das 81 combinacoes nao passou"
        exit 1
    fi
    tail -4 /tmp/fnf_sweep.log

    # FCH5: o NOTE KIND (`k`) do JSON tem de chegar ao runtime. O passe
    # principal roda v3 (o container provado no console), onde o campo nao
    # existe; este passe roda o mesmo jogo nos charts que TEM kinds, no
    # formato v5, e cobra do log o numero exato de notas com kind do JSON.
    echo
    echo "== sweep v5: note kinds do JSON chegam ao jogo =="
    if ! FNF_SWEEP_FMT=v5 sh tools/run_sweep_harness.sh 1200 \
            spookeez,spaghetti,cocoa,blazin,2hot,ugh,stress \
            > /tmp/fnf_sweep_v5.log 2>&1; then
        grep -E "FALHA|falharam" /tmp/fnf_sweep_v5.log | head -20
        echo "  FALHA: o passe v5 dos note kinds nao passou"
        exit 1
    fi
    tail -4 /tmp/fnf_sweep_v5.log
    # O passe v5 deixa os charts do harness em FCH5; o Teste de Story e o de
    # sanitizadores abaixo esperam o v3 que o console roda. Volta o formato.
    FNF_SWEEP_FMT=v3 python3 tools/preparar_sweep.py --forcar >/dev/null || exit 1

    # STORY MODE: uma SEMANA INTEIRA. Antes o Story Menu so' tocava a musica 0
    # de cada semana e voltava ao menu -- 20 das 27 musicas existiam e nunca
    # eram alcancadas por ele. O original encadeia as musicas da semana numa
    # playlist e so' mostra o resultado no fim (PlayStatePlaylist.hx /
    # PlayState.hx L3560-3660). Aqui a semana e' jogada de verdade, com o
    # roteiro de teclas calculado do chart de cada musica.
    echo
    echo "== story mode: semanas inteiras em sequencia =="
    for sem in 1 8 9; do
        if ! sh tools/run_story_harness.sh "$sem" 1 12000 > /tmp/fnf_story_$sem.log 2>&1; then
            tail -20 /tmp/fnf_story_$sem.log
            echo "  FALHA: a semana $sem nao passou"
            exit 1
        fi
        grep -E "^  (musicas tocadas|vezes que emendou|telas de resultado)" /tmp/fnf_story_$sem.log
        tail -1 /tmp/fnf_story_$sem.log
    done
else
    echo "  SKIP: precisa do clone do ps2sdk (PS2SDK_HEADERS)"
fi
echo

echo "== harness com ASan + UBSan (todos os buffers, nao so o pacote) =="

# O pacote limitado pega estouro DO PACOTE de desenho. Estes sanitizadores
# pegam estouro, uso apos free e vazamento em TODOS os outros buffers do jogo
# -- a mesma classe de bug, so que em outro lugar. Se o harness comum passa e
# este nao, ha um erro de memoria que o pacote limitado nao ve.
#
# O binario TEM de ser reconstruido quando um fonte muda. Antes so' se
# construia quando faltava, e o suite rodava um binario velho: aconteceu de o
# sweep das 81 acusar "o Tutorial voltou ao caminho antigo" com o codigo novo ja'
# no repositorio -- era o binario de antes, e o suite passava (ou falhava) por um
# motivo que nao existia mais. Comparar a data de cada fonte com a do binario
# custa um segundo e tira essa classe de erro do caminho.
if [ -x build/fnf_harness_asan ]; then
    novos=$(find src test/ps2harness -newer build/fnf_harness_asan \( -name '*.c' -o -name '*.h' \) 2>/dev/null | head -1)
    if [ -n "$novos" ]; then
        echo "  (fonte mais novo que o binario: $novos -- reconstruindo)"
        rm -f build/fnf_harness_asan
    fi
fi
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

    # E os sanitizadores nas 81 combinacoes, nao so' nas telas do tutorial.
    # Cada rodada e' barata (0,4 s), e e' o unico lugar onde um estouro de
    # buffer no carregamento de um chart do week7 apareceria como erro e nao
    # como arte estranha. 600 quadros: contagem + o comeco das notas.
    echo
    echo "== sweep das 81 combinacoes com ASan + UBSan =="
    if ! PS2H_BIN=build/fnf_harness_asan sh tools/run_sweep_harness.sh 600 \
            > /tmp/fnf_sweep_asan.log 2>&1; then
        grep -E "FALHA|falharam|ERROR|runtime error" /tmp/fnf_sweep_asan.log | head -20
        echo "  FALHA: o sweep com sanitizadores nao passou"
        exit 1
    fi
    tail -4 /tmp/fnf_sweep_asan.log
fi

echo
echo "ALL SUITES PASSED"
