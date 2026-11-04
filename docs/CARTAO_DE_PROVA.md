# Cartao de prova (gerado por tools/cartao_de_prova.py)

| item | medido | limite | resultado | nota |
|---|---|---|---|---|
| RAM estatica (.text+.data+.rodata+.bss) | 5059544 | 33554432 | OK | _end = 0x005D3558; 4.83 MB |
| heap depois do ELF | 27445928 | 0 | OK | 26.17 MB livres, 17.83 MB de buffers dinamicos |
| RAM total que o jogo ocupa | 23755348 | 33554432 | OK | 22.65 MB de 32 MB; sobra 9.35 MB para pilha/kernel |
| VRAM do mapa fixo | 1810432 | 4194304 | OK | 1.73 MB de 4,00 MB |
| largura maxima de textura | 640 | 1024 | OK | 640 px (teto do GS: 1024) |
| pacote de desenho | 65536 | 0 | OK | 4096 quadwords = 65.536 B; um quadro cabe (o harness recusa estouro) |
| DMA por envio | 4096 | 65535 | OK | 4096 quadwords por envio; teto do DMAC: 65535 |
| escritas bloqueantes por quadro | 0 | 0 | OK | 0 chamadas fixas de open+write+fsync no corpo do quadro |
| ELF32 MIPS para o EE | 32 bits, machine 8 | 32 bits, machine 8 | OK | entry 0x001010C0, 6559104 B |
