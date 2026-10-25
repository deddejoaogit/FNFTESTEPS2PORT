/* O kernel.h do ps2sdk define EE_SYNCL() como __asm__("sync.l") -- sintaxe
   MIPS, que o montador do host nao entende. No harness isso travaria a
   compilacao do main.c real, que eh justamente o que queremos rodar.
   Pegamos o kernel.h verdadeiro (include_next) e so trocamos essa macro:
   todos os prototipos continuam sendo os reais do ps2sdk. */
#ifndef FNF_PS2SHIM_KERNEL_H
#define FNF_PS2SHIM_KERNEL_H
#include_next <kernel.h>
#undef EE_SYNCL
#define EE_SYNCL() do { } while (0)
#endif
