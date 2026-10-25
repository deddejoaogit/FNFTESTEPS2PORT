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
