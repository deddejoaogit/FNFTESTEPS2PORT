/* O fcntl.h do ps2sdk define os O_* mas nao declara open(); no toolchain real
   isso vem do newlib. No host, o fcntl.h do glibc declara -- e declaracao eh o
   que este harness verifica. */
#include "/usr/include/fcntl.h"
