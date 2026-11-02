#!/bin/sh
# Harness do SAVE e do SELO DE RANK da Freeplay.
#
# Por que existe: o save mora no memory card (`mc0:/FNFPS2/save.dat`) e no PC nao
# ha' cartao, entao toda a suite rodava com "sem cartao" e o selo de rank nunca
# aparecia -- o caminho de GRAVAR, LER de volta e DESENHAR nao tinha prova
# nenhuma. O harness agora mapeia `mc0:/` para `<raiz>/FNFPS2/` quando
# `FNF_CARD=1` (test/ps2harness/ps2h.c, `ps2h_map_path`), o que deixa testar o
# formato, o CRC e o selo de ponta a ponta. O CARTAO DO PS2 (mcInit, IRX,
# sistema de arquivos) continua sendo so' do console: isso esta' escrito no
# PLANO como pendencia.
#
# O que este teste cobra, nesta ordem:
#   1. sem arquivo: o jogo abre, joga uma musica inteira e GRAVA 1.992 B no fim
#      (`save_gravado,1992,0`), sem travar;
#   2. o arquivo em disco: 1.992 B, magic `FNFS`, versao 2, 81 registros, CRC32
#      conferido por uma implementacao INDEPENDENTE (o zlib do Python) -- nao
#      pelo mesmo codigo que escreveu;
#   3. a partida seguinte LE o arquivo (`save_estado,1,1992`) e a Freeplay
#      desenha o selo do rank daquela musica naquela dificuldade
#      (`freeplay_selo_ranks` com pelo menos 1, e `freeplay_selo_desenho`);
#   4. a dificuldade em que o rank NAO existe nao desenha selo nenhum (o selo e'
#      por musica E dificuldade).
#
# Uso: sh tools/run_save_harness.sh
set -u

REPO=$(cd "$(dirname "$0")/.." && pwd)
cd "$REPO" || exit 1

LOG=build/harness_assets/FNFPS2/FNF_COLHEITA.txt
SAVE=build/harness_assets/FNFPS2/save.dat
FALHA=0
falha() { echo "  FALHA: $*"; FALHA=1; }

echo "== save: um cartao comecando vazio =="
rm -rf "$SAVE" build/harness_assets/FNFPS2/sem-cartao

# ---- 1. a partida que grava ------------------------------------------------
echo "== save 1/2: a partida que grava (FNF_CARD=1, arquivo nao existe) =="
FNF_CARD=1 sh tools/run_gameplay_harness.sh 7600 > /tmp/save_p1.log 2>&1 || {
    echo "  FALHA: run_gameplay_harness.sh nao passou"; tail -20 /tmp/save_p1.log; exit 1; }
GRAVOU=$(awk -F, '$1=="E" && $3=="save_gravado" {print $4","$5; exit}' "$LOG")
ESTADO=$(awk -F, '$1=="E" && $3=="save_estado" {print $4","$5; exit}' "$LOG")
echo "  save_estado ................. $ESTADO (esperado 0,-1: ainda sem arquivo)"
echo "  save_gravado ................ $GRAVOU (esperado 1992,0)"
[ "$ESTADO" = "0,-1" ] || falha "o primeiro boot devia dizer 0,-1 (sem arquivo) e disse $ESTADO"
[ "$GRAVOU" = "1992,0" ] || falha "a gravacao devia ser 1992,0 e foi $GRAVOU"
[ -f "$SAVE" ] || falha "$SAVE nao existe depois de gravar"

# ---- 2. o arquivo, conferido por fora --------------------------------------
echo
echo "== save 2/2: o arquivo em disco, conferido pelo zlib (nao pelo codigo que escreveu) =="
if [ -f "$SAVE" ]; then
    python3 - "$SAVE" <<'PY' || FALHA=1
import struct, sys, zlib
b = open(sys.argv[1], "rb").read()
ok = True
def check(nome, cond, extra=""):
    global ok
    print("  %-34s %s%s" % (nome, "ok" if cond else "FALHOU", (" " + extra) if extra else ""))
    if not cond:
        ok = False
check("tamanho 1.992 B", len(b) == 1992, "%d" % len(b))
check("magic FNFS", b[:4] == b"FNFS", repr(b[:4]))
ver, cnt, _res = struct.unpack("<HHI", b[4:12])
check("versao 2", ver == 2, "%d" % ver)
check("81 registros (27 x 3)", cnt == 81, "%d" % cnt)
crc = struct.unpack("<I", b[12:16])[0]
check("CRC32 do corpo (zlib)", (zlib.crc32(b[16:]) & 0xFFFFFFFF) == crc, "%08x" % crc)
validos = [i for i in range(cnt) if b[16 + i * 24 + 22]]
check("exatamente 1 registro valido", len(validos) == 1, str(validos))
if len(validos) == 1:
    i = validos[0]
    o = 16 + i * 24
    score = struct.unpack("<I", b[o:o + 4])[0]
    tallies = struct.unpack("<9H", b[o + 4:o + 22])
    check("jogou a musica 0 na dificuldade 1", i == 1, "indice %d" % i)
    check("placar > 0", score > 0, "%d" % score)
    check("tallies somam as notas", tallies[8] > 0, "totalNotes %d" % tallies[8])
# A secao do FAVORITO (versao 2): 27 bytes no fim do corpo, um por musica.
fav = b[16 + 81 * 24:16 + 81 * 24 + 27]
check("secao do favorito (27 B)", len(fav) == 27, str(len(fav)))
check("nenhum favorito ainda", sum(fav) == 0, str(sum(fav)))
check("corpo v2 = 1944 + 32", len(b) - 16 == 1976, str(len(b) - 16))
sys.exit(0 if ok else 1)
PY
else
    falha "sem $SAVE nao ha' o que conferir"
fi

# ---- 3. a partida que LE e a Freeplay que desenha o selo -------------------
# Roteiro: 200 CROSS (titulo->menu), 640 DOWN (STORY->FREEPLAY), 700 CROSS
# (abre a Freeplay). A Freeplay abre por volta do quadro 790; o titulo das
# capsulas so' aparece em 729 ms (17,5 quadros), entao a janela com rank e
# texto comeca no quadro ~810. Os 1.200 quadros sao para caber isso com folga.
echo
echo "== save: a partida seguinte le o arquivo e o selo aparece =="
ENTRADA="200:CROSS,420:CROSS,640:DOWN,700:CROSS"
# O log e' ABERTO EM ANEXO pelo jogo: sem apagar, as linhas da rodada anterior
# ficam na frente e o `awk` le' o passado (foi o que fez este teste reprovar
# dizendo "o segundo boot nao leu o arquivo").
rm -f "$LOG"
FNF_CARD=1 PS2H_INPUT="$ENTRADA" sh tools/run_harness.sh 1200 > /tmp/save_p2.log 2>&1 || {
    echo "  FALHA: run_harness.sh (leitura) nao passou"; tail -20 /tmp/save_p2.log; exit 1; }
LEU=$(awk -F, '$1=="E" && $3=="save_estado" {print $4","$5; exit}' "$LOG")
RANKS=$(awk -F, '$1=="E" && $3=="freeplay_selo_ranks" {print $4","$5; exit}' "$LOG")
DES=$(awk -F, '$1=="E" && $3=="freeplay_selo_desenho" {print $4; exit}' "$LOG")
SELO=$(awk -F, '$1=="E" && $3=="freeplay_selo" {print $4","$5; exit}' "$LOG")
NDES=$(awk -F, '$1=="E" && $3=="freeplay_selo_desenho" {n++} END{print n+0}' "$LOG")
echo "  save_estado ................. $LEU (esperado 1,1992)"
echo "  freeplay_selo (pacote) ...... $SELO (esperado 1,1104)"
echo "  ranks na lista .............. $RANKS (esperado >=1 na dificuldade 1)"
echo "  selo desenhado .............. rank $DES em $NDES quadro(s)"
[ "$LEU" = "1,1992" ] || falha "o segundo boot devia ler 1,1992 e disse $LEU"
[ "$SELO" = "1,1104" ] || falha "o pacote do selo devia abrir com 1104 B e abriu $SELO"
RCOM=${RANKS%%,*}
[ "${RCOM:-0}" -ge 1 ] || falha "nenhuma musica da lista tinha rank ($RANKS)"
[ "${NDES:-0}" -ge 1 ] || falha "o selo de rank nunca foi desenhado na Freeplay"

# ---- 4. a dificuldade sem rank nao desenha ------------------------------
# O rank gravado e' da dificuldade 1 (NORMAL). RIGHT muda para outra e o selo
# daquela musica tem de sumir: e' a prova de que o selo e' por musica E
# dificuldade, e nao um enfeite fixo.
echo
echo "== save: a dificuldade sem rank nao acende o selo =="
ENTRADA="200:CROSS,420:CROSS,640:DOWN,700:CROSS,900:RIGHT,1300:LEFT"
rm -f "$LOG"
FNF_CARD=1 PS2H_INPUT="$ENTRADA" sh tools/run_harness.sh 1700 > /tmp/save_p3.log 2>&1 || {
    echo "  FALHA: run_harness.sh (dificuldade) nao passou"; tail -20 /tmp/save_p3.log; exit 1; }
RANKS=$(awk -F, '$1=="E" && $3=="freeplay_selo_ranks" {printf "%s:%s ", $4, $5}' "$LOG")
echo "  ranks por dificuldade ....... $RANKS"
# A primeira consulta e' a dificuldade da abertura (1, com rank); a segunda e' a
# de depois do RIGHT (2, sem rank). Sem RIGHT/LEFT de verdade o log teria uma
# linha so' e a conferencia abaixo reprovaria.
printf '%s' "$RANKS" | grep -q "^1:1 " || falha "na abertura (dif 1) devia haver 1 rank: $RANKS"
printf '%s' "$RANKS" | grep -q "0:2 " || falha "depois do RIGHT (dif 2) nao devia haver rank: $RANKS"

echo
if [ "$FALHA" -eq 0 ]; then
    echo "SAVE: PASSOU (gravacao 1.992 B, CRC pelo zlib, leitura na partida"
    echo "      seguinte e o selo de rank desenhado na dificuldade certa)"
    exit 0
fi
echo "SAVE: FALHOU"
exit 1
