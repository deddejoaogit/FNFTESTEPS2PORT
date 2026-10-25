#ifndef FNF_PS2SHIM_MALLOC_H
#define FNF_PS2SHIM_MALLOC_H

/* O malloc.h do glibc declara malloc_info(FILE*) sem incluir stdio.h, e um
   header do ps2sdk o inclui antes de qualquer stdio.h. Caminho absoluto de
   proposito: o stdio.h do proprio ps2sdk (libcglue) tambem puxa malloc.h, e um
   include <stdio.h> normal pegaria o dele, reintroduzindo o problema. */
#include "/usr/include/stdio.h"
#include_next <malloc.h>

#endif
