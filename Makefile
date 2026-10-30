# O nome tem de ser FNF_PS2.ELF: e' o que src/main.c e build_pendrive.sh
# esperam em mass:/FNFPS2/, e e' o nome que o alvo do make usa (make <nome> so
# funciona quando <nome> e' o proprio EE_BIN).
EE_BIN = FNF_PS2.ELF
EE_OBJS = src/main.o src/title_packs.o src/story_packs.o src/audio_ring.o src/chart.o src/chart_events.o src/engine/song_registry.o src/engine/gameplay.o src/engine/vram_layout.o src/engine/stage_pool.o src/engine/stage_layout.o src/engine/char_layout.o src/engine/char_anim.o src/telemetry.o src/iomanX_irx.o src/fileXio_irx.o src/usbd_irx.o src/usbhdfsd_irx.o src/audsrv_irx.o
EE_LIBS = -ldraw -lgraph -lpacket -ldma -lmath3d -lpad -laudsrv -lfileXio -lpatches
# FNF_VERSION e FNX_EXTRA existem para permitir gerar variantes comparaveis
# sem sobrescrever EE_CFLAGS na linha de comando (isso descartaria o -D_EE
# que o Makefile.pref compoe, e o build quebraria em tamtypes.h).
# Uso:  make FNX_EXTRA=-DFNX_V1_INCLUSIVO FNF_VERSION=0.2.85-v1inclusivo \
#            EE_BIN=fnf_ps2_m85_fix.elf FNX_MAP=fnf_ps2_m85_fix.map
# Sem FNF_VERSION explicito, usa o conteudo do arquivo VERSION (assim o
# COMPILAR.bat do Windows carimba a versao certa sem ninguem exportar nada).
FNF_VERSION ?= $(shell cat VERSION 2>/dev/null || echo 0.2.84-calibracao)
FNX_EXTRA   ?=
FNX_MAP     ?= FNF_PS2.map
EE_CFLAGS = -O2 -G0 -Wall -Wextra -Werror=implicit-function-declaration -std=gnu11 -Isrc -Isrc/engine -DFNF_PS2_VERSION=\"$(FNF_VERSION)\" $(FNX_EXTRA)
EE_LDFLAGS = -Wl,-Map,$(FNX_MAP)
all: $(EE_BIN)
clean:
	rm -f $(EE_BIN) $(EE_OBJS) FNF_PS2.map fnf_ps2_m84.map fnf_ps2_m85_fix.map
include $(PS2SDK)/samples/Makefile.pref
include $(PS2SDK)/samples/Makefile.eeglobal
