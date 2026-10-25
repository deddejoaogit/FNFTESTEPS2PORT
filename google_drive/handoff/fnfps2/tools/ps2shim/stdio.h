/* O libcglue do ps2sdk nao declara snprintf e, quando incluido, esconde as
   declaracoes do newlib que o toolchain real traria. No host, o stdio.h do
   glibc tem os prototipos corretos -- e prototipo correto eh exatamente o que
   este harness existe para verificar. */
#include "/usr/include/stdio.h"
