#!/bin/sh
# Verificacao de compilacao fora do console.
#
# O que este script faz: compila TODOS os fontes com os headers REAIS do
# ps2sdk, usando o gcc do host. Nao gera binario de PS2 -- nao ha toolchain
# aqui -- mas valida contra os prototipos verdadeiros: contagem de argumentos,
# tipos, constantes, declaracoes ausentes.
#
# Por que isso existe: a build que o usuario testou por ultimo nao abria, nao
# tinha som e so mostrava uma textura corrupta no canto. A causa foi uma linha
# minha de depuracao na tela, que despejava dezenas de quads no pacote do
# quadro com a textura errada ligada. Nada disso era visivel sem o console.
# Da proxima vez, o que for detectavel aqui, sera detectado aqui.
#
# Uso:  ./tools/verificar_sdk.sh            # todos os fontes
#        ./tools/verificar_sdk.sh src/main.c # um fonte
#
# Variaveis:
#   PS2SDK_HEADERS  onde guardar/ler os headers do ps2sdk (padrao /tmp)

set -u

HERE=$(cd "$(dirname "$0")/.." && pwd)
cd "$HERE" || exit 1

SDK=${PS2SDK_HEADERS:-/tmp/fnf-ps2sdk-headers}
SHIM="$HERE/tools/ps2shim"

# Os headers do ps2sdk. Baixados se nao existirem: github.com eh alcancavel a
# partir do ambiente de build, e o clone raso eh poucos megabytes.
if [ ! -f "$SDK/common/include/tamtypes.h" ]; then
    echo "== baixando headers do ps2sdk em $SDK =="
    rm -rf "$SDK"
    if ! git clone --depth 1 -q https://github.com/ps2dev/ps2sdk.git "$SDK"; then
        echo "FALHA: nao consegui obter os headers do ps2sdk."
        echo "       Defina PS2SDK_HEADERS para uma copia local do repositorio."
        exit 1
    fi
fi

# _EE diz ao tamtypes.h que estamos no lado EE (o processador principal).
# Sem isso ele aborta com "#error Either _EE or _IOP must be defined!".
#
# Os dois -Wno sao de proposito: o tamtypes.h converte enderecos de 32 bits do
# PS2 em ponteiros, o que o host de 64 bits sempre reclama. Ficaríamos cegos
# para os avisos que importam no meio desse ruido, entao eles saem -- e só eles.
INC=$(find "$SDK" -type d -name include | sed 's|^|-I|' | tr '\n' ' ')
BASE="-fsyntax-only -std=gnu11 -D_EE -I$SHIM $INC -Isrc -Isrc/engine -Wall -Wextra -Wno-int-to-pointer-cast -Wno-pointer-to-int-cast"

if [ $# -gt 0 ]; then
    FILES="$*"
else
    FILES=$(ls src/*.c src/engine/*.c)
fi

fail=0
count=0
for f in $FILES; do
    [ -f "$f" ] || continue
    count=$((count + 1))
    # O erro de sintaxe vem antes; se ainda assim algo vazar, o grep o mostra.
    if ! gcc $BASE "$f" 2>&1 | grep -E "error:|warning:" | sed 's|^|  |'; then
        :
    fi
    # Segundo passo so para o veredito: grep -c nao distingue erro de warning.
    errs=$(gcc $BASE "$f" 2>&1 | grep -c "error:")
    if [ "$errs" -ne 0 ]; then
        echo "FALHA  $f  ($errs erros)"
        fail=1
    else
        echo "ok     $f"
    fi
done

echo
if [ "$fail" -ne 0 ]; then
    echo "VERIFICACAO REPROVOU: $count fontes examinados, ha erros."
    exit 1
fi
echo "VERIFICACAO APROVOU: $count fontes compilam contra os headers reais do ps2sdk."
exit 0
