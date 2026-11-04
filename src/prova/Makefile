EE_BIN = PROVA_PS2.ELF
EE_OBJS = prova.o ../iomanX_irx.o ../fileXio_irx.o ../usbd_irx.o ../usbhdfsd_irx.o ../audsrv_irx.o
EE_LIBS = -ldraw -lgraph -lpacket -ldma -lpad -laudsrv -lfileXio -lpatches
EE_CFLAGS = -O2 -G0 -Wall -Wextra -std=gnu11 -DFNF_PS2_VERSION=\"prova\"
all: $(EE_BIN)
clean:
	rm -f $(EE_BIN) $(EE_OBJS) PROVA_PS2.map
include $(PS2SDK)/samples/Makefile.pref
include $(PS2SDK)/samples/Makefile.eeglobal
