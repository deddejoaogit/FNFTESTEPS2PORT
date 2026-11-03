.section .rodata
.align 4
.global g_story_gf_pack_start
.global g_story_gf_pack_end
g_story_gf_pack_start:
.incbin "assets/story_gf_idle.lz4f"
g_story_gf_pack_end:
.align 4
.global g_story_bf_pack_start
.global g_story_bf_pack_end
g_story_bf_pack_start:
.incbin "assets/story_bf_idle.lz4f"
g_story_bf_pack_end:

/* O binario e' DADO, nao codigo: sem esta marca o linker do PC avisa que o
   objeto pede pilha executavel (`missing .note.GNU-stack section implies
   executable stack`). A secao vazia diz "aqui nao ha' codigo". */
.section .note.GNU-stack,"",@progbits
