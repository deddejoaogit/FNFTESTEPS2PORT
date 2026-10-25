# Toolchain ps2dev no Google Drive — manifesto e pipeline de build

## O que está no Drive

A instalação completa do ps2dev (Ubuntu 22.04) está na sua pasta do Drive, em
`$PS2DEV`, com o manifesto `ps2dev-complete-ubuntu22.manifest.txt` ao lado:

```
archive=ps2dev-complete-ubuntu22.tar.gz   (266.104.421 bytes — passa do limite de 100 MB)
source=/usr/local/ps2dev
source_files=5359   source_directories=356   source_symlinks=63
compiler_version=mips64r5900el-ps2-elf-gcc (GCC) 15.2.0
```

O tar.gz não baixa inteiro (o conector do Drive recusa acima de 100 MB), então
a pipeline baixa arquivo por arquivo. O `download_file` escreve direto no disco,
sem passar pelo contexto — é por isso que isso é viável.

## A descoberta que muda o tamanho do ELF

Meu primeiro ELF deu 4.927.396 bytes; o seu dá 5.231.012. A diferença **não é
os headers** — é o link. O `Makefile.eeglobal` do ps2sdk tem:

```
EE_LDFLAGS := -L$(PS2SDK)/ee/lib -Wl,-zmax-page-size=128 $(EE_LDFLAGS)
...
$(EE_CC) -T$(EE_LINKFILE) $(EE_OPTFLAGS) -o $(EE_BIN) $(EE_OBJS) $(EE_LDFLAGS) $(EE_LIBS)
```

com `EE_LINKFILE = $(PS2SDK)/ee/startup/linkfile`. Duas coisas que não se
adivinham e que mudam o ELF em centenas de KB:

1. **O script de linker próprio**, não o default. Ele põe `.text` em `0x00100000`,
   junta tudo num único segmento `PT_LOAD` (o PHDR `text`), e reserva `.spad` em
   `0x70000000` para o scratchpad. `ENTRY(__start)`.
2. **`-Wl,-zmax-page-size=128`**, que alinha os segmentos em 128 bytes em vez de
   4 KB. Num ELF de 5 MB com vários segmentos, isso sozinho vale centenas de KB.

E a ordem dos includes, que também vem do eeglobal:

```
EE_INCS := -I$(PS2SDK)/ee/include -I$(PS2SDK)/common/include -I. $(EE_INCS)
```

`-I.` (a raiz do projeto) entra **antes** dos includes do projeto.

## A armadilha dos nomes repetidos

Vários headers do newlib existem em dois lugares com o mesmo nome e tamanhos
muito diferentes. Baixar todos para uma pasta plana faz um sobrescrever o outro,
e o build quebra de um jeito que parece outro problema:

| arquivo   | raiz de `include/` | `sys/`    | `machine/` |
|-----------|--------------------|-----------|------------|
| stdio.h   | 29.660             | 799       | —          |
| stdlib.h  | 11.867             | —         | 138        |
| string.h  | 5.639              | 116       | —          |
| malloc.h  | 4.166              | —         | 138        |
| time.h    | 7.626              | —         | 103        |
| signal.h  | 774                | 12.424    | —          |
| fcntl.h   | 23                 | 83        | —          |
| types.h   | —                  | 5.414     | 372        |
| limits.h  | 3.888 (newlib)     | —         | —          |
| syslimits.h | 445 (gcc)        | 2.974     | —          |

Por isso a árvore de destino é montada com subdiretórios desde o início
(`tree/mips64r5900el-ps2-elf/include/{sys,machine,bits}`), nunca plana.

## O driver do gcc procura `as` sem prefixo

O gcc acha o assembler pelo nome `as`, não `mips64r5900el-ps2-elf-as`. Sem os
links sem prefixo em `bin/`, ele pega o `as` do x86 do host, que rejeita `-EL`:

```
as: unrecognized option '-EL'
```

Sintoma enganoso: parece erro do assembler, é o assembler errado.

## O linker reclama do LTO mesmo sem usar

`ld: error: could not run lto-wrapper` e depois `cannot execute 'lto1'`. O gcc
passa `-fuse-linker-plugin` por padrão, então o `ld` exige `lto-wrapper`, `lto1`
e `liblto_plugin.so` presentes em `libexec/gcc/.../15.2.0/` e
`lib/bfd-plugins/`.

## Pipeline

```
tools/harness_ps2dev.sh [versao] [elf-de-saida] [dir-de-objetos]
```

Monta `$PS2DEV_HARNESS` (default `/tmp/ps2dev`) a partir de
`google_drive/tree/`, compila os 13 `.c` e os 2 `.S`, e liga com os flags do
eeglobal. O `/tmp` não sobrevive entre chamadas do shell, por isso a árvore é
remontada toda vez — os binários em `google_drive/` é que persistem.

**Nota sobre persistência:** os binários do toolchain somam ~100 MB, e o
snapshot do workspace é limitado a ~128 MB. Eles foram podados uma vez. Se
`google_drive/tree/` estiver vazio, é preciso baixar de novo — o manifesto
acima diz exatamente o quê.

## Verificado

- `mips64r5900el-ps2-elf-gcc` 15.2.0, binutils 2.45.1, ambos rodam nativamente
  no sandbox (x86-64 Linux).
- 13 fontes `.c` + 2 `.S` compilam; 15 objetos.
- ELF linkado: válido, MIPS, executável, entry `0x00109548`.
- Com o linkfile e o `-zmax-page-size=128` corretos, o tamanho fecha com o seu.
