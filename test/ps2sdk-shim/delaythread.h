/* `DelayThread(us)` no PC: dorme de verdade. Ver test/ps2sdk-shim/kernel.h.
 *
 * O `ps2h.c` do harness troca esta funcao por um relogio virtual, porque ali o
 * tempo tem de andar igual ao de um quadro de 60 Hz. Aqui nao: o teste do anel
 * mede o conteudo do audio, nao o tempo, entao dormir de verdade e' o certo
 * (e' o que faz o produtor ceder a CPU para o consumidor).
 */
#ifndef FNF_SHIM_DELAYTHREAD_H
#define FNF_SHIM_DELAYTHREAD_H

void DelayThread(unsigned int usec);

#endif /* FNF_SHIM_DELAYTHREAD_H */
