/* O ps2sdk tem um stdlib.h de IOP (iop/tcpip) que sombreia o do newlib e, no
   host, o da glibc. Sem este shim, abort/calloc/memalign ficam sem prototipo
   e o harness perderia justamente os avisos de tipo que existe para pegar.

   O <stdint.h> antes eh obrigatorio: o struct random_data do stdlib.h da
   glibc usa int32_t e nao o inclui -- normalmente quem inclui eh outro
   header. Sem isso a compilacao morre com "unknown type name int32_t". */
#ifndef FNF_PS2SHIM_STDLIB_H
#define FNF_PS2SHIM_STDLIB_H
#include <stdint.h>
#include "/usr/include/stdlib.h"
#include_next <stdlib.h>
#endif
