.section .rodata
.balign 16
.global g_title_logo_pack_start
g_title_logo_pack_start:
.incbin "assets/title_logoBumpin.lz4f"
.global g_title_logo_pack_end
g_title_logo_pack_end:
.balign 16
.global g_title_gf_pack_start
g_title_gf_pack_start:
.incbin "assets/title_gfDanceTitle.lz4f"
.global g_title_gf_pack_end
g_title_gf_pack_end:

/* O binario e' DADO, nao codigo: sem esta marca o linker do PC avisa que o
   objeto pede pilha executavel (`missing .note.GNU-stack section implies
   executable stack`). A secao vazia diz "aqui nao ha' codigo". */
.section .note.GNU-stack,"",@progbits
