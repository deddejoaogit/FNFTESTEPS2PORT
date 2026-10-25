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
