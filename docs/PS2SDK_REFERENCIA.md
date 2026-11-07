# Referência do ps2sdk — API oficial usada pelo port FNF→PS2

Gerado em 2026-11-07 a partir dos **headers reais instalados** em
`build/tc/ps2dev-ee-min-fnf/ps2sdk` (toolchain `PS2DEV_EE_MIN_FNF.tar.xz`,
gcc 15.2.0, ps2sdk master de 2026). É a documentação COMPLETA de tudo o que o
jogo pode chamar: cada protótipo exacto do cabeçalho onde ele vive. As
semânticas críticas (semaforos, syscall de I/O) estão no capítulo final,
verificadas contra fonte (ps2tek + forum ps2dev + kernel.h oficial).

**Regra de ouro do projeto**: antes de usar qualquer função do ps2sdk, consultar
ESTE ficheiro (ou o header indicado). Se a semântica não estiver no capítulo
final, é SEM DADO — medir no console ou ler o fonte do módulo, nunca assumir
(retorno de `PollSema` ensinou esta lição: ver §Semânticas).

---

## Índice de módulos EE (`ee/include`) e comuns (`common/include`)


### `common/include/aifregs.h`

Macros/constantes: `AIF_INTCL`, `AIF_INTR_ATA0`, `AIF_INTR_PCMCIA`, `AIF_INTR_RTC`, `AIF_REGBASE`, `RTC_24H`, `RTC_AF`, `RTC_AIE`, `RTC_CONTROL`, `RTC_DIV_CTL`, `RTC_DIV_RESET1`, `RTC_DIV_RESET2`, `RTC_DM_BINARY`, `RTC_DST_EN`, `RTC_FREQ_SELECT`, `RTC_INTR_FLAGS`, `RTC_IRQF`, `RTC_PF`, `RTC_PIE`, `RTC_RATE_SELECT`, `RTC_REF_CLCK_1MHZ`, `RTC_REF_CLCK_32KHZ`, `RTC_REF_CLCK_4MHZ`, `RTC_SET`, `RTC_SQWE`, `RTC_UF`, `RTC_UIE`, `RTC_UIP`, `RTC_VALID`, `RTC_VRT`, `USE_AIF_REGS`, `USE_AIF_RTC_REGS`, `__AIFREGS_H__`

### `common/include/as_reg_compat.h`

Macros/constantes: `ABI_EABI64`, `__AS_REG_COMPAT_H__`, `a0`, `a1`, `a2`, `a3`, `a4`, `a5`, `a6`, `a7`, `at`, `fp`, `gp`, `k0`, `k1`, `ra`, `s0`, `s1`, `s2`, `s3`, `s4`, `s5`, `s6`, `s7`, `sp`, `t0`, `t1`, `t2`, `t3`, `t4`, `t5`, `t6`, `t7`, `t8`, `t9`, `v0`, `v1`, `zero`

### `common/include/atahw.h`

Macros/constantes: `ATA_AIF_HDD_BASE`, `ATA_DEV9_HDD_BASE`, `ATA_ERR_ABORT`, `ATA_ERR_ECC`, `ATA_ERR_ICRC`, `ATA_ERR_ID`, `ATA_ERR_MARK`, `ATA_ERR_MC`, `ATA_ERR_MCR`, `ATA_ERR_TRACK0`, `ATA_F_SEC_ENABLED`, `ATA_F_SEC_LOCKED`, `ATA_SEL_LBA`, `ATA_STAT_BUSY`, `ATA_STAT_DRQ`, `ATA_STAT_ECC`, `ATA_STAT_ERR`, `ATA_STAT_INDEX`, `ATA_STAT_READY`, `ATA_STAT_SEEK`, `ATA_STAT_WRERR`, `USE_AIF_ATA_REGS`, `USE_ATA_REGS`, `__ATAHW_H__`, `r_command`, `r_feature`

### `common/include/blkio-ioctl.h`

Macros/constantes: `FDIOC_BLKIO`, `__BLKIO_IOCTL__`

### `common/include/cdvd-ioctl.h`

Macros/constantes: `CDIOC_BREAK`, `CDIOC_DISKRDY`, `CDIOC_FSCACHEDELETE`, `CDIOC_FSCACHEINIT`, `CDIOC_GETDISKTYP`, `CDIOC_GETERROR`, `CDIOC_GETTOC`, `CDIOC_INIT`, `CDIOC_MMODE`, `CDIOC_PAUSE`, `CDIOC_POWEROFF`, `CDIOC_READCLOCK`, `CDIOC_READDVDDUALINFO`, `CDIOC_SETTIMEOUT`, `CDIOC_SPINNOM`, `CDIOC_SPINSTM`, `CDIOC_STANDBY`, `CDIOC_STATUS`, `CDIOC_STOP`, `CDIOC_STREAMINIT`, `CDIOC_TRAYREQ`, `CDIOC_TRYCNT`, `CIOCSTREAMPAUSE`, `CIOCSTREAMRESUME`, `CIOCSTREAMSTAT`, `__CDVD_IOCTL_H__`

### `common/include/dev5_mmio_hwport.h`

Macros/constantes: `USE_DEV5_MMIO_HWPORT`, `__DEV5_MMIO_HWPORT__`

### `common/include/dev9regs.h`

Macros/constantes: `DEV9_DEV9C_9566`, `DEV9_DEV9C_9611`, `DEV9_DMAC_BASE`, `DEV9_DMAC_BCR`, `DEV9_DMAC_CHCR`, `DEV9_DMAC_MADR`, `DEV9_REG`, `DEV9_REGBASE`, `DEV9_R_146C`, `DEV9_R_146E`, `USE_DEV9_REGS`, `__DEV9REGS_H__`

### `common/include/ee_cop0_defs.h`

Macros/constantes: `EE_BPC_BED`, `EE_BPC_DKE`, `EE_BPC_DRB`, `EE_BPC_DRE`, `EE_BPC_DSE`, `EE_BPC_DTE`, `EE_BPC_DUE`, `EE_BPC_DVE`, `EE_BPC_DWB`, `EE_BPC_DWE`, `EE_BPC_DXE`, `EE_BPC_IAB`, `EE_BPC_IAE`, `EE_BPC_IKE`, `EE_BPC_ISE`, `EE_BPC_ITE`, `EE_BPC_IUE`, `EE_BPC_IXE`, `EE_CAUSE_BD`, `EE_CAUSE_BD2`, `EE_CAUSE_SIO`, `EE_COP0_BadPAddr`, `EE_COP0_BadVAddr`, `EE_COP0_Cause`, `EE_COP0_Compare`, `EE_COP0_Config`, `EE_COP0_Context`, `EE_COP0_Count`, `EE_COP0_EPC`, `EE_COP0_EntryHi`, `EE_COP0_EntryLo0`, `EE_COP0_EntryLo1`, `EE_COP0_ErrorEPC`, `EE_COP0_Index`, `EE_COP0_PRId`, `EE_COP0_Random`, `EE_COP0_Status`, `EE_COP0_TagHi`, `EE_COP0_TagLo`, `EE_COP0_Wired`, `EE_EXC2_DBG`, `EE_EXC2_NMI`, `EE_EXC2_PERF`, `EE_EXC2_RST`, `EE_STATUS_ERL`, `EE_STATUS_EXL`, `EE_STATUS_IE`, `M_EE_GET_CAUSE_EXC2`, `M_EE_GET_CAUSE_EXCODE`, `__EE_COP0_DEFS_H__`

### `common/include/ee_regs.h`

Macros/constantes: `A_EE_D0_ASR0`, `A_EE_D0_ASR1`, `A_EE_D0_CHCR`, `A_EE_D0_MADR`, `A_EE_D0_QWC`, `A_EE_D0_TADR`, `A_EE_D1_ASR0`, `A_EE_D1_ASR1`, `A_EE_D1_CHCR`, `A_EE_D1_MADR`, `A_EE_D1_QWC`, `A_EE_D1_TADR`, `A_EE_D2_ASR0`, `A_EE_D2_ASR1`, `A_EE_D2_CHCR`, `A_EE_D2_MADR`, `A_EE_D2_QWC`, `A_EE_D2_TADR`, `A_EE_D3_CHCR`, `A_EE_D3_MADR`, `A_EE_D3_QWC`, `A_EE_D4_CHCR`, `A_EE_D4_MADR`, `A_EE_D4_QWC`, `A_EE_D4_TADR`, `A_EE_D5_CHCR`, `A_EE_D5_MADR`, `A_EE_D5_QWC`, `A_EE_D6_CHCR`, `A_EE_D6_MADR`, `A_EE_D6_QWC`, `A_EE_D6_TADR`, `A_EE_D7_CHCR`, `A_EE_D7_MADR`, `A_EE_D7_QWC`, `A_EE_D8_CHCR`, `A_EE_D8_MADR`, `A_EE_D8_QWC`, `A_EE_D8_SADR`, `A_EE_D9_CHCR`, `A_EE_D9_MADR`, `A_EE_D9_QWC`, `A_EE_D9_SADR`, `A_EE_D9_TADR`, `A_EE_D_CTRL`, `A_EE_D_ENABLER`, `A_EE_D_ENABLEW`, `A_EE_D_PCR`, `A_EE_D_RBOR`, `A_EE_D_RBSR`, `A_EE_D_SQWC`, `A_EE_D_STADR`, `A_EE_D_STAT`, `A_EE_GIF_CNT`, `A_EE_GIF_CTRL`, `A_EE_GIF_FIFO`, `A_EE_GIF_MODE`, `A_EE_GIF_P3CNT`, `A_EE_GIF_P3TAG`, `A_EE_GIF_STAT` …

### `common/include/errno.h`

Macros/constantes: `E2BIG`, `EACCES`, `EADDRINUSE`, `EADDRNOTAVAIL`, `EADV`, `EAFNOSUPPORT`, `EAGAIN`, `EALREADY`, `EBADE`, `EBADF`, `EBADFD`, `EBADMSG`, `EBADR`, `EBADRQC`, `EBADSLT`, `EBFONT`, `EBUSY`, `ECANCELED`, `ECASECLASH`, `ECHILD`, `ECHRNG`, `ECOMM`, `ECONNABORTED`, `ECONNREFUSED`, `ECONNRESET`, `EDEADLK`, `EDEADLOCK`, `EDESTADDRREQ`, `EDOM`, `EDOTDOT`, `EDQUOT`, `EEXIST`, `EFAULT`, `EFBIG`, `EFORMAT`, `EFTYPE`, `EHOSTDOWN`, `EHOSTUNREACH`, `EIDRM`, `EILSEQ`, `EINPROGRESS`, `EINTR`, `EINVAL`, `EIO`, `EISCONN`, `EISDIR`, `EL2HLT`, `EL2NSYNC`, `EL3HLT`, `EL3RST`, `ELBIN`, `ELIBACC`, `ELIBBAD`, `ELIBEXEC`, `ELIBMAX`, `ELIBSCN`, `ELNRNG`, `ELOOP`, `EMFILE`, `EMLINK` …

### `common/include/fileXio.h`

Macros/constantes: `CTL_BUF_SIZE`, `FILEXIO_DIRFLAGS_DIR`, `FILEXIO_DIRFLAGS_FILE`, `FILEXIO_DT_BLOCK`, `FILEXIO_DT_CHAR`, `FILEXIO_DT_CONS`, `FILEXIO_DT_FS`, `FILEXIO_DT_FSEXT`, `FILEXIO_DT_RAW`, `FILEXIO_IRX`, `FILEXIO_MAX_DEVICES`, `FILEXIO_MOUNTFLAG_NORMAL`, `FILEXIO_MOUNTFLAG_READONLY`, `FILEXIO_MOUNTFLAG_ROBUST`, `IOCTL_BUF_SIZE`, `__FILEXIO_H__`

### `common/include/fileio-common.h`

Macros/constantes: `FIO_PATH_MAX`, `__FILEIO_COMMON_H__`

### `common/include/gif_registers.h`

Macros/constantes: `GIF_REG_CNT`, `GIF_REG_CTRL`, `GIF_REG_MODE`, `GIF_REG_P3CNT`, `GIF_REG_P3TAG`, `GIF_REG_STAT`, `GIF_REG_TAG0`, `GIF_REG_TAG1`, `GIF_REG_TAG2`, `GIF_REG_TAG3`, `GIF_SET_CTRL`, `GIF_SET_MODE`, `__GIF_REGISTERS_H__`

### `common/include/gif_tags.h`

Macros/constantes: `GIF_BLOCK_SIZE`, `GIF_FLG_IMAGE`, `GIF_FLG_PACKED`, `GIF_FLG_REGLIST`, `GIF_PRE_DISABLE`, `GIF_PRE_ENABLE`, `GIF_PRIM_LINE`, `GIF_PRIM_LINE_STRIP`, `GIF_PRIM_POINT`, `GIF_PRIM_SPRITE`, `GIF_PRIM_TRIANGLE`, `GIF_PRIM_TRIANGLE_FAN`, `GIF_PRIM_TRIANGLE_STRIP`, `GIF_REG_AD`, `GIF_REG_CLAMP`, `GIF_REG_CLAMP_1`, `GIF_REG_CLAMP_2`, `GIF_REG_FOG`, `GIF_REG_NOP`, `GIF_REG_PRIM`, `GIF_REG_RGBAQ`, `GIF_REG_ST`, `GIF_REG_TEX0`, `GIF_REG_TEX0_1`, `GIF_REG_TEX0_2`, `GIF_REG_UV`, `GIF_REG_XYZ2`, `GIF_REG_XYZ3`, `GIF_REG_XYZF2`, `GIF_REG_XYZF3`, `GIF_SET_CLAMP`, `GIF_SET_FOG`, `GIF_SET_PRIM`, `GIF_SET_RGBAQ`, `GIF_SET_ST`, `GIF_SET_TAG`, `GIF_SET_TEX0`, `GIF_SET_UV`, `GIF_SET_XYZ`, `GIF_SET_XYZF`, `PACK_GIFTAG`, `__GIF_TAGS_H__`

### `common/include/gs_gp.h`

Macros/constantes: `GS_DISABLE`, `GS_ENABLE`, `GS_PRIM_LINE`, `GS_PRIM_LINE_STRIP`, `GS_PRIM_POINT`, `GS_PRIM_SPRITE`, `GS_PRIM_TRIANGLE`, `GS_PRIM_TRIANGLE_FAN`, `GS_PRIM_TRIANGLE_STRIP`, `GS_REG_ALPHA`, `GS_REG_ALPHA_1`, `GS_REG_ALPHA_2`, `GS_REG_BITBLTBUF`, `GS_REG_CLAMP`, `GS_REG_CLAMP_1`, `GS_REG_CLAMP_2`, `GS_REG_COLCLAMP`, `GS_REG_DIMX`, `GS_REG_DTHE`, `GS_REG_FBA`, `GS_REG_FBA_1`, `GS_REG_FBA_2`, `GS_REG_FINISH`, `GS_REG_FOG`, `GS_REG_FOGCOL`, `GS_REG_FRAME`, `GS_REG_FRAME_1`, `GS_REG_FRAME_2`, `GS_REG_HWREG`, `GS_REG_LABEL`, `GS_REG_MIPTBP1`, `GS_REG_MIPTBP1_1`, `GS_REG_MIPTBP1_2`, `GS_REG_MIPTBP2`, `GS_REG_MIPTBP2_1`, `GS_REG_MIPTBP2_2`, `GS_REG_NOP`, `GS_REG_PABE`, `GS_REG_PRIM`, `GS_REG_PRMODE`, `GS_REG_PRMODECONT`, `GS_REG_RGBAQ`, `GS_REG_SCANMSK`, `GS_REG_SCISSOR`, `GS_REG_SCISSOR_1`, `GS_REG_SCISSOR_2`, `GS_REG_SIGNAL`, `GS_REG_ST`, `GS_REG_TEST`, `GS_REG_TEST_1`, `GS_REG_TEST_2`, `GS_REG_TEX0`, `GS_REG_TEX0_1`, `GS_REG_TEX0_2`, `GS_REG_TEX1`, `GS_REG_TEX1_1`, `GS_REG_TEX1_2`, `GS_REG_TEX2`, `GS_REG_TEX2_1`, `GS_REG_TEX2_2` …

### `common/include/gs_privileged.h`

Macros/constantes: `GS_REG_BGCOLOR`, `GS_REG_BUSDIR`, `GS_REG_CSR`, `GS_REG_DISPFB1`, `GS_REG_DISPFB2`, `GS_REG_DISPLAY1`, `GS_REG_DISPLAY2`, `GS_REG_EXTBUF`, `GS_REG_EXTDATA`, `GS_REG_EXTWRITE`, `GS_REG_IMR`, `GS_REG_PMODE`, `GS_REG_SIGLBLID`, `GS_REG_SMODE1`, `GS_REG_SMODE2`, `GS_REG_SRFSH`, `GS_REG_SYNCH1`, `GS_REG_SYNCH2`, `GS_REG_SYNCHV`, `GS_SET_BGCOLOR`, `GS_SET_BUSDIR`, `GS_SET_CSR`, `GS_SET_DISPFB`, `GS_SET_DISPLAY`, `GS_SET_EXTBUF`, `GS_SET_EXTDATA`, `GS_SET_EXTWRITE`, `GS_SET_IMR`, `GS_SET_PMODE`, `GS_SET_PMODE_EXT`, `GS_SET_SIGLBLID`, `GS_SET_SMODE1`, `GS_SET_SMODE2`, `GS_SET_SRFSH`, `GS_SET_SYNCH1`, `GS_SET_SYNCH2`, `GS_SET_SYNCHV`, `__GS_PRIVILEGED_H__`

### `common/include/gs_psm.h`

Macros/constantes: `GS_PSMZ_16`, `GS_PSMZ_16S`, `GS_PSMZ_24`, `GS_PSMZ_32`, `GS_PSM_16`, `GS_PSM_16S`, `GS_PSM_24`, `GS_PSM_32`, `GS_PSM_4`, `GS_PSM_4HH`, `GS_PSM_4HL`, `GS_PSM_8`, `GS_PSM_8H`, `GS_PSM_PS24`, `GS_ZBUF_16`, `GS_ZBUF_16S`, `GS_ZBUF_24`, `GS_ZBUF_32`, `__GS_PSM_H__`

### `common/include/hdd-ioctl.h`

Macros/constantes: `APA_DEVCTL_ATA_READ`, `APA_DEVCTL_ATA_WRITE`, `APA_DEVCTL_DEV9_SHUTDOWN`, `APA_DEVCTL_FLUSH_CACHE`, `APA_DEVCTL_FORMAT`, `APA_DEVCTL_FREE_SECTORS`, `APA_DEVCTL_GETTIME`, `APA_DEVCTL_GET_ERROR_PART_NAME`, `APA_DEVCTL_GET_SECTOR_ERROR`, `APA_DEVCTL_IDLE`, `APA_DEVCTL_MAX_SECTORS`, `APA_DEVCTL_SCE_IDENTIFY_DRIVE`, `APA_DEVCTL_SET_OSDMBR`, `APA_DEVCTL_SMART_STAT`, `APA_DEVCTL_STATUS`, `APA_DEVCTL_SWAP_TMP`, `APA_DEVCTL_TOTAL_SECTORS`, `APA_FLAG_SUB`, `APA_IDMAX`, `APA_IOCTL2_ADD_SUB`, `APA_IOCTL2_DELETE_LAST_SUB`, `APA_IOCTL2_FLUSH_CACHE`, `APA_IOCTL2_GETSIZE`, `APA_IOCTL2_GET_PART_ERROR`, `APA_IOCTL2_NUMBER_OF_SUBS`, `APA_IOCTL2_SET_PART_ERROR`, `APA_IOCTL2_TRANSFER_DATA`, `APA_IO_MODE_READ`, `APA_IO_MODE_WRITE`, `APA_MAXSUB`, `APA_PASSMAX`, `APA_TYPE_CFS`, `APA_TYPE_EXT2`, `APA_TYPE_EXT2SWAP`, `APA_TYPE_FREE`, `APA_TYPE_HDL`, `APA_TYPE_MBR`, `APA_TYPE_PFS`, `APA_TYPE_REISER`, `DDIOC_LED2CTL`, `DDIOC_MODEL`, `DDIOC_OFF`, `DDIOC_SETPIO3`, `DEV9_TYPE_EXPBAY`, `DEV9_TYPE_PCMCIA`, `HDDCTL_DEV9_SHUTDOWN`, `HDDCTL_FLUSH_CACHE`, `HDDCTL_FORMAT`, `HDDCTL_FREE_SECTORS`, `HDDCTL_IDLE`, `HDDCTL_MAX_SECTORS`, `HDDCTL_SMART_STAT`, `HDDCTL_STATUS`, `HDDCTL_SWAP_TMP`, `HDDCTL_TOTAL_SECTORS`, `HDDIO_ADD_SUB`, `HDDIO_DELETE_END_SUB`, `HDDIO_FLUSH_CACHE`, `HDDIO_GETSIZE`, `HDDIO_NUMBER_OF_SUBS` …

### `common/include/io_common.h`

Macros/constantes: `FIO_MT_RDONLY`, `FIO_MT_RDWR`, `FIO_O_APPEND`, `FIO_O_CREAT`, `FIO_O_DIROPEN`, `FIO_O_EXCL`, `FIO_O_NBLOCK`, `FIO_O_NOWAIT`, `FIO_O_RDONLY`, `FIO_O_RDWR`, `FIO_O_TRUNC`, `FIO_O_WRONLY`, `FIO_SEEK_CUR`, `FIO_SEEK_END`, `FIO_SEEK_SET`, `__IO_COMMON_H__`

### `common/include/iop_cop0_defs.h`

Macros/constantes: `IOP_CAUSE_BD`, `IOP_COP0_BADVADDR`, `IOP_COP0_BDA`, `IOP_COP0_BDAM`, `IOP_COP0_BPC`, `IOP_COP0_BPCM`, `IOP_COP0_DCIC`, `IOP_COP0_STATUS`, `IOP_DCIC_D`, `IOP_DCIC_DA`, `IOP_DCIC_DAE`, `IOP_DCIC_DB`, `IOP_DCIC_DE`, `IOP_DCIC_DL`, `IOP_DCIC_DR`, `IOP_DCIC_DW`, `IOP_DCIC_I`, `IOP_DCIC_IL`, `IOP_DCIC_KD`, `IOP_DCIC_PC`, `IOP_DCIC_PCE`, `IOP_DCIC_R`, `IOP_DCIC_T`, `IOP_DCIC_TE`, `IOP_DCIC_TR`, `IOP_DCIC_UD`, `IOP_DCIC_W`, `M_IOP_GET_CAUSE_EXCODE`, `__IOP_COP0_DEFS_H__`

### `common/include/iop_low_memory_globals.h`

Macros/constantes: `USE_IOP_LOW_MEMORY_GLOBALS`, `__IOP_LOW_MEMORY_GLOBALS__`

### `common/include/iop_mmio_hwport.h`

Macros/constantes: `USE_IOP_MMIO_HWPORT`, `__IOP_MMIO_HWPORT__`

### `common/include/iop_regs.h`

Macros/constantes: `A_IOP_BF80146E`, `A_IOP_BF801470`, `A_IOP_BF801472`, `A_IOP_BF803200`, `A_IOP_BF803204`, `A_IOP_BF803218`, `A_IOP_BF808400`, `A_IOP_BF808414`, `A_IOP_BF808420`, `A_IOP_BF808428`, `A_IOP_BF808430`, `A_IOP_BF80844C`, `A_IOP_BF80847C`, `A_IOP_D0_BCR`, `A_IOP_D0_CHCR`, `A_IOP_D0_MADR`, `A_IOP_D10_BCR`, `A_IOP_D10_CHCR`, `A_IOP_D10_MADR`, `A_IOP_D11_BCR`, `A_IOP_D11_CHCR`, `A_IOP_D11_MADR`, `A_IOP_D12_BCR`, `A_IOP_D12_CHCR`, `A_IOP_D12_MADR`, `A_IOP_D1_BCR`, `A_IOP_D1_CHCR`, `A_IOP_D1_MADR`, `A_IOP_D2_BCR`, `A_IOP_D2_CHCR`, `A_IOP_D2_MADR`, `A_IOP_D3_BCR`, `A_IOP_D3_CHCR`, `A_IOP_D3_MADR`, `A_IOP_D4_BCR`, `A_IOP_D4_CHCR`, `A_IOP_D4_MADR`, `A_IOP_D4_TADR`, `A_IOP_D5_BCR`, `A_IOP_D5_CHCR`, `A_IOP_D5_MADR`, `A_IOP_D6_BCR`, `A_IOP_D6_CHCR`, `A_IOP_D6_MADR`, `A_IOP_D7_BCR`, `A_IOP_D7_CHCR`, `A_IOP_D7_MADR`, `A_IOP_D8_BCR`, `A_IOP_D8_CHCR`, `A_IOP_D8_MADR`, `A_IOP_D9_BCR`, `A_IOP_D9_CHCR`, `A_IOP_D9_MADR`, `A_IOP_D9_TADR`, `A_IOP_DICR`, `A_IOP_DICR2`, `A_IOP_DICR3`, `A_IOP_DMAC_1560`, `A_IOP_DMAC_1564`, `A_IOP_DMAC_1568` …

### `common/include/iopheap-common.h`

Macros/constantes: `LIH_PATH_MAX`, `__IOPHEAP_COMMON_H__`

### `common/include/iox_stat.h`

Funções:

- `iox_stat_t __attribute__((aligned(64)))`
- `iox_dirent_t __attribute__((aligned(64)))`

Macros/constantes: `FIO_CST_AT`, `FIO_CST_ATTR`, `FIO_CST_CT`, `FIO_CST_MODE`, `FIO_CST_MT`, `FIO_CST_PRVT`, `FIO_CST_SIZE`, `FIO_SO_IFDIR`, `FIO_SO_IFLNK`, `FIO_SO_IFMT`, `FIO_SO_IFREG`, `FIO_SO_IROTH`, `FIO_SO_ISDIR`, `FIO_SO_ISLNK`, `FIO_SO_ISREG`, `FIO_SO_IWOTH`, `FIO_SO_IXOTH`, `FIO_S_IFDIR`, `FIO_S_IFLNK`, `FIO_S_IFMT`, `FIO_S_IFREG`, `FIO_S_IRGRP`, `FIO_S_IROTH`, `FIO_S_IRUSR`, `FIO_S_IRWXG`, `FIO_S_IRWXO`, `FIO_S_IRWXU`, `FIO_S_ISDIR`, `FIO_S_ISGID`, `FIO_S_ISLNK`, `FIO_S_ISREG`, `FIO_S_ISUID`, `FIO_S_ISVTX`, `FIO_S_IWGRP`, `FIO_S_IWOTH`, `FIO_S_IWUSR`, `FIO_S_IXGRP`, `FIO_S_IXOTH`, `FIO_S_IXUSR`, `__SYS_IOX_STAT_H__`

### `common/include/libcdvd-common.h`

Funções:

- `requires sceCdSync() call * * @param lbn sector location to start reading from * @param sectors number of sectors to read * @param buffer buffer to read to * @param mode mode to read as * @return 1 on success, 0 on failure. */ extern int sceCdRead(u32 lbn, u32 sectors, void *buffer, sceCdRMode *mode)`
- `extern int sceCdReadDVDV(u32 lbn, u32 sectors, void *buffer, sceCdRMode *mode)`
- `extern int sceCdReadCDDA(u32 lbn, u32 sectors, void *buffer, sceCdRMode *mode)`
- `param toc buffer to hold toc(1024 bytes) * @return 1 on success, 0 on failure. */ extern int sceCdGetToc(u8 *toc)`
- `param toc buffer to hold toc(1024 bytes) * @param param Parameter * @return 1 on success, 0 on failure. */ extern int sceCdGetToc2(u8 *toc, int param)`
- `requires sceCdSync() call * * @param lbn sector to seek to on disc * @return 1 on success, 0 on failure. */ extern int sceCdSeek(u32 lbn)`
- `requires sceCdSync() call * * @return 1 on success, 0 on failure. */ extern int sceCdStandby(void)`
- `requires sceCdSync() call * * @return 1 on success, 0 on failure. */ extern int sceCdStop(void)`
- `requires sceCdSync() call * * @return 1 on success, 0 on failure. */ extern int sceCdPause(void)`
- `requires sceCdSync() call * * SUPPORTED IN XCDVDMAN/XCDVDFSV ONLY * * @param tag pointer to an array of read chain data * @param mode read mode * @return 1 on success, 0 on failure. */ extern int sceCdReadChain(sceCdRChain *tag, sceCdRMode *mode)`
- `param inBuff input buffer(can be null) * @param inBuffSize size of input buffer (0 - 16 bytes) * @return 1 on success, 0 on failure. */ extern int sceCdApplyNCmd(u8 cmdNum, const void* inBuff, u16 inBuffSize)`
- `extern int sceCdReadClock(sceCdCLOCK *clock)`
- `extern int sceCdWriteClock(sceCdCLOCK *clock)`
- `param param param(SCECdvdTrayReqMode) * @param traychk address for returning tray state change * @return 1 on success, 0 on failure. */ extern int sceCdTrayReq(int param, u32 *traychk)`
- `extern int sceCdBreak(void)`
- `extern int sceCdCancelPOffRdy(u32 *result)`
- `extern int sceCdBlueLEDCtl(u8 control, u32 *result)`
- `extern int sceCdPowerOff(u32 *result)`
- `param media media mode(SCECdvdMModeMediaTypes) * @return 1 on success, 0 on failure. */ extern int sceCdMmode(int media)`
- `extern int sceCdChangeThreadPriority(int priority)`
- `param input buffer(can be null) * @param size of input buffer (0 - 16 bytes) * @param output buffer (16 bytes, can be null) * @return 1 if successful, 0 if error */ extern int sceCdApplySCmd(u8 cmdNum, const void *inBuff, u16 inBuffSize, void *outBuff)`
- `extern int sceCdStStart(u32 lbn, sceCdRMode *mode)`
- `param mode data stream mode(STMNBLK or STMBLK) * @param error error value holder * @return number of sectors read if successful, 0 otherwise */ extern int sceCdStRead(u32 sectors, u32 *buffer, u32 mode, u32 *error)`
- `extern int sceCdStStop(void)`
- `extern int sceCdStSeek(u32 lbn)`
- `extern int sceCdStInit(u32 bufmax, u32 bankmax, void *buffer)`
- `extern int sceCdStStat(void)`
- `extern int sceCdStPause(void)`
- `extern int sceCdStResume(void)`
- `param mode mode(SCECdvdInitModes) * @return 1 on success, 0 on failure. */ extern int sceCdInit(int mode)`
- `extern int sceCdSearchFile(sceCdlFILE *file, const char *name)`
- `extern int sceCdDiskReady(int mode)`
- `extern sceCdlLOCCD * sceCdIntToPos(u32 i, sceCdlLOCCD *p)`
- `extern u32 sceCdPosToInt(sceCdlLOCCD *p)`
- `get the current read position(when reading using sceCdRead) */ extern u32 sceCdGetReadPos(void)`
- `extern int sceCdInitEeCB(int priority, void *stackAddr, int stackSize)`
- `extern sceCdCBFunc sceCdCallback(sceCdCBFunc function)`
- `extern int sceCdReadSUBQ(void *buffer, u32 *status)`
- `extern int sceCdForbidDVDP(u32 *result)`
- `tilt motor(on units that have one) to be activated. * * @param mode Mode * @param result Result code. * @return 1 on success, 0 on failure. */ extern int sceCdAutoAdjustCtrl(int mode, u32 *result)`
- `extern int sceCdDecSet(unsigned char enable_xor, unsigned char enable_shift, unsigned char shiftval)`
- `extern int sceCdReadKey(unsigned char arg1, unsigned char arg2, unsigned int command, unsigned char *key)`
- `extern int sceCdDoesUniqueKeyExist(u32 *status)`
- `extern int sceCdSetHDMode(u32 mode)`
- `param mode Mode(0 = read, 1 = write). * @param NumBlocks Number of blocks. * @param status Result code. * @return 1 on success, 0 on failure. */ extern int sceCdOpenConfig(int block, int mode, int NumBlocks, u32 *status)`
- `extern int sceCdCloseConfig(u32 *result)`
- `Reads the configuration block(s) that was/were previously opened. Each block is 15 bytes long. * * @param buffer Pointer to the buffer to store the configuration data in. * @param result Result code. * @return 1 on success, 0 on failure. */ extern int sceCdReadConfig(void *buffer, u32 *result)`
- `Writes to the configuration block(s) that was/were previously opened. Each block is 15 bytes long. * * @param buffer Pointer to the buffer that contains the new configuration data. * @param result Result code. * @return 1 on success, 0 on failure. */ extern int sceCdWriteConfig(const void *buffer, u32 *result)`
- `extern int sceCdReadNVM(u32 address, u16 *data, u8 *result)`
- `extern int sceCdWriteNVM(u32 address, u16 data, u8 *result)`
- `extern int sceCdRI(u8 *buffer, u32 *result)`
- `a unlock combination(0x03 0x46 and 0x03 0x47) needs to be executed first. * * @param buffer Pointer to the buffer that contains the new i.Link ID. * @param result Result code. * @return 1 on success, 0 on failure. */ extern int sceCdWI(const u8 *buffer, u32 *result)`
- `extern int sceCdReadConsoleID(u8 *buffer, u32 *result)`
- `a unlock combination(0x03 0x46 and 0x03 0x47) needs to be executed first. * * @param buffer Pointer to the buffer that contains the new i.Link ID. * @param status Result code. * @return 1 on success, 0 on failure. */ extern int sceCdWriteConsoleID(const u8 *buffer, u32 *status)`
- `extern int sceCdCtrlADout(int mode, u32 *status)`
- `Reads MECHACON version data(RR MM mm TT): RR = Magicgate region, MM = major, mm = minor, TT = system type (00 = PS2, 01 = PSX) * Magicgate region codes are: 00 = Japan, 01 = USA, 02 = Europe, 03 = Oceania, 04 = Asia, 05 = Russia, 06 = China, and 07 = Mexico. * * NOTE: The old CDVDMAN module only returns MM mm TT instead. The region byte is made up of the lower 7 bits of the status/result code. * * @param buffer Pointer to the buffer to store the version data in. * @param status Result code. * @return 1 on success, 0 on failure. */ extern int sceCdMV(u8 *buffer, u32 *status)`
- `ROM major version number(In decimal). * romname[1] - ROM minor version number (In decimal). * romname[2] - ROM region. * romname[3] - Console type (E.g. C, D or T). * @return 1 on success, 0 on failure. */ extern int sceCdBootCertify(const u8 *romname)`
- `extern int sceCdRM(char *buffer, u32 *status)`
- `a unlock combination(0x03 0x46 and 0x03 0x47) needs to be executed first. * SUPPORTED IN XCDVDMAN/XCDVDFSV ONLY * * @param buffer Pointer to a buffer containing the new model name. * @param status Result code. * @return 1 on success, 0 on failure. */ extern int sceCdWM(const char *buffer, u32 *status)`
- `extern int sceCdForbidRead(u32 *result)`
- `extern int sceCdReadFull(unsigned int lsn, unsigned int sectors, void *buf, sceCdRMode *mode)`
- `extern int sceCdStSeekF(unsigned int lsn)`
- `extern void * sceCdPOffCallback(void (*func)(void *userdata), void *userdata)`
- `extern int sceCdSetTimeout(int param, int timeout)`
- `extern int sceCdReadModelID(unsigned int *id)`
- `extern int sceCdReadDvdDualInfo(int *on_dual, unsigned int *layer1_start)`
- `param layer layer to search(0 for the first layer, 1 for the second layer) * @return 1 on success, 0 on failure. */ extern int sceCdLayerSearchFile(sceCdlFILE *fp, const char *path, int layer)`
- `extern int sceCdStatus2(void)`
- `extern int sceCdRE(unsigned int lsn, unsigned int sectors, void *buf, sceCdRMode *mode)`
- `extern int sceCdReadGUID(u64 *guid)`
- `extern int sceCdRcBypassCtl(int mode, u32 *status)`
- `param userdata Any arbitrary value(not used in timer processing) * @param wakeupreason The reason why the system woke up. * @param flags bit 0 -> disable timer after expiration, bit 1 -> disable timer * @return 1 on success, 0 on failure */ extern int sceCdReadWakeUpTime(sceCdCLOCK *clock, u16 *userdata, u32 *wakeupreason, int *flags)`
- `in newer Mechacon firmware versions(TODO: determine the range), the wake up timer function is removed. However, the storage for the parameters remain. * Note: if there was a non-zero value in sceCdCLOCK.stat the last time sceCdReadClock was called, no data will be written/enabled. * SUPPORTED IN XCDVDMAN INCLUDED WITHIN NEWER BOOT ROMS ONLY * * @param clock The time to wake up the system. * @param userdata Any arbitrary value (not used in timer processing) * @param flags When 1, disables the timer after it has expired. When 255, disables the timer and sets the seconds value to 0xFF. * @return 1 on success, 0 on failure */ extern int sceCdWriteWakeUpTime(const sceCdCLOCK *clock, u16 userdata, int flags)`
- `extern int sceRemote2_7(u16 param, u32 *status)`
- `extern int sceRemote2_7Get(u32 *param, u32 *status)`
- `extern int sceCdSetLEDsMode(u32 param, u32 *result)`
- `extern int sceCdReadPS1BootParam(u8 *out, u32 *result)`
- `param param param(SCECdvdFanSpeed) * @return 1 on success, 0 on failure */ extern int sceCdSetFanProfile(u8 param, u32 *result)`
- `extern int sceCdSendSCmd1D(int *arg1, unsigned int *arg2, unsigned int *arg3, u32 *status)`
- `extern int sceCdChgSys(u32 arg1)`
- … +13 (ver header)

Macros/constantes: `CDVD_ERR_ABRT`, `CDVD_ERR_CMD`, `CDVD_ERR_CUD`, `CDVD_ERR_EOM`, `CDVD_ERR_FAIL`, `CDVD_ERR_ILI`, `CDVD_ERR_IPI`, `CDVD_ERR_NO`, `CDVD_ERR_NODISC`, `CDVD_ERR_NORDY`, `CDVD_ERR_OPENS`, `CDVD_ERR_PRM`, `CDVD_ERR_READ`, `CDVD_ERR_READCF`, `CDVD_ERR_READCFR`, `CDVD_ERR_TRMOPN`, `CDVD_INIT_EXIT`, `CDVD_INIT_INIT`, `CDVD_INIT_NOCHECK`, `CDVD_MEDIA_MODE_CD`, `CDVD_MEDIA_MODE_DVD`, `CDVD_READY_NOTREADY`, `CDVD_READY_READY`, `CDVD_SECTOR_2048`, `CDVD_SECTOR_2328`, `CDVD_SECTOR_2340`, `CDVD_SECTOR_2352`, `CDVD_SECTOR_2368`, `CDVD_SECTOR_2448`, `CDVD_SPIN_DVD0`, `CDVD_SPIN_MAX`, `CDVD_SPIN_NORMAL`, `CDVD_SPIN_STREAM`, `CDVD_STAT_ERROR`, `CDVD_STAT_OPEN`, `CDVD_STAT_PAUSE`, `CDVD_STAT_READ`, `CDVD_STAT_SEEK`, `CDVD_STAT_SPIN`, `CDVD_STAT_STOP`, `CDVD_STREAM_BLOCK`, `CDVD_STREAM_NONBLOCK`, `CDVD_TRAY_CHECK`, `CDVD_TRAY_CLOSE`, `CDVD_TRAY_OPEN`, `CDVD_TYPE_CDDA`, `CDVD_TYPE_DETECT`, `CDVD_TYPE_DETECT_CD`, `CDVD_TYPE_DETECT_DVDDUAL`, `CDVD_TYPE_DETECT_DVDSINGLE`, `CDVD_TYPE_DVDVIDEO`, `CDVD_TYPE_ILLEGAL`, `CDVD_TYPE_NODISK`, `CDVD_TYPE_PS1CD`, `CDVD_TYPE_PS1CDDA`, `CDVD_TYPE_PS2CD`, `CDVD_TYPE_PS2CDDA`, `CDVD_TYPE_PS2DVD`, `CDVD_TYPE_UNKNOWN`, `CdBreak` …

### `common/include/libcdvd-rpc.h`

Macros/constantes: `__LIBCDVD_RPC_H__`

### `common/include/libmc-common.h`

Macros/constantes: `EFORMAT`, `SCE_CST_AT`, `SCE_CST_ATTR`, `SCE_CST_CT`, `SCE_CST_MODE`, `SCE_CST_MT`, `SCE_CST_PRVT`, `SCE_CST_SIZE`, `SCE_STM_C`, `SCE_STM_D`, `SCE_STM_F`, `SCE_STM_R`, `SCE_STM_W`, `SCE_STM_X`, `__LIBMC_COMMON_H__`, `sceMcFile0400`, `sceMcFileAttrClosed`, `sceMcFileAttrDupProhibit`, `sceMcFileAttrExecutable`, `sceMcFileAttrExists`, `sceMcFileAttrFile`, `sceMcFileAttrHidden`, `sceMcFileAttrPDAExec`, `sceMcFileAttrPS1`, `sceMcFileAttrReadable`, `sceMcFileAttrSubdir`, `sceMcFileAttrWriteable`, `sceMcFileCreateDir`, `sceMcFileCreateFile`, `sceMcFileInfoAttr`, `sceMcFileInfoCreate`, `sceMcFileInfoModify`, `sceMcResChangedCard`, `sceMcResDeniedPS1Permit`, `sceMcResDeniedPermit`, `sceMcResFailAuth`, `sceMcResFailDetect`, `sceMcResFailDetect2`, `sceMcResFailReplace`, `sceMcResFailResetAuth`, `sceMcResFullDevice`, `sceMcResNoEntry`, `sceMcResNoFormat`, `sceMcResNotEmpty`, `sceMcResSucceed`, `sceMcResUpLimitHandle`, `sceMcTypeNoCard`, `sceMcTypePDA`, `sceMcTypePS1`, `sceMcTypePS2`

### `common/include/librm-common.h`

Macros/constantes: `RMMAN2_RPC_ID`, `RMMANX_RPC_ID`, `RMMAN_RPC_ID`, `__LIBRM_COMMON_H__`

### `common/include/libsd-common.h`

Funções:

- `typedef int(*sceSdSpu2IntrHandler)(int core_bit, void *arg)`
- `typedef int(*sceSdTransIntrHandler)(int core, void *arg)`
- `typedef int(*SdIntrCallback)(void *data)`
- `typedef int(*sceSdBlockTransHandler)(int channel, void *userdata, void **addr, int *size)`
- `extern int sceSdInit(int flag)`
- `extern SdIntrCallback sceSdSetIRQCallback(SdIntrCallback cb)`
- `extern SdIntrCallback sceSdSetTransCallback(s32 core, SdIntrCallback cb)`
- `extern void sceSdSetParam(u16 entry, u16 value)`
- `extern u16 sceSdGetParam(u16 entry)`
- `extern void sceSdSetCoreAttr(u16 entry, u16 value)`
- `extern u16 sceSdGetCoreAttr(u16 entry)`
- `extern int sceSdClearEffectWorkArea(int core, int channel, int effect_mode)`
- `extern void sceSdSetAddr(u16 entry, u32 value)`
- `extern u32 sceSdGetAddr(u16 entry)`
- `extern void sceSdSetSwitch(u16 entry, u32 value)`
- `extern u32 sceSdGetSwitch(u16 entry)`
- `extern u16 sceSdNote2Pitch(u16 center_note, u16 center_fine, u16 note, s16 fine)`
- `extern u16 sceSdPitch2Note(u16 center_note, u16 center_fine, u16 pitch)`
- `extern int sceSdSetEffectAttr(int core, const sceSdEffectAttr *attr)`
- `extern void sceSdGetEffectAttr(int core, sceSdEffectAttr *attr)`
- `extern int sceSdProcBatch(const sceSdBatch *batch, u32 *rets, u32 num)`
- `extern int sceSdProcBatchEx(const sceSdBatch *batch, u32 *rets, u32 num, u32 voice)`
- `extern int sceSdVoiceTrans(s16 chan, u16 mode, u8 *iopaddr, u32 *spuaddr, u32 size)`
- `extern int sceSdBlockTrans(s16 chan, u16 mode, u8 *iopaddr, u32 size, ...)`
- `extern u32 sceSdVoiceTransStatus(s16 channel, s16 flag)`
- `extern u32 sceSdBlockTransStatus(s16 channel, s16 flag)`
- `extern sceSdTransIntrHandler sceSdSetTransIntrHandler(int channel, sceSdTransIntrHandler func, void *arg)`
- `extern sceSdSpu2IntrHandler sceSdSetSpu2IntrHandler(sceSdSpu2IntrHandler func, void *arg)`
- `extern void * sceSdGetTransIntrHandlerArgument(int arg)`
- `extern void * sceSdGetSpu2IntrHandlerArgument()`
- `extern int sceSdStopTrans(int channel)`
- `extern int sceSdCleanEffectWorkArea(int core, int channel, int effect_mode)`
- `extern int sceSdSetEffectMode(int core, const sceSdEffectAttr *param)`
- `extern int sceSdSetEffectModeParams(int core, const sceSdEffectAttr *attr)`

Macros/constantes: `ADPCM_LOOP`, `ADPCM_LOOP_END`, `ADPCM_LOOP_START`, `BATCH_EERETURN`, `BATCH_GETADDR`, `BATCH_GETCORE`, `BATCH_GETPARAM`, `BATCH_GETSWITCH`, `BATCH_SETADDR`, `BATCH_SETCORE`, `BATCH_SETPARAM`, `BATCH_SETSWITCH`, `BATCH_WRITEEE`, `BATCH_WRITEIOP`, `IntrCallback`, `SD_ADDR_EEA`, `SD_ADDR_ESA`, `SD_ADDR_IRQA`, `SD_ADDR_TSA`, `SD_ADSR_AR_EXPi`, `SD_ADSR_AR_LINEARi`, `SD_ADSR_RR_EXPd`, `SD_ADSR_RR_LINEARd`, `SD_ADSR_SR_EXPd`, `SD_ADSR_SR_EXPi`, `SD_ADSR_SR_LINEARd`, `SD_ADSR_SR_LINEARi`, `SD_BATCH_EERETURN`, `SD_BATCH_GETADDR`, `SD_BATCH_GETCORE`, `SD_BATCH_GETPARAM`, `SD_BATCH_GETSWITCH`, `SD_BATCH_SETADDR`, `SD_BATCH_SETCORE`, `SD_BATCH_SETPARAM`, `SD_BATCH_SETSWITCH`, `SD_BATCH_WRITEEE`, `SD_BATCH_WRITEIOP`, `SD_BLOCK_TRANS_LOOP`, `SD_BLOCK_TRANS_READ`, `SD_BLOCK_TRANS_STOP`, `SD_BLOCK_TRANS_WRITE`, `SD_BLOCK_TRANS_WRITE_FROM`, `SD_CORE_DMA`, `SD_CORE_EFFECT_ENABLE`, `SD_CORE_IRQ_ENABLE`, `SD_CORE_MUTE_ENABLE`, `SD_CORE_NOISE_CLK`, `SD_CORE_SPDIF_MODE`, `SD_DMA_IN_PROCESS`, `SD_DMA_IO`, `SD_DMA_READ`, `SD_DMA_WRITE`, `SD_EFFECT_MODE_CLEAR`, `SD_EFFECT_MODE_DELAY`, `SD_EFFECT_MODE_ECHO`, `SD_EFFECT_MODE_HALL`, `SD_EFFECT_MODE_OFF`, `SD_EFFECT_MODE_PIPE`, `SD_EFFECT_MODE_ROOM` …

### `common/include/libsdr-common.h`

Macros/constantes: `SDR_CB_DMA0`, `SDR_CB_DMA0INT`, `SDR_CB_DMA1`, `SDR_CB_DMA1INT`, `SDR_CB_IRQ`, `SDR_CB_IRQINT`, `SDR_CMD_CB_DEINIT`, `SDR_CMD_CB_INIT`, `__LIBSDR_COMMON_H__`, `rSdBlockTrans`, `rSdBlockTransStatus`, `rSdChangeThreadPriority`, `rSdCleanEffectWorkArea`, `rSdClearEffectWorkArea`, `rSdGetAddr`, `rSdGetCoreAttr`, `rSdGetEffectAttr`, `rSdGetParam`, `rSdGetSwitch`, `rSdInit`, `rSdNote2Pitch`, `rSdPitch2Note`, `rSdProcBatch`, `rSdProcBatch2`, `rSdProcBatchEx`, `rSdProcBatchEx2`, `rSdSetAddr`, `rSdSetCoreAttr`, `rSdSetEffectAttr`, `rSdSetEffectMode`, `rSdSetEffectModeParams`, `rSdSetIRQCallback`, `rSdSetParam`, `rSdSetSpu2IntrHandler`, `rSdSetSwitch`, `rSdSetTransCallback`, `rSdSetTransIntrHandler`, `rSdStopTrans`, `rSdUserCommand0`, `rSdUserCommand1`, `rSdUserCommand2`, `rSdUserCommand3`, `rSdUserCommand4`, `rSdUserCommand5`, `rSdUserCommand6`, `rSdUserCommand7`, `rSdUserCommand8`, `rSdUserCommand9`, `rSdUserCommandA`, `rSdUserCommandB`, `rSdUserCommandC`, `rSdUserCommandD`, `rSdUserCommandE`, `rSdUserCommandF`, `rSdUserCommandMax`, `rSdUserCommandMin`, `rSdVoiceTrans`, `rSdVoiceTransStatus`, `sce_SDRST_CB`, `sce_SDR_DEV`

### `common/include/libsecr-common.h`

Funções:

- `extern int SecrCardBootHeader(int port, int slot, void *buffer, SecrBitTable_t *BitTable, s32 *pSize)`
- `extern int SecrCardBootBlock(void *src, void *dst, unsigned int size)`
- `extern void * SecrCardBootFile(int port, int slot, void *buffer)`
- `extern int SecrDiskBootHeader(void *buffer, SecrBitTable_t *BitTable, s32 *pSize)`
- `extern int SecrDiskBootBlock(void *src, void *dst, unsigned int size)`
- `extern void * SecrDiskBootFile(void *buffer)`
- `extern int SecrDownloadHeader(int port, int slot, void *buffer, SecrBitTable_t *BitTable, s32 *pSize)`
- `extern int SecrDownloadBlock(void *src, unsigned int size)`
- `extern void * SecrDownloadFile(int port, int slot, void *buffer)`
- `extern int SecrDownloadGetKbit(int port, int slot, void *kbit)`
- `extern int SecrDownloadGetKc(int port, int slot, void *kbit)`
- `extern int SecrDownloadGetICVPS2(void *icvps2)`

Macros/constantes: `__LIBSECR_COMMON_H__`

### `common/include/loadfile-common.h`

Macros/constantes: `LF_ARG_MAX`, `LF_PATH_MAX`, `__LOADFILE_COMMON_H__`

### `common/include/mipscopaccess.h`

Funções:

- `__asm__ __volatile__("mfc%[cop]\t%[val], $%[idx]\n" : [val] "=r"(val) : [cop] "i"(cop), [idx] "i"(idx))`
- `__asm__ __volatile__("mtc%[cop]\t%[val], $%[idx]\n" :: [val] "r"(val), [cop] "i"(cop), [idx] "i"(idx))`

Macros/constantes: `__MIPSCOPACCESS_H__`

### `common/include/netman.h`

Funções:

- `extern void NetManDeinit(void)`
- `Network Interface(IF) control. extern int NetManGetGlobalNetIFLinkState(void)`
- `extern int NetManSetMainIF(const char *name)`
- `extern int NetManQueryMainIF(char *name)`
- `extern int NetManRegisterNetworkStack(const struct NetManNetProtStack *stack)`
- `extern void NetManUnregisterNetworkStack(void)`
- `Common network Interface(IF) management functions. Used by the user program and the protocol stack. */ extern int NetManIoctl(unsigned int command, void *args, unsigned int args_len, void *output, unsigned int length)`
- `extern int NetManSetLinkMode(int mode)`
- `Network Interface(IF) management functions. Used by the protocol stack. */ extern void NetManNetIFXmit(void)`
- `Used by the Network InterFace(IF) driver. */ extern void *NetManNetProtStackAllocRxPacket(unsigned int length, void **payload)`
- `extern void NetManNetProtStackFreeRxPacket(void *packet)`
- `extern void NetManNetProtStackEnQRxPacket(void *packet)`
- `extern int NetManTxPacketNext(void **payload)`
- `extern void NetManTxPacketDeQ(void)`
- `extern int NetManTxPacketAfter(void **payload)`
- `extern void NetManNetProtStackReallocRxPacket(void *packet, unsigned int length)`
- `Network InterFace(IF) management functions. Used by the network InterFace (IF). */ extern int NetManRegisterNetIF(struct NetManNetIF *NetIF)`
- `extern void NetManUnregisterNetIF(const char *name)`
- `extern void NetManToggleNetIFLinkState(int NetIFID, unsigned char state)`

Macros/constantes: `I_NetManGetGlobalNetIFLinkState`, `I_NetManIoctl`, `I_NetManNetIFXmit`, `I_NetManNetProtStackAllocPacket`, `I_NetManNetProtStackEnQPacket`, `I_NetManNetProtStackFreePacket`, `I_NetManQueryMainIF`, `I_NetManRegisterNetIF`, `I_NetManRegisterNetworkStack`, `I_NetManSetLinkMode`, `I_NetManSetMainIF`, `I_NetManToggleNetIFLinkState`, `I_NetManTxPacketDeQ`, `I_NetManTxPacketNext`, `I_NetManUnregisterNetIF`, `I_NetManUnregisterNetworkStack`, `NETMAN_FRAME_GROUP_SIZE`, `NETMAN_MAX_NETIF_COUNT`, `NETMAN_NETIF_DIALUP`, `NETMAN_NETIF_ETHERNET`, `NETMAN_NETIF_ETH_LINK_DISABLE_PAUSE`, `NETMAN_NETIF_EVF_DOWN`, `NETMAN_NETIF_EVF_UP`, `NETMAN_NETIF_FRAME_SIZE`, `NETMAN_NETIF_IN_USE`, `NETMAN_NETIF_LINK_UP`, `NETMAN_NETIF_NAME_MAX_LEN`, `__NETMAN_H__`, `netman_IMPORTS_end`, `netman_IMPORTS_start`

### `common/include/netman_rpc.h`

Macros/constantes: `NETMAN_MAX_FRAME_SIZE`, `NETMAN_RPC_BLOCK_SIZE`, `NETMAN_RPC_NUMBER`, `NETMAN_SIFCMD_ID`, `__NETMAN_RPC_H__`

### `common/include/ps2_debug.h`

Macros/constantes: `__PS2_DEBUG_H__`

### `common/include/ps2_reg_defs.h`

Macros/constantes: `PS2_DMA_FROM_MEM`, `PS2_DMA_TO_MEM`, `PS2_IRQ_SBUS`, `PS2_IRQ_SIF0`, `PS2_IRQ_SIF1`, `PS2_IRQ_SIF2`, `PS2_IRQ_UNK0`, `PS2_IRQ_UNK10`, `PS2_IRQ_UNK2`, `PS2_IRQ_UNK3`, `PS2_IRQ_UNK4`, `PS2_IRQ_UNK8`, `PS2_IRQ_UNK9`, `PS2_SBUS_MS_ADDR`, `PS2_SBUS_MS_FLAG`, `PS2_SBUS_REG4`, `PS2_SBUS_REG5`, `PS2_SBUS_REG6`, `PS2_SBUS_REG7`, `PS2_SBUS_SM_ADDR`, `PS2_SBUS_SM_FLAG`, `R_EE_SBUS`, `R_IOP_SBUS`, `R_PS2_SBUS`, `SBUS_CTRL_MSCLK`, `SBUS_CTRL_MSINT`, `SBUS_CTRL_PGPU_INT`, `SBUS_CTRL_PS1_RESET`, `SIF_FLAG_INIT`, `SIF_XFER_MODE_IN`, `SIF_XFER_MODE_OUT`, `__PS2_REG_DEFS_H__`

### `common/include/ps2ip_rpc.h`

Macros/constantes: `PS2IP_IRX`, `__PS2IP_RPC_H__`

### `common/include/ps2lib_err.h`

Macros/constantes: `__PS2LIB_ERR_H__`

### `common/include/ps2smb.h`

Macros/constantes: `HASHED_PASSWORD`, `NO_PASSWORD`, `PLAINTEXT_PASSWORD`, `SMB_DEVCTL_CLOSESHARE`, `SMB_DEVCTL_ECHO`, `SMB_DEVCTL_GETPASSWORDHASHES`, `SMB_DEVCTL_GETSHARELIST`, `SMB_DEVCTL_LOGOFF`, `SMB_DEVCTL_LOGON`, `SMB_DEVCTL_LOGON_ERR_CONN`, `SMB_DEVCTL_LOGON_ERR_LOGON`, `SMB_DEVCTL_LOGON_ERR_PROT`, `SMB_DEVCTL_OPENSHARE`, `SMB_DEVCTL_QUERYDISKINFO`, `__PS2SMB_H__`

### `common/include/ps2snd.h`

Funções:

- `extern u32 sndQueryMaxFreeMemSize(void)`
- `extern int sndStreamClose(void)`
- `extern int sndStreamPlay(void)`
- `extern int sndStreamPause(void)`
- `extern int sndStreamSetPosition(int block)`
- `extern int sndStreamGetPosition(void)`
- `extern int sndStreamSetVolume(int left, int right)`
- `extern u32 sndQueryMaxFreeMemSize()`

Macros/constantes: `BINDID_PS2SND`, `PS2SND_BlockTrans`, `PS2SND_BlockTransStatus`, `PS2SND_ClearEffectWorkArea`, `PS2SND_GetAddr`, `PS2SND_GetCoreAttr`, `PS2SND_GetEffectAttr`, `PS2SND_GetParam`, `PS2SND_GetSwitch`, `PS2SND_Init`, `PS2SND_Note2Pitch`, `PS2SND_Pitch2Note`, `PS2SND_ProcBatch`, `PS2SND_ProcBatchEx`, `PS2SND_QueryMaxFreeMemSize`, `PS2SND_SetAddr`, `PS2SND_SetCoreAttr`, `PS2SND_SetEffectAttr`, `PS2SND_SetIRQCallback`, `PS2SND_SetParam`, `PS2SND_SetSpu2IntrHandler`, `PS2SND_SetSwitch`, `PS2SND_SetTransCallback`, `PS2SND_SetTransIntrHandler`, `PS2SND_StreamClose`, `PS2SND_StreamGetPosition`, `PS2SND_StreamOpen`, `PS2SND_StreamPause`, `PS2SND_StreamPlay`, `PS2SND_StreamSetPosition`, `PS2SND_StreamSetVolume`, `PS2SND_VoiceTrans`, `PS2SND_VoiceTransStatus`, `STREAM_END_CLOSE`, `STREAM_END_REPEAT`, `STREAM_STEREO`, `__PS2SND_H__`

### `common/include/pwroff_rpc.h`

Macros/constantes: `POFF_RPC_BUTTON`, `PWROFF_ENABLE_AUTO_SHUTOFF`, `PWROFF_IRX`, `PWROFF_SHUTDOWN`, `__PWROFF_RPC_H__`

### `common/include/s147_mmio_hwport.h`

Macros/constantes: `USE_S147LINK_DEV9_MEM_MMIO`, `USE_S147MAMD_DEV9_IO_MMIO`, `USE_S147_DEV9_MEM_MMIO`, `__S147_MMIO_HWPORT__`

### `common/include/secrsif.h`

Macros/constantes: `SECRSIF_DISK_BOOT_BLOCK`, `SECRSIF_DISK_BOOT_HEADER`, `SECRSIF_DOWNLOAD_BLOCK`, `SECRSIF_DOWNLOAD_GET_ICVPS2`, `SECRSIF_DOWNLOAD_GET_KBIT`, `SECRSIF_DOWNLOAD_GET_KC`, `SECRSIF_DOWNLOAD_HEADER`

### `common/include/sif_mmio_hwport.h`

Macros/constantes: `USE_SIF_MMIO_HWPORT`, `__SIF_MMIO_HWPORT__`

### `common/include/sifcmd-common.h`

Funções:

- `extern void sceSifExitCmd(void)`
- `extern unsigned int sceSifGetSreg(int sreg)`
- `extern void sceSifSetSreg(int sreg, unsigned int value)`
- `extern void sceSifSetCmdBuffer(SifCmdHandlerData_t *db, int size)`
- `extern void sceSifSetSysCmdBuffer(SifCmdSysHandlerData_t *db, int size)`
- `extern void sceSifAddCmdHandler(int cid, SifCmdHandler_t handler, void *harg)`
- `extern void sceSifRemoveCmdHandler(int cid)`
- `extern unsigned int sceSifSendCmd(int cid, void *packet, int packet_size, void *src_extra, void *dest_extra, int size_extra)`
- `extern unsigned int isceSifSendCmd(int cid, void *packet, int packet_size, void *src_extra, void *dest_extra, int size_extra)`
- `extern unsigned int isceSifSendCmdIntr(int cid, void *packet, int packet_size, void *src_extra, void *dest_extra, int size_extra, void (*completioncb)(void *userdata), void *userdata)`

Macros/constantes: `RESET_ARG_MAX`, `SIF_CMD_CHANGE_SADDR`, `SIF_CMD_ID_SYSTEM`, `SIF_CMD_INIT_CMD`, `SIF_CMD_M_INTR`, `SIF_CMD_M_WBDC`, `SIF_CMD_RESET_CMD`, `SIF_CMD_RPC_BIND`, `SIF_CMD_RPC_CALL`, `SIF_CMD_RPC_END`, `SIF_CMD_RPC_RDATA`, `SIF_CMD_SET_SREG`, `SIF_SREG_RPCINIT`, `SifAddCmdHandler`, `SifExitCmd`, `SifGetSreg`, `SifInitCmd`, `SifRemoveCmdHandler`, `SifSendCmd`, `SifSetCmdBuffer`, `SifWriteBackDCache`, `__SIFCMD_COMMON_H__`, `iSifSendCmd`

### `common/include/sifrpc-common.h`

Funções:

- `typedef void(*SifRpcEndFunc_t)(void *end_param)`
- `extern int sceSifBindRpc(SifRpcClientData_t *cd, int sid, int mode)`
- `extern int sceSifCallRpc(SifRpcClientData_t *cd, int rpc_number, int mode, void *sendbuf, int ssize, void *recvbuf, int rsize, SifRpcEndFunc_t end_function, void *end_param)`
- `extern void sceSifRegisterRpc(SifRpcServerData_t *sd, int sid, SifRpcFunc_t func, void *buf, SifRpcFunc_t cfunc, void *cbuf, SifRpcDataQueue_t *qd)`
- `extern int sceSifCheckStatRpc(SifRpcClientData_t *cd)`
- `extern void sceSifSetRpcQueue(SifRpcDataQueue_t *qd, int thread_id)`
- `extern void sceSifExecRequest(SifRpcServerData_t *sd)`
- `extern void sceSifRpcLoop(SifRpcDataQueue_t *qd)`
- `extern int sceSifGetOtherData(SifRpcReceiveData_t *rd, void *src, void *dest, int size, int mode)`
- `extern void sceSifClearSif1CB(void)`

Macros/constantes: `SIF_RPC_M_NOWAIT`, `SIF_RPC_M_NOWBDC`, `SifBindRpc`, `SifCallRpc`, `SifCheckStatRpc`, `SifExecRequest`, `SifExitRpc`, `SifGetNextRequest`, `SifInitRpc`, `SifRegisterRpc`, `SifRemoveRpc`, `SifRemoveRpcQueue`, `SifRpcGetOtherData`, `SifRpcLoop`, `SifSetRpcQueue`, `__SIFRPC_COMMON_H__`

### `common/include/sior-common.h`

Macros/constantes: `SIOR_IRX`, `__SIOR_COMMON_H__`

### `common/include/smapregs.h`

Funções:

- `file * SMAP(PS2 Network Adapter) register definitions. * Contains register definitions for the Ethernet MAC (EMAC). * The EMAC is similar to the EMAC from the IBM PowerPC 405gp. */ #ifndef __SMAPREGS_H__ #define __SMAPREGS_H__ #include <tamtypes.h> #include <speedregs.h> /* SMAP interrupt status bits (selected from the SPEED device). */ #define SMAP_INTR_EMAC3 (1 << 6) #define SMAP_INTR_RXEND (1 << 5) #define SMAP_INTR_TXEND (1 << 4) #define SMAP_INTR_RXDNV (1 << 3) /* descriptor not valid */ #define SMAP_INTR_TXDNV (1 << 2) /* descriptor not valid */ #define SMAP_INTR_CLR_ALL (SMAP_INTR_RXEND | SMAP_INTR_TXEND | SMAP_INTR_RXDNV) #define SMAP_INTR_ENA_ALL (SMAP_INTR_EMAC3 | SMAP_INTR_CLR_ALL) #define SMAP_INTR_BITMSK (SMAP_INTR_EMAC3 | SMAP_INTR_RXEND | SMAP_INTR_TXEND | SMAP_INTR_RXDNV | SMAP_INTR_TXDNV) /* SMAP Register Definitions. */ #define SMAP_REGBASE (SPD_REGBASE + 0x100) #define USE_SMAP_REGS volatile u8 *smap_regbase = \ (volatile u8 *)SMAP_REGBASE #define SMAP_REG8(offset) (*(volatile u8 *)(smap_regbase + (offset))) #define SMAP_REG16(offset) (*(volatile u16 *)(smap_regbase + (offset))) #define SMAP_REG32(offset) (*(volatile u32 *)(smap_regbase + (offset))) #define SMAP_R_BD_MODE 0x02 #define SMAP_BD_SWAP (1 << 0) #define SMAP_R_INTR_CLR 0x28 /* SMAP FIFO Registers. */ #define SMAP_R_TXFIFO_CTRL 0xf00 #define SMAP_TXFIFO_RESET (1 << 0) #define SMAP_TXFIFO_DMAEN (1 << 1) #define SMAP_R_TXFIFO_WR_PTR 0xf04 #define SMAP_R_TXFIFO_SIZE 0xf08 #define SMAP_R_TXFIFO_FRAME_CNT 0xf0C #define SMAP_R_TXFIFO_FRAME_INC 0xf10 #define SMAP_R_TXFIFO_DATA 0x1000 #define SMAP_R_RXFIFO_CTRL 0xf30 #define SMAP_RXFIFO_RESET (1 << 0) #define SMAP_RXFIFO_DMAEN (1 << 1) #define SMAP_R_RXFIFO_RD_PTR 0xf34 #define SMAP_R_RXFIFO_SIZE 0xf38 #define SMAP_R_RXFIFO_FRAME_CNT 0xf3C #define SMAP_R_RXFIFO_FRAME_DEC 0xf40 #define SMAP_R_RXFIFO_DATA 0x1100 #define SMAP_R_FIFO_ADDR 0x1200 #define SMAP_FIFO_CMD_READ (1 << 1) #define SMAP_FIFO_DATA_SWAP (1 << 0) #define SMAP_R_FIFO_DATA 0x1208 /* EMAC3 Registers. */ #define SMAP_EMAC3_REGBASE 0x1f00 /* Seperating out the EMAC3 registers makes it a lot easier to debug register assigns. Besides, I am a staunch supporter of preprocessor macro abuse. */ #define USE_SMAP_EMAC3_REGS volatile u8 *emac3_regbase = \ (volatile u8 *)(SMAP_REGBASE + SMAP_EMAC3_REGBASE) #define SMAP_EMAC3_REG(offset) (*(volatile u16 *)(emac3_regbase + (offset))) #define SMAP_EMAC3_REG32(offset) (*(volatile u32 *)(emac3_regbase + (offset))) #define SMAP_EMAC3_GET(offset) ((SMAP_EMAC3_REG((offset)) << 16) | \ (SMAP_EMAC3_REG((offset) + 2))) #define SMAP_EMAC3_GET32(offset) (((SMAP_EMAC3_REG32((offset)) >> 16) & 0xffff) | \ ((SMAP_EMAC3_REG32((offset)) & 0xffff) << 16)) #define SMAP_EMAC3_WRITE32(offset, val) \ SMAP_EMAC3_REG32((offset)) = ((((val) >> 16) & 0xffff) | (((val)&0xffff) << 16))`

Macros/constantes: `SMAP_BD_MAX_ENTRY`, `SMAP_BD_REGBASE`, `SMAP_BD_RX_ALIGNERR`, `SMAP_BD_RX_BADFCS`, `SMAP_BD_RX_BADFRM`, `SMAP_BD_RX_BASE`, `SMAP_BD_RX_EMPTY`, `SMAP_BD_RX_ERROR`, `SMAP_BD_RX_FRMTOOLONG`, `SMAP_BD_RX_INRANGE`, `SMAP_BD_RX_OUTRANGE`, `SMAP_BD_RX_OVERRUN`, `SMAP_BD_RX_PFRM`, `SMAP_BD_RX_RUNTFRM`, `SMAP_BD_RX_SHORTEVNT`, `SMAP_BD_SIZE`, `SMAP_BD_SWAP`, `SMAP_BD_TX_BADFCS`, `SMAP_BD_TX_BADPKT`, `SMAP_BD_TX_BASE`, `SMAP_BD_TX_ECOLL`, `SMAP_BD_TX_EDEFER`, `SMAP_BD_TX_ERROR`, `SMAP_BD_TX_GENFCS`, `SMAP_BD_TX_GENPAD`, `SMAP_BD_TX_INSSA`, `SMAP_BD_TX_INSVLAN`, `SMAP_BD_TX_LCOLL`, `SMAP_BD_TX_LOSSCR`, `SMAP_BD_TX_MCOLL`, `SMAP_BD_TX_READY`, `SMAP_BD_TX_RPLSA`, `SMAP_BD_TX_RPLVLAN`, `SMAP_BD_TX_SCOLL`, `SMAP_BD_TX_SQE`, `SMAP_BD_TX_UNDERRUN`, `SMAP_DsPHYTER_10BTSCR`, `SMAP_DsPHYTER_ADDRESS`, `SMAP_DsPHYTER_ANAR`, `SMAP_DsPHYTER_ANER`, `SMAP_DsPHYTER_ANLPAR`, `SMAP_DsPHYTER_ANLPARNP`, `SMAP_DsPHYTER_ANNPTR`, `SMAP_DsPHYTER_BMCR`, `SMAP_DsPHYTER_BMSR`, `SMAP_DsPHYTER_CDCTRL`, `SMAP_DsPHYTER_FCSCR`, `SMAP_DsPHYTER_PCSR`, `SMAP_DsPHYTER_PHYCTRL`, `SMAP_DsPHYTER_PHYIDR1`, `SMAP_DsPHYTER_PHYIDR2`, `SMAP_DsPHYTER_PHYSTS`, `SMAP_DsPHYTER_RECR`, `SMAP_E3_ALLOW_EXTMNGIF`, `SMAP_E3_ALLOW_PF`, `SMAP_E3_DEAD_ALL`, `SMAP_E3_FDX_ENABLE`, `SMAP_E3_FLOWCTRL_ENABLE`, `SMAP_E3_HASH_MSK`, `SMAP_E3_IFGAP_MSK` …

### `common/include/speedregs.h`

Macros/constantes: `SPD_CAPS_ATA`, `SPD_CAPS_DVR`, `SPD_CAPS_FLASH`, `SPD_CAPS_SMAP`, `SPD_CAPS_UART`, `SPD_IF_ATA_RESET`, `SPD_IF_DMA_ENABLE`, `SPD_INTR_ATA`, `SPD_INTR_ATA0`, `SPD_INTR_ATA1`, `SPD_INTR_DVR`, `SPD_INTR_UART`, `SPD_PP_CSEL`, `SPD_PP_DIN`, `SPD_PP_DOUT`, `SPD_PP_OP_EWDS`, `SPD_PP_OP_EWEN`, `SPD_PP_OP_READ`, `SPD_PP_OP_WRITE`, `SPD_PP_SCLK`, `SPD_REG16`, `SPD_REG32`, `SPD_REG8`, `SPD_REGBASE`, `SPD_R_DMA_CTRL`, `SPD_R_IF_CTRL`, `SPD_R_INTR_MASK`, `SPD_R_INTR_STAT`, `SPD_R_MWDMA_MODE`, `SPD_R_PIO_DATA`, `SPD_R_PIO_DIR`, `SPD_R_PIO_MODE`, `SPD_R_REV`, `SPD_R_REV_1`, `SPD_R_REV_3`, `SPD_R_REV_8`, `SPD_R_UDMA_MODE`, `SPD_R_XFR_CTRL`, `USE_SPD_REGS`, `__SPEEDREGS_H__`

### `common/include/spu2_mmio_hwport.h`

Macros/constantes: `USE_SPU2_MMIO_HWPORT`, `__SPU2_MMIO_HWPORT__`

### `common/include/tamtypes.h`

Funções:

- `typedef volatile u128 vu128 __attribute__((mode(TI)))`
- `typedef volatile s128 vs128 __attribute__((mode(TI)))`

Macros/constantes: `NULL`, `__TAMTYPES_H__`

### `common/include/tcpip.h`

Funções:

- `typedef err_t(*netif_init_fn)(struct netif *netif)`
- `typedef err_t(*netif_input_fn)(struct pbuf *p, struct netif *inp)`
- `param p The packet to send(p->payload points to IP header) * @param ipaddr The IP address to which the packet shall be sent */ typedef err_t (*netif_output_fn)(struct netif *netif, struct pbuf *p, const ip4_addr_t *ipaddr)`
- `param p The packet to send(p->payload points to IP header) * @param ipaddr The IPv6 address to which the packet shall be sent */ typedef err_t (*netif_output_ip6_fn)(struct netif *netif, struct pbuf *p, const ip6_addr_t *ipaddr)`
- `param p The packet to send(raw ethernet packet) */ typedef err_t (*netif_linkoutput_fn)(struct netif *netif, struct pbuf *p)`
- `typedef void(*netif_status_callback_fn)(struct netif *netif)`
- `typedef err_t(*netif_igmp_mac_filter_fn)(struct netif *netif, const ip4_addr_t *group, enum netif_mac_filter_action action)`
- `typedef err_t(*netif_mld_mac_filter_fn)(struct netif *netif, const ip6_addr_t *group, enum netif_mac_filter_action action)`
- `u8 netif_alloc_client_data_id(void)`
- `void netif_ip6_addr_set_parts(struct netif *netif, s8_t addr_idx, u32 i0, u32 i1, u32 i2, u32 i3)`

Macros/constantes: `AF_INET`, `AF_INET6`, `AF_UNSPEC`, `AI_ADDRCONFIG`, `AI_ALL`, `AI_CANONNAME`, `AI_NUMERICHOST`, `AI_NUMERICSERV`, `AI_PASSIVE`, `AI_V4MAPPED`, `DNS_LOCAL_HOSTLIST`, `DNS_LOCAL_HOSTLIST_MAX_NAMELEN`, `DNS_MAX_SERVERS`, `DNS_TMR_INTERVAL`, `EAI_FAIL`, `EAI_FAMILY`, `EAI_MEMORY`, `EAI_NONAME`, `EAI_SERVICE`, `ENABLE_LOOPBACK`, `FDSETSAFEGET`, `FDSETSAFESET`, `FD_CLR`, `FD_ISSET`, `FD_SET`, `FD_SETSIZE`, `FD_ZERO`, `FIONBIO`, `FIONREAD`, `F_GETFL`, `F_SETFL`, `HOST_NOT_FOUND`, `IF__NETIF_CHECKSUM_ENABLED`, `INADDR_ANY`, `INADDR_BROADCAST`, `INADDR_LOOPBACK`, `INADDR_NONE`, `IOCPARM_MASK`, `IOC_IN`, `IOC_INOUT`, `IOC_OUT`, `IOC_VOID`, `IP4ADDR_STRLEN_MAX`, `IP4_ADDR`, `IP4_ADDR_ANY`, `IP4_ADDR_ANY4`, `IP4_ADDR_BROADCAST`, `IP6ADDR_STRLEN_MAX`, `IP6_ADDR`, `IP6_ADDR_ANY`, `IP6_ADDR_ANY6`, `IP6_ADDR_BLOCK1`, `IP6_ADDR_BLOCK2`, `IP6_ADDR_BLOCK3`, `IP6_ADDR_BLOCK4`, `IP6_ADDR_BLOCK5`, `IP6_ADDR_BLOCK6`, `IP6_ADDR_BLOCK7`, `IP6_ADDR_BLOCK8`, `IP6_ADDR_DEPRECATED` …

### `common/include/usbhdfsd-common.h`

Macros/constantes: `USBMASS_DEVCTL_STOP_ALL`, `USBMASS_DEVCTL_STOP_UNIT`, `USBMASS_DEV_STAT_CONF`, `USBMASS_DEV_STAT_CONN`, `USBMASS_DEV_STAT_ERR`, `USBMASS_IOCTL_CHECK_CHAIN`, `USBMASS_IOCTL_GET_CLUSTER`, `USBMASS_IOCTL_GET_DEVICE_NUMBER`, `USBMASS_IOCTL_GET_DRIVERNAME`, `USBMASS_IOCTL_GET_FRAGLIST`, `USBMASS_IOCTL_GET_LBA`, `USBMASS_IOCTL_RENAME`, `__USBHDFSD_COMMON_H__`

### `common/include/vif_codes.h`

Macros/constantes: `BASE_IMDT`, `DIRECTHL_IMDT`, `DIRECT_IMDT`, `ITOP_IMDT`, `MARK_IMDT`, `MPG_IMDT`, `MPG_NUM`, `MSCALF_IMDT`, `MSCAL_IMDT`, `MSKPATH3_IMDT`, `OFFSET_IMDT`, `PACK_VIFTAG`, `STCYCL_IMDT`, `STMOD_IMDT`, `UNPACK_IMDT`, `UNPACK_NUM`, `VIF_CMD_BASE`, `VIF_CMD_DIRECT`, `VIF_CMD_DIRECTHL`, `VIF_CMD_FLUSH`, `VIF_CMD_FLUSHA`, `VIF_CMD_FLUSHE`, `VIF_CMD_ITOP`, `VIF_CMD_MARK`, `VIF_CMD_MPG`, `VIF_CMD_MSCAL`, `VIF_CMD_MSCALF`, `VIF_CMD_MSCNT`, `VIF_CMD_MSKPATH3`, `VIF_CMD_NOP`, `VIF_CMD_OFFSET`, `VIF_CMD_STCOL`, `VIF_CMD_STCYCL`, `VIF_CMD_STMASK`, `VIF_CMD_STMOD`, `VIF_CMD_STROW`, `VIF_CMD_UNPACK`, `VIF_CODE`, `__VIF_CODES_H__`

### `common/include/vif_registers.h`

Macros/constantes: `VIF0_C0`, `VIF0_C1`, `VIF0_C2`, `VIF0_C3`, `VIF0_CODE`, `VIF0_CYCLE`, `VIF0_ERR`, `VIF0_FBRST`, `VIF0_FIFO`, `VIF0_ITOP`, `VIF0_ITOPS`, `VIF0_MARK`, `VIF0_MASK`, `VIF0_MODE`, `VIF0_NUM`, `VIF0_R0`, `VIF0_R1`, `VIF0_R2`, `VIF0_R3`, `VIF0_STAT`, `VIF0_TOP`, `VIF1_BASE`, `VIF1_C0`, `VIF1_C1`, `VIF1_C2`, `VIF1_C3`, `VIF1_CODE`, `VIF1_CYCLE`, `VIF1_ERR`, `VIF1_FBRST`, `VIF1_FIFO`, `VIF1_ITOP`, `VIF1_ITOPS`, `VIF1_MARK`, `VIF1_MASK`, `VIF1_MODE`, `VIF1_NUM`, `VIF1_OFST`, `VIF1_R0`, `VIF1_R1`, `VIF1_R2`, `VIF1_R3`, `VIF1_STAT`, `VIF1_TOP`, `VIF1_TOPS`, `VIF_SET_ERR`, `VIF_SET_FBRST`, `VIF_SET_MARK`, `VU0_MEM0_START`, `VU0_MICROMEM0_START`, `VU1_MEM1_START`, `VU1_MICROMEM1_START`, `__VIF_REGISTERS_H__`

### `ee/include/ahx_rpc.h`

Funções:

- `extern int AHX_Init()`
- `extern int AHX_LoadSongBuffer(char* songdata, int songsize)`
- `extern int AHX_LoadSong(char* filename)`
- `telling it to load * subsong(songNo). You can determine the number of subsongs * by checking the values returned by AHX_LoadSong() and * AHX_LoadSongBuffer()`
- `extern int AHX_SubSong(int songNo)`
- `extern int AHX_SetVolume(int volumePercentage)`
- `A boost value of 3 * is twice as load as 2 etc etc(ala DB) */ extern int AHX_SetBoost(int boostValue)`
- `use with * caution(or not at all) */ extern int AHX_ToggleOversampling()`
- `extern int AHX_Quit()`
- `extern int AHX_Play()`
- `extern int AHX_Pause()`

Macros/constantes: `AHX_INIT`, `AHX_IRX`, `AHX_LOADSONG`, `AHX_OVERSAMPLING`, `AHX_PAUSE`, `AHX_PLAY`, `AHX_QUIT`, `AHX_SETBOOST`, `AHX_SETVOLUME`, `AHX_SUBSONG`, `__AHX_H__`

### `ee/include/audsrv.h`

Funções:

- `extern int audsrv_init()`
- `extern int audsrv_quit()`
- `extern int audsrv_set_format(struct audsrv_fmt_t *fmt)`
- `param bytes size of chunk requested to be enqueued(in bytes) * @returns error code * * Blocks until there are enough space to store the upcoming chunk * in audsrv's internal ring buffer. */ extern int audsrv_wait_audio(int bytes)`
- `extern int audsrv_set_volume(int volume)`
- `The buffer and buflen are given * in host format(i.e, 11025hz 8bit stereo.) */ extern int audsrv_play_audio(const char *chunk, int bytes)`
- `extern int audsrv_stop_audio()`
- `extern int audsrv_get_error()`
- `Translates audsrv_get_error() response to readable string * @returns string representation of error code */ extern const char *audsrv_get_error_string()`
- `extern int audsrv_play_cd(int track)`
- `extern int audsrv_stop_cd()`
- `extern int audsrv_get_cdpos()`
- `extern int audsrv_get_trackpos()`
- `extern int audsrv_get_numtracks()`
- `extern int audsrv_get_track_offset(int track)`
- `extern int audsrv_pause_cd()`
- `extern int audsrv_resume_cd()`
- `extern int audsrv_play_sectors(int start, int end)`
- `Returns the status of the CD tray(open, closed, seeking etc.) * @returns value as defined in libcdvd, negative on error */ extern int audsrv_get_cd_status()`
- `extern int audsrv_get_cd_type()`
- `extern int audsrv_adpcm_init()`
- `param vol volume in percentage(0-100) * @param pan left/right offset [-100 .. 0 .. 100] * @returns 0 on success, negative otherwise */ extern int audsrv_adpcm_set_volume_and_pan(int ch, int vol, int pan)`
- `Plays an adpcm sample already uploaded with audsrv_load_adpcm() * @param ch channel identifier. Specifies one of the 24 voice channel to play the ADPCM channel on. * @param id sample identifier, as specified in load() * @returns channel identifier on success, negative value on error * * When ch is set to an invalid channel ID, the sample will be played in an unoccupied channel. * If all 24 channels are used, then -AUDSRV_ERR_NO_MORE_CHANNELS is returned. * When ch is set to a valid channel ID, -AUDSRV_ERR_NO_MORE_CHANNELS is returned if the channel is currently in use. * Trying to play a sample which is unavailable will result in -AUDSRV_ERR_ARGS */ extern int audsrv_ch_play_adpcm(int ch, audsrv_adpcm_t *adpcm)`
- `Remove an adpcm sample uploaded with audsrv_load_adpcm() from the list of loaded sounds * @param id sample identifier, as specified in load() * * SPU memory is freed only when there are no sounds in the list that where loaded after the ones that have been freed */ extern int audsrv_free_adpcm(audsrv_adpcm_t *adpcm)`
- `extern int audsrv_on_cdda_stop(audsrv_callback_t cb, void *arg)`
- `extern int audsrv_available()`
- `extern int audsrv_queued()`

Macros/constantes: `AUDSRV_ERR_ARGS`, `AUDSRV_ERR_FAILED_TO_CREATE_SEMA`, `AUDSRV_ERR_FAILED_TO_LOAD_ADPCM`, `AUDSRV_ERR_FORMAT_NOT_SUPPORTED`, `AUDSRV_ERR_NOERROR`, `AUDSRV_ERR_NOT_INITIALIZED`, `AUDSRV_ERR_NO_DISC`, `AUDSRV_ERR_NO_MORE_CHANNELS`, `AUDSRV_ERR_OUT_OF_MEMORY`, `AUDSRV_ERR_RPC_FAILED`, `AUDSRV_IRX`, `MAX_VOLUME`, `MIN_VOLUME`, `__AUDSRV_H__`, `audsrv_adpcm_set_volume`, `audsrv_play_adpcm`

### `ee/include/debug.h`

Funções:

- `extern void scr_printf(const char *, ...) __attribute__((format(printf,1,2)))`
- `extern void scr_vprintf(const char *format, va_list opt)`
- `extern void scr_putchar(int x, int y, u32 color, int ch)`
- `extern void ps2GetStackTrace(unsigned int* results,int max)`
- `extern void scr_setXY(int x, int y)`
- `extern int scr_getX(void)`
- `extern int scr_getY(void)`
- `extern void scr_clear(void)`
- `extern void scr_clearline(int Y)`
- `extern void scr_clearchar(int X, int Y)`
- `extern void scr_setbgcolor(u32 color)`
- `extern void scr_setfontcolor(u32 color)`
- `extern void scr_setcursorcolor(u32 color)`
- `extern void scr_setCursor(int enable)`
- `extern int scr_getCursor(void)`
- `extern void scr_change_defaultcolor(int index, u32 color)`

Macros/constantes: `DEBUG_BGCOLOR`, `__DEBUG_H__`

### `ee/include/deci2.h`

Funções:

- `int sceDeci2Open(unsigned short protocol, void *opt, void (*handler)(int event, int param, void *opt))`
- `int sceDeci2Close(int s)`
- `int sceDeci2ReqSend(int s, char dest)`
- `void sceDeci2Poll(int s)`
- `int sceDeci2ExRecv(int s, void *buf, unsigned short len)`
- `int sceDeci2ExSend(int s, void *buf, unsigned short len)`
- `int sceDeci2ExReqSend(int s, char dest)`
- `int sceDeci2ExLock(int s)`
- `int sceDeci2ExUnLock(int s)`
- `int kputs(char *s)`

Macros/constantes: `DECI2_H_`

### `ee/include/delaythread.h`

Macros/constantes: `__DELAYTHREAD_H__`

### `ee/include/dma.h`

Funções:

- `extern int dma_channel_initialize(int channel, void *handler, int flags)`
- `extern void dma_channel_fast_waits(int channel)`
- `extern void dma_wait_fast(void)`
- `extern int dma_channel_wait(int channel, int timeout)`
- `extern void dma_channel_send_packet2(packet2_t *packet2, int channel, u8 flush_cache)`
- `extern int dma_channel_send_chain(int channel, void *data, int qwc, int flags, int spr)`
- `extern int dma_channel_send_chain_ucab(int channel, void *data, int qwc, int flags)`
- `extern int dma_channel_send_normal(int channel, void *data, int qwc, int flags, int spr)`
- `extern int dma_channel_send_normal_ucab(int channel, void *data, int qwc, int flags)`
- `extern int dma_channel_receive_normal(int channel, void *data, int data_size, int flags, int spr)`
- `extern int dma_channel_receive_chain(int channel, void *data, int data_size, int flags, int spr)`
- `extern int dma_channel_shutdown(int channel, int flags)`

Macros/constantes: `DMA_CHANNEL_GIF`, `DMA_CHANNEL_SIF2`, `DMA_CHANNEL_VIF0`, `DMA_CHANNEL_VIF1`, `DMA_CHANNEL_fromIPU`, `DMA_CHANNEL_fromSIF0`, `DMA_CHANNEL_fromSPR`, `DMA_CHANNEL_toIPU`, `DMA_CHANNEL_toSIF1`, `DMA_CHANNEL_toSPR`, `DMA_FLAG_INTERRUPTSAFE`, `DMA_FLAG_TRANSFERTAG`, `__DMA_H__`

### `ee/include/dma_registers.h`

Macros/constantes: `DMAE_DISABLE`, `DMAE_ENABLE`, `DMA_REG_CTRL`, `DMA_REG_ENABLER`, `DMA_REG_ENABLEW`, `DMA_REG_PCR`, `DMA_REG_RBOR`, `DMA_REG_RBSR`, `DMA_REG_SQWC`, `DMA_REG_STADR`, `DMA_REG_STAT`, `DMA_SET_ASR0`, `DMA_SET_ASR1`, `DMA_SET_CHCR`, `DMA_SET_CTRL`, `DMA_SET_ENABLER`, `DMA_SET_ENABLEW`, `DMA_SET_MADR`, `DMA_SET_PCR`, `DMA_SET_QWC`, `DMA_SET_RBOR`, `DMA_SET_RBSR`, `DMA_SET_SADR`, `DMA_SET_SQWC`, `DMA_SET_STADR`, `DMA_SET_STAT`, `DMA_SET_TADR`, `MFD_GIF`, `MFD_OFF`, `MFD_RES`, `MFD_VIF`, `RCYC_128`, `RCYC_16`, `RCYC_256`, `RCYC_32`, `RCYC_64`, `RCYC_8`, `RELE_OFF`, `RELE_ON`, `STD_GIF`, `STD_OFF`, `STD_SIF`, `STD_VIF`, `STS_IPU`, `STS_SIF`, `STS_SPR`, `STS_UNSPEC`, `__DMA_REGISTERS_H__`

### `ee/include/dma_tags.h`

Macros/constantes: `DMATAG`, `DMATAG_CALL`, `DMATAG_CNT`, `DMATAG_CNTS`, `DMATAG_END`, `DMATAG_NEXT`, `DMATAG_REF`, `DMATAG_REFE`, `DMATAG_REFS`, `DMATAG_RET`, `DMA_TAG_CALL`, `DMA_TAG_CNT`, `DMA_TAG_CNTS`, `DMA_TAG_END`, `DMA_TAG_NEXT`, `DMA_TAG_REF`, `DMA_TAG_REFE`, `DMA_TAG_REFS`, `DMA_TAG_RET`, `PACK_DMATAG`, `__DMA_TAGS_H__`

### `ee/include/draw.h`

Funções:

- `extern qword_t * draw_setup_environment(qword_t *q, int context, framebuffer_t *frame, zbuffer_t *z)`
- `extern qword_t * draw_clear(qword_t *q, int context, float x, float y, float width, float height, int r, int g, int b)`
- `extern qword_t * draw_finish(qword_t *q)`
- `extern void draw_wait_finish(void)`
- `extern qword_t * draw_texture_transfer(qword_t *q, void *src, int width, int height, int psm, int dest, int dest_width)`
- `extern qword_t * draw_texture_flush(qword_t *q)`

Macros/constantes: `DRAW_DISABLE`, `DRAW_ENABLE`, `__DRAW_H__`

### `ee/include/draw2d.h`

Funções:

- `extern void draw_enable_blending()`
- `extern void draw_disable_blending()`
- `extern qword_t * draw_point(qword_t *q, int context, point_t *point)`
- `extern qword_t * draw_line(qword_t *q, int context, line_t *line)`
- `extern qword_t * draw_triangle_outline(qword_t *q, int context, triangle_t *triangle)`
- `extern qword_t * draw_triangle_filled(qword_t *q, int context,triangle_t *triangle)`
- `extern qword_t * draw_rect_outline(qword_t *q, int context, rect_t *rect)`
- `extern qword_t * draw_rect_filled(qword_t *q, int context, rect_t *rect)`
- `extern qword_t * draw_rect_textured(qword_t *q, int context, texrect_t *rect)`
- `extern qword_t * draw_rect_filled_strips(qword_t *q, int context, rect_t *rect)`
- `extern qword_t * draw_rect_textured_strips(qword_t *q, int context, texrect_t *rect)`
- `extern qword_t * draw_round_rect_filled(qword_t *q, int context, rect_t *rect)`
- `extern qword_t * draw_round_rect_outline(qword_t *q, int context, rect_t *rect)`
- `extern qword_t * draw_arc_outline(qword_t *q, int context, point_t *center, float radius, float angle_start, float angle_end)`
- `extern qword_t * draw_arc_filled(qword_t *q, int context, point_t *center, float radius, float angle_start, float angle_end)`

Macros/constantes: `__DRAW2D_H__`

### `ee/include/draw3d.h`

Funções:

- `extern qword_t * draw_prim_start(qword_t *q, int context, const prim_t *prim, const color_t *color)`
- `extern qword_t * draw_prim_end(qword_t *q,int nreg, u64 reglist)`
- `extern int draw_convert_rgbq(color_t *output, int count, vertex_f_t *vertices, color_f_t *colours, unsigned char alpha)`
- `extern int draw_convert_rgbaq(color_t *output, int count, vertex_f_t *vertices, color_f_t *colours)`
- `extern int draw_convert_st(texel_t *output, int count, vertex_f_t *vertices, texel_f_t *coords)`
- `extern int draw_convert_xyz(xyz_t *output, float x, float y, int z, int count, vertex_f_t *vertices)`

Macros/constantes: `DRAW_RGBAQ_REGLIST`, `DRAW_RGBAQ_UV_REGLIST`, `DRAW_STQ2_REGLIST`, `DRAW_STQ_REGLIST`, `DRAW_UV_REGLIST`, `DRAW_XYZ_REGLIST`, `__DRAW3D_H__`

### `ee/include/draw_blending.h`

Funções:

- `extern qword_t * draw_pixel_alpha_control(qword_t *q, int enable)`
- `extern qword_t * draw_alpha_blending(qword_t *q, int context, blend_t *blend)`
- `extern qword_t * draw_alpha_correction(qword_t *q, int context, int alpha)`

Macros/constantes: `ALPHA_CORRECT_RGBA16`, `ALPHA_CORRECT_RGBA32`, `BLEND_ALPHA_DEST`, `BLEND_ALPHA_FIXED`, `BLEND_ALPHA_SOURCE`, `BLEND_COLOR_DEST`, `BLEND_COLOR_SOURCE`, `BLEND_COLOR_ZERO`, `__DRAW_BLENDING_H__`

### `ee/include/draw_buffers.h`

Funções:

- `extern unsigned char draw_log2(unsigned int x)`
- `extern qword_t * draw_framebuffer(qword_t *q, int context, framebuffer_t *frame)`
- `extern qword_t * draw_zbuffer(qword_t *q, int context, zbuffer_t *zbuffer)`
- `extern qword_t * draw_texturebuffer(qword_t *q, int context, texbuffer_t *texbuffer, clutbuffer_t *clut)`
- `extern qword_t * draw_clutbuffer(qword_t *q, int context, int psm, clutbuffer_t *clut)`
- `extern qword_t * draw_clut_offset(qword_t *q, int cbw, int u, int v)`

Macros/constantes: `CLUT_COMPARE_CBP0`, `CLUT_COMPARE_CBP1`, `CLUT_LOAD`, `CLUT_LOAD_COPY_CBP0`, `CLUT_LOAD_COPY_CBP1`, `CLUT_NO_LOAD`, `CLUT_STORAGE_MODE1`, `CLUT_STORAGE_MODE2`, `TEXTURE_COMPONENTS_RGB`, `TEXTURE_COMPONENTS_RGBA`, `TEXTURE_FUNCTION_DECAL`, `TEXTURE_FUNCTION_HIGHLIGHT`, `TEXTURE_FUNCTION_HIGHLIGHT2`, `TEXTURE_FUNCTION_MODULATE`, `__DRAW_BUFFERS_H__`

### `ee/include/draw_dithering.h`

Funções:

- `extern qword_t * draw_dithering(qword_t *q, int enable)`
- `extern qword_t * draw_dither_matrix(qword_t *q,char *dm)`

Macros/constantes: `__DRAW_DITHERING_H__`

### `ee/include/draw_fog.h`

Funções:

- `extern qword_t * draw_fog_color(qword_t *q, unsigned char r, unsigned char g, unsigned char b)`

Macros/constantes: `__DRAW_FOG_H__`

### `ee/include/draw_masking.h`

Funções:

- `Scanline Masking(framebuffer) */ extern qword_t *draw_scan_masking(qword_t *q, int mask)`
- `extern qword_t * draw_color_clamping(qword_t *q, int enable)`

Macros/constantes: `COLOR_CLAMP_ENABLE`, `COLOR_CLAMP_MASK`, `SCAN_MASK_EVEN`, `SCAN_MASK_NORMAL`, `SCAN_MASK_ODD`, `__DRAW_MASKING_H__`

### `ee/include/draw_primitives.h`

Funções:

- `extern qword_t * draw_primitive_xyoffset(qword_t *q, int context, float x, float y)`
- `extern qword_t * draw_primitive_override(qword_t *q, int mode)`
- `extern qword_t * draw_primitive_override_setting(qword_t *q, int context, prim_t *prim)`

Macros/constantes: `PRIM_FIXED`, `PRIM_LINE`, `PRIM_LINE_STRIP`, `PRIM_MAP_ST`, `PRIM_MAP_UV`, `PRIM_OVERRIDE_DISABLE`, `PRIM_OVERRIDE_ENABLE`, `PRIM_POINT`, `PRIM_SHADE_FLAT`, `PRIM_SHADE_GOURAUD`, `PRIM_SPRITE`, `PRIM_TRIANGLE`, `PRIM_TRIANGLE_FAN`, `PRIM_TRIANGLE_STRIP`, `PRIM_UNFIXED`, `__DRAW_PRIMITIVES_H__`

### `ee/include/draw_sampling.h`

Funções:

- `extern qword_t * draw_texture_sampling(qword_t *q, int context, lod_t *lod)`
- `extern qword_t * draw_mipmap1(qword_t *q, int context, mipmap_t *mipmap)`
- `extern qword_t * draw_mipmap2(qword_t *q, int context, mipmap_t *mipmap)`
- `extern qword_t * draw_texture_wrapping(qword_t *q, int context, texwrap_t *wrap)`
- `extern qword_t * draw_texture_expand_alpha(qword_t *q, unsigned char zero_value, int expand, unsigned char one_value)`

Macros/constantes: `ALPHA_EXPAND_NORMAL`, `ALPHA_EXPAND_TRANSPARENT`, `LOD_FORMULAIC`, `LOD_MAG_LINEAR`, `LOD_MAG_NEAREST`, `LOD_MIN_LINEAR`, `LOD_MIN_LINE_MIPMAP_LINE`, `LOD_MIN_LINE_MIPMAP_NEAR`, `LOD_MIN_NEAREST`, `LOD_MIN_NEAR_MIPMAP_LINE`, `LOD_MIN_NEAR_MIPMAP_NEAR`, `LOD_MIPMAP_CALCULATE`, `LOD_MIPMAP_REGISTER`, `LOD_USE_K`, `WRAP_CLAMP`, `WRAP_REGION_CLAMP`, `WRAP_REGION_REPEAT`, `WRAP_REPEAT`, `__DRAW_SAMPLING_H__`

### `ee/include/draw_tests.h`

Funções:

- `extern qword_t * draw_scissor_area(qword_t *q, int context, int x0, int x1, int y0, int y1)`
- `extern qword_t * draw_pixel_test(qword_t *q, int context, atest_t *atest, dtest_t *dtest, ztest_t *ztest)`
- `extern qword_t * draw_disable_tests(qword_t *q, int context, zbuffer_t *z)`
- `extern qword_t * draw_enable_tests(qword_t *q, int context, zbuffer_t *z)`

Macros/constantes: `ATEST_KEEP_ALL`, `ATEST_KEEP_ALPHA`, `ATEST_KEEP_FRAMEBUFFER`, `ATEST_KEEP_ZBUFFER`, `ATEST_METHOD_ALLFAIL`, `ATEST_METHOD_ALLPASS`, `ATEST_METHOD_EQUAL`, `ATEST_METHOD_GREATER`, `ATEST_METHOD_GREATER_EQUAL`, `ATEST_METHOD_LESS`, `ATEST_METHOD_LESS_EQUAL`, `ATEST_METHOD_NOTEQUAL`, `DTEST_METHOD_PASS_ONE`, `DTEST_METHOD_PASS_ZERO`, `ZTEST_METHOD_ALLFAIL`, `ZTEST_METHOD_ALLPASS`, `ZTEST_METHOD_GREATER`, `ZTEST_METHOD_GREATER_EQUAL`, `__DRAW_TESTS_H__`

### `ee/include/draw_types.h`

Macros/constantes: `__DRAW_TYPES_H__`, `ftoi4`

### `ee/include/ee_debug.h`

Funções:

- `extern int ee_dbg_install(int levels)`
- `extern int ee_dbg_remove(int levels)`
- `extern EE_ExceptionHandler * ee_dbg_get_level1_handler(int cause)`
- `extern EE_ExceptionHandler * ee_dbg_set_level1_handler(int cause, EE_ExceptionHandler *handler)`
- `extern EE_ExceptionHandler * ee_dbg_get_level2_handler(int cause)`
- `extern EE_ExceptionHandler * ee_dbg_set_level2_handler(int cause, EE_ExceptionHandler *handler)`
- `extern u32 ee_dbg_get_bpc(void)`
- `extern void ee_dbg_set_bpc(u32)`
- `extern u32 ee_dbg_get_iab(void)`
- `extern void ee_dbg_set_iab(u32)`
- `extern u32 ee_dbg_get_iabm(void)`
- `extern void ee_dbg_set_iabm(u32)`
- `extern u32 ee_dbg_get_dab(void)`
- `extern void ee_dbg_set_dab(u32)`
- `extern u32 ee_dbg_get_dabm(void)`
- `extern void ee_dbg_set_dabm(u32)`
- `extern u32 ee_dbg_get_dvb(void)`
- `extern void ee_dbg_set_dvb(u32)`
- `extern u32 ee_dbg_get_dvbm(void)`
- `extern void ee_dbg_set_dvbm(u32)`
- `extern void ee_dbg_set_bpr(u32 addr, u32 mask, u32 opmode_mask)`
- `extern void ee_dbg_set_bpw(u32 addr, u32 mask, u32 opmode_mask)`
- `extern void ee_dbg_set_bpv(u32 value, u32 mask, u32 opmode_mask)`
- `extern void ee_dbg_set_bpx(u32 addr, u32 mask, u32 opmode_mask)`
- `extern void ee_dbg_clr_bps(void)`
- `extern void ee_dbg_clr_bpda(void)`
- `extern void ee_dbg_clr_bpdv(void)`
- `extern void ee_dbg_clr_bpx(void)`

Macros/constantes: `__EE_DEBUG_H__`

### `ee/include/elf-loader.h`

Funções:

- `ore call this method be sure that you have previously called sbv_patch_disable_prefix_check()`
- `extern int LoadELFFromFile(const char *filename, int argc, char *argv[])`
- `extern int LoadELFFromFileWithPartition(const char *filename, const char *partition, int argc, char *argv[])`

Macros/constantes: `__ELFLOADER_H__`

### `ee/include/erl.h`

Funções:

- `extern struct erl_record_t * load_erl_from_mem(u8 * mem, int argc, char ** argv)`
- `extern struct erl_record_t * load_erl_from_file(const char * fname, int argc, char ** argv)`
- `extern struct erl_record_t * _init_load_erl_from_file(const char * fname, char * erl_id)`
- `extern struct erl_record_t * load_erl_from_file_to_addr(const char * fname, u32 addr, int argc, char ** argv)`
- `extern struct erl_record_t * _init_load_erl_from_file_to_addr(const char * fname, u32 addr, char * erl_id)`
- `extern int unload_erl(struct erl_record_t * erl)`
- `extern int erl_add_global_symbol(const char * symbol, u32 address)`
- `extern struct erl_record_t * find_erl(const char * name)`
- `extern struct erl_record_t * erl_resolve(u32 address)`
- `extern struct symbol_t * erl_find_local_symbol(const char * symbol, struct erl_record_t * erl)`
- `extern struct symbol_t * erl_find_symbol(const char * symbol)`
- `extern void erl_flush_symbols(struct erl_record_t * erl)`

Macros/constantes: `ERL_DYN_ADDR`, `ERL_FLAG_CLEAR`, `ERL_FLAG_STATIC`, `ERL_FLAG_STICKY`, `__ERL_H__`

### `ee/include/fileXio_rpc.h`

Funções:

- `extern void fileXioExit(void)`
- `extern void fileXioSetBlockMode(int blocking)`
- `extern int fileXioWaitAsync(int mode, int *retVal)`
- `extern void fileXioStop()`
- `extern int fileXioGetDeviceList(struct fileXioDevice deviceEntry[], unsigned int req_entries)`
- `extern int fileXioGetdir(const char* pathname, struct fileXioDirEntry DirEntry[], unsigned int req_entries)`
- `extern int fileXioMount(const char* mountpoint, const char* blockdev, int flag)`
- `extern int fileXioUmount(const char* mountpoint)`
- `extern int fileXioCopyfile(const char* source, const char* dest, int mode)`
- `extern int fileXioMkdir(const char* pathname, int mode)`
- `extern int fileXioRmdir(const char* pathname)`
- `extern int fileXioRemove(const char* pathname)`
- `extern int fileXioRename(const char* source, const char* dest)`
- `extern int fileXioSymlink(const char* source, const char* dest)`
- `extern int fileXioReadlink(const char* source, char* buf, unsigned int buflen)`
- `extern int fileXioChdir(const char* pathname)`
- `extern int fileXioOpen(const char* source, int flags, ...)`
- `extern int fileXioClose(int fd)`
- `extern int fileXioRead(int fd, void *buf, int size)`
- `extern int fileXioWrite(int fd, const void *buf, int size)`
- `extern int fileXioLseek(int fd, int offset, int whence)`
- `extern s64 fileXioLseek64(int fd, s64 offset, int whence)`
- `extern int fileXioChStat(const char *name, iox_stat_t *stat, int mask)`
- `extern int fileXioGetStat(const char *name, iox_stat_t *stat)`
- `extern int fileXioFormat(const char *dev, const char *blockdev, const void *args, int arglen)`
- `extern int fileXioSync(const char *devname, int flag)`
- `extern int fileXioDopen(const char *name)`
- `extern int fileXioDclose(int fd)`
- `extern int fileXioDread(int fd, iox_dirent_t *dirent)`
- `extern int fileXioDevctl(const char *name, int cmd, void *arg, unsigned int arglen, void *buf,unsigned int buflen)`
- `extern int fileXioIoctl(int fd, int cmd, void *arg)`
- `extern int fileXioIoctl2(int fd, int command, void *arg, unsigned int arglen, void *buf, unsigned int buflen)`
- `extern int fileXioSetRWBufferSize(int size)`

Macros/constantes: `FXIO_COMPLETE`, `FXIO_INCOMPLETE`, `FXIO_NOWAIT`, `FXIO_WAIT`, `__FILEXIO_RPC_H__`

### `ee/include/fileio.h`

Funções:

- `extern void fioExit(void)`
- `extern int fioOpen(const char *fname, int mode)`
- `extern int fioClose(int fd)`
- `extern int fioRead(int fd, void *buff, int buff_size)`
- `extern int fioWrite(int fd, const void *buff, int buff_size)`
- `extern int fioLseek(int fd, int offset, int whence)`
- `extern int fioMkdir(const char *dirname)`
- `extern int fioPutc(int fd, int c)`
- `extern int fioGetc(int fd)`
- `extern int fioGets(int fd, char *buff, int n)`
- `extern void fioSetBlockMode(int blocking)`
- `extern int fioSync(int mode, int *retVal)`
- `extern int fioIoctl(int fd, int request, void *data)`
- `extern int fioDopen(const char *name)`
- `extern int fioDclose(int fd)`
- `extern int fioDread(int fd, io_dirent_t *buf)`
- `extern int fioGetstat(const char *name, io_stat_t *buf)`
- `extern int fioChstat(const char *name, io_stat_t *buf, unsigned int cbit)`
- `extern int fioRemove(const char *name)`
- `extern int fioFormat(const char *name)`
- `extern int fioRmdir(const char *dirname)`

Macros/constantes: `EOF`, `FIO_COMPLETE`, `FIO_INCOMPLETE`, `FIO_NOWAIT`, `FIO_WAIT`, `__FILEIO_H__`

### `ee/include/font.h`

Funções:

- `would take roughly 2 kilobytes to draw(16x16)*8 bytes /** Loads a FontX2 type font with set characteristics * Use "rom0:KROM" as the path to load the PS2's internal FontX2 font */ extern int fontx_load(const char *path, fontx_t* fontx, int type, int wmargin, int hmargin, int bold)`
- `extern void fontx_unload(fontx_t *fontx)`
- `extern qword_t * fontx_print_ascii(qword_t *q, int context, const unsigned char *str, int alignment, const vertex_t *v0, const color_t *c0, fontx_t *fontx)`
- `extern qword_t * fontx_print_sjis(qword_t *q, int context, const unsigned char *str, int alignment, const vertex_t *v0, const color_t *c0, fontx_t *ascii, fontx_t *kanji)`
- `extern fsfont_t * fontstudio_init(int height)`
- `extern void fontstudio_free(fsfont_t *font)`
- `extern int fontstudio_parse_ini(fsfont_t *font, char *ini, float tex_width, float tex_height)`
- `extern char * fontstudio_load_ini(const char *path)`
- `extern void fontstudio_unload_ini(fsfont_t *font)`

Macros/constantes: `CENTER_ALIGN`, `DOUBLE_BYTE`, `LEFT_ALIGN`, `RIGHT_ALIGN`, `SINGLE_BYTE`, `__FONT_H__`

### `ee/include/graph.h`

Funções:

- `extern int graph_initialize(int fbp, int width, int height, int psm, int x, int y)`
- `extern int graph_get_region(void)`
- `extern float graph_aspect_ratio(void)`
- `extern void graph_enable_output(void)`
- `extern void graph_disable_output(void)`
- `extern int graph_set_mode(int interlace, int mode, int ffmd, int flicker_filter)`
- `extern int graph_set_screen(int x, int y, int width, int height)`
- `extern void graph_set_framebuffer_filtered(int fbp, int width, int psm, int x, int y)`
- `extern void graph_set_framebuffer(int context, int fbp, int width, int psm, int x, int y)`
- `extern void graph_set_bgcolor(unsigned char r, unsigned char g, unsigned char b)`
- `extern void graph_set_output(int rc1, int rc2, int alpha_select, int alpha_output, int blend_method, unsigned char alpha)`
- `extern int graph_add_vsync_handler(int (*vsync_callback)(int cause))`
- `extern void graph_remove_vsync_handler(int callback_id)`
- `extern void graph_wait_hsync(void)`
- `extern void graph_wait_vsync(void)`
- `extern int graph_check_vsync(void)`
- `extern void graph_start_vsync(void)`
- `extern int graph_shutdown(void)`
- `void graph_set_smode1(char cmod, char gcont)`

Macros/constantes: `GRAPH_BLEND_BGCOLOR`, `GRAPH_BLEND_RC2`, `GRAPH_CMOD_NTSC`, `GRAPH_CMOD_PAL`, `GRAPH_DISABLE`, `GRAPH_ENABLE`, `GRAPH_FIELD_EVEN`, `GRAPH_FIELD_ODD`, `GRAPH_GCONT_RGB`, `GRAPH_GCONT_YCRCB`, `GRAPH_MODE_AUTO`, `GRAPH_MODE_FIELD`, `GRAPH_MODE_FRAME`, `GRAPH_MODE_HDTV_1080I`, `GRAPH_MODE_HDTV_480P`, `GRAPH_MODE_HDTV_576P`, `GRAPH_MODE_HDTV_720P`, `GRAPH_MODE_INTERLACED`, `GRAPH_MODE_NONINTERLACED`, `GRAPH_MODE_NTSC`, `GRAPH_MODE_PAL`, `GRAPH_MODE_VGA_1024_60`, `GRAPH_MODE_VGA_1024_70`, `GRAPH_MODE_VGA_1024_75`, `GRAPH_MODE_VGA_1024_85`, `GRAPH_MODE_VGA_1280_60`, `GRAPH_MODE_VGA_1280_75`, `GRAPH_MODE_VGA_640_60`, `GRAPH_MODE_VGA_640_72`, `GRAPH_MODE_VGA_640_75`, `GRAPH_MODE_VGA_640_85`, `GRAPH_MODE_VGA_800_56`, `GRAPH_MODE_VGA_800_60`, `GRAPH_MODE_VGA_800_72`, `GRAPH_MODE_VGA_800_75`, `GRAPH_MODE_VGA_800_85`, `GRAPH_RC1_ALPHA`, `GRAPH_RC2_ALPHA`, `GRAPH_VALUE_ALPHA`, `GRAPH_VALUE_RC1`, `__GRAPH_H__`

### `ee/include/graph_config.h`

Funções:

- `extern int graph_make_config(int mode, int interlace, int ffmd, int x, int y, int flicker_filter, char *config)`
- `extern int graph_get_config(char *config)`
- `extern int graph_set_config(char *config)`
- `extern int graph_load_config(char *filename)`
- `extern int graph_save_config(char *filename)`

Macros/constantes: `__GRAPH_CONFIG_H__`

### `ee/include/graph_vram.h`

Funções:

- `extern int graph_vram_allocate(int width, int height, int psm, int alignment)`
- `extern void graph_vram_free(int address)`
- `extern void graph_vram_clear(void)`
- `extern int graph_vram_size(int width, int height, int psm, int alignment)`

Macros/constantes: `GRAPH_ALIGN_BLOCK`, `GRAPH_ALIGN_PAGE`, `GRAPH_VRAM_MAX_WORDS`, `__GRAPH_VRAM_H__`

### `ee/include/hashtab.h`

Funções:

- `extern htab * hcreate(int logsize)`
- `extern void hdestroy(htab *t)`
- `extern int hdel(htab *t)`
- `word hnext(/o_ htab *t _o/)`
- `extern int hnbucket(htab *t)`
- `extern void hstat(/*_ htab *t _*/)`

Macros/constantes: `hcount`, `hkey`, `hkeyl`, `hnext`, `hstuff`

### `ee/include/hwbp.h`

Funções:

- `extern void InitBPC(void)`
- `extern void SetInstructionBP(u32 addr, u32 mask, u32 options)`
- `extern void SetDataAddrBP(u32 addr, u32 mask, u32 options)`
- `extern void SetDataValueBP(u32 addr, u32 mask, u32 value, u32 vmask, u32 options)`
- `extern u32 GetBPC(void)`
- `extern void SetBPC(u32 bpc)`
- `extern u32 GetIAB(void)`
- `extern void SetIAB(u32 val)`
- `extern u32 GetIABM(void)`
- `extern void SetIABM(u32 val)`
- `extern u32 GetDAB(void)`
- `extern void SetDAB(u32 val)`
- `extern u32 GetDABM(void)`
- `extern void SetDABM(u32 val)`
- `extern u32 GetDVB(void)`
- `extern void SetDVB(u32 val)`
- `extern u32 GetDVBM(void)`
- `extern void SetDVBM(u32 val)`

Macros/constantes: `BPC_BED`, `BPC_DKE`, `BPC_DRB`, `BPC_DRE`, `BPC_DSE`, `BPC_DTE`, `BPC_DUE`, `BPC_DVE`, `BPC_DWB`, `BPC_DWE`, `BPC_DXE`, `BPC_IAB`, `BPC_IAE`, `BPC_IKE`, `BPC_ISE`, `BPC_ITE`, `BPC_IUE`, `BPC_IXE`, `__HWBP_H__`

### `ee/include/input.h`

Funções:

- `extern pad_t * pad_open(unsigned int port, unsigned int slot, unsigned int mode, unsigned int lock)`
- `extern void pad_close(pad_t *pad)`
- `extern void pad_wait(pad_t *pad)`
- `extern void pad_print_state(pad_t *pad)`
- `extern void pad_print_supported_modes(pad_t *pad)`
- `extern void pad_set_mode(pad_t *pad, int mode, int lock)`
- `extern void pad_set_sensitivity(pad_t *pad, int enable)`
- `extern void pad_init_actuators(pad_t *pad)`
- `extern void pad_set_actuators(pad_t *pad, int small, unsigned char large)`
- `extern int pad_get_state(pad_t *pad)`
- `extern int pad_get_num_modes(pad_t *pad)`
- `extern int pad_has_type(pad_t *pad, int type)`
- `extern int pad_get_type(pad_t *pad)`
- `extern int pad_get_mode(pad_t *pad)`

Macros/constantes: `MODE_ANALOG`, `MODE_DIGITAL`, `MODE_LOCKED`, `MODE_UNLOCKED`, `PAD_CIRCLE`, `PAD_CROSS`, `PAD_DOWN`, `PAD_L1`, `PAD_L2`, `PAD_L3`, `PAD_LEFT`, `PAD_R1`, `PAD_R2`, `PAD_R3`, `PAD_RIGHT`, `PAD_SELECT`, `PAD_SQUARE`, `PAD_START`, `PAD_TRIANGLE`, `PAD_UP`, `__INPUT_H__`

### `ee/include/iopcontrol.h`

Funções:

- `extern int SifIopReboot(const char *arg)`
- `extern int SifIopReset(const char *arg, int mode)`
- `extern int SifIopIsAlive(void)`
- `extern int SifIopSync(void)`

Macros/constantes: `__IOPCONTROL_H__`

### `ee/include/iopcontrol_special.h`

Funções:

- `extern int SifIopRebootBufferEncrypted(const void *udnl, int size)`
- `extern int SifIopRebootBuffer(const void *ioprp, int size)`

Macros/constantes: `__IOPCONTROL_SPECIAL_H__`

### `ee/include/iopheap.h`

Funções:

- `extern void SifExitIopHeap(void)`
- `extern void * SifAllocIopHeap(int size)`
- `extern int SifFreeIopHeap(void *addr)`
- `extern int SifLoadIopHeap(const char *path, void *addr)`

Macros/constantes: `__IOPHEAP_H__`

### `ee/include/ioprpgen.h`

Funções:

- `typedef int(*ioprpgen_write_cb_t)(void *userdata, const struct ioprpgen_ctx *ctx, const void *buf, u32 size)`
- `param memwrite_ctx Pointer to memwrite context(ensure it has the same lifetime as ctx) * @param buf Output buffer * @param size Size of the output buffer * @return 1 on success, 0 on failure. */ extern void ioprpgen_setup_membuf(struct ioprpgen_ctx *ctx, struct ioprpgen_memwrite_ctx *memwrite_ctx, void *buf, u32 size)`
- `extern u32 ioprpgen_write_ioprp(const struct ioprpgen_ctx *ctx, const struct ioprpgen_entry *entries)`

Macros/constantes: `__IOPRPGEN__`

### `ee/include/kernel.h`

Funções:

- `Gustavo Scotti(gustavo@scotti.com) # (c) 2003 Marcus R. Brown <mrbrown@0xd6.org> # Licenced under Academic Free License version 2.0 # Review ps2sdk README & LICENSE files for further details. */ /** * @file * EE Kernel prototypes */ #ifndef __KERNEL_H__ #define __KERNEL_H__ #include <stddef.h> #include <stdarg.h> #include <sifdma.h> #include <mipscopaccess.h> #define DI DIntr #define EI EIntr // Workaround for EE kernel bug: call this immediately before returning from any interrupt handler. #define ExitHandler() __asm__ __volatile__("sync\nei\n") // note: 'sync' is the same as 'sync.l' #define EE_SYNC() __asm__ __volatile__("sync") #define EE_SYNCL() __asm__ __volatile__("sync.l") #define EE_SYNCP() __asm__ __volatile__("sync.p") #define UNCACHED_SEG(x) \ ((void *)(((u32)(x)) | 0x20000000)) #define IS_UNCACHED_SEG(x) \ (((u32)(x)) & 0x20000000) #define UCAB_SEG(x) \ ((void *)(((u32)(x)) | 0x30000000)) #define PUSHDATA(t, x, v, l) \ *(t *)(x) = (v)`
- `extern void SetGP(void *gp)`
- `extern void * GetGP(void)`
- `__asm__ __volatile__("nop\nnop\nnop\nnop\nnop\n")`
- `extern void _InitSys(void)`
- `Run by crt0 extern void TerminateLibrary(void)`
- `extern int InitThread(void)`
- `Run by _InitSys extern s32 iWakeupThread(s32 thread_id)`
- `extern s32 iRotateThreadReadyQueue(s32 priority)`
- `extern s32 iSuspendThread(s32 thread_id)`
- `extern void InitTLBFunctions(void)`
- `Run by _InitSys extern void InitTLB(void)`
- `extern void Exit(s32 exit_code) __attribute__((noreturn))`
- `extern s32 ExecPS2(void *entry, void *gp, int num_args, char *args[])`
- `extern void LoadExecPS2(const char *filename, s32 num_args, char *args[]) __attribute__((noreturn))`
- `extern void ExecOSD(int num_args, char *args[]) __attribute__((noreturn))`
- `extern void InitAlarm(void)`
- `extern void InitExecPS2(void)`
- `ExecPS2 and ExecOSD extern void InitOsd(void)`
- `c extern int PatchIsNeeded(void)`
- `extern void InitDebug(void)`
- `extern int DIntr(void)`
- `extern int EIntr(void)`
- `extern int EnableIntc(int intc)`
- `extern int DisableIntc(int intc)`
- `extern int EnableDmac(int dmac)`
- `extern int DisableDmac(int dmac)`
- `extern int iEnableIntc(int intc)`
- `extern int iDisableIntc(int intc)`
- `extern int iEnableDmac(int dmac)`
- `extern int iDisableDmac(int dmac)`
- `extern void SyncDCache(void *start, void *end)`
- `extern void iSyncDCache(void *start, void *end)`
- `extern void InvalidDCache(void *start, void *end)`
- `extern void iInvalidDCache(void *start, void *end)`
- `extern void ResetEE(u32 init_bitfield)`
- `extern void SetGsCrt(s16 interlace, s16 pal_ntsc, s16 field)`
- `extern void KExit(s32 exit_code) __attribute__((noreturn))`
- `extern void _LoadExecPS2(const char *filename, s32 num_args, char *args[]) __attribute__((noreturn))`
- `extern s32 _ExecPS2(void *entry, void *gp, int num_args, char *args[])`
- `extern void RFU009(u32 arg0, u32 arg1)`
- `extern s32 AddSbusIntcHandler(s32 cause, void (*handler)(int call))`
- `extern s32 RemoveSbusIntcHandler(s32 cause)`
- `extern s32 Interrupt2Iop(s32 cause)`
- `extern void SetVTLBRefillHandler(s32 handler_num, void *handler_func)`
- `extern void SetVCommonHandler(s32 handler_num, void *handler_func)`
- `extern void SetVInterruptHandler(s32 handler_num, void *handler_func)`
- `extern s32 AddIntcHandler(s32 cause, s32 (*handler_func)(s32 cause), s32 next)`
- `extern s32 AddIntcHandler2(s32 cause, s32 (*handler_func)(s32 cause, void *arg, void *addr), s32 next, void *arg)`
- `extern s32 RemoveIntcHandler(s32 cause, s32 handler_id)`
- `extern s32 AddDmacHandler(s32 channel, s32 (*handler)(s32 channel), s32 next)`
- `extern s32 AddDmacHandler2(s32 channel, s32 (*handler)(s32 channel, void *arg, void *addr), s32 next, void *arg)`
- `extern s32 RemoveDmacHandler(s32 channel, s32 handler_id)`
- `extern s32 _EnableIntc(s32 cause)`
- `extern s32 _DisableIntc(s32 cause)`
- `extern s32 _EnableDmac(s32 channel)`
- `extern s32 _DisableDmac(s32 channel)`
- `extern s32 SetAlarm(u16 time, void (*callback)(s32 alarm_id, u16 time, void *common), void *common)`
- `extern s32 _SetAlarm(u16 time, void (*callback)(s32 alarm_id, u16 time, void *common), void *common)`
- `extern s32 ReleaseAlarm(s32 alarm_id)`
- `extern s32 _ReleaseAlarm(s32 alarm_id)`
- `extern s32 _iEnableIntc(s32 cause)`
- `extern s32 _iDisableIntc(s32 cause)`
- `extern s32 _iEnableDmac(s32 channel)`
- `extern s32 _iDisableDmac(s32 channel)`
- `extern s32 iSetAlarm(u16 time, void (*callback)(s32 alarm_id, u16 time, void *common), void *common)`
- `extern s32 _iSetAlarm(u16 time, void (*callback)(s32 alarm_id, u16 time, void *common), void *common)`
- `extern s32 iReleaseAlarm(s32 alarm_id)`
- `extern s32 _iReleaseAlarm(s32 alarm_id)`
- `extern s32 CreateThread(ee_thread_t *thread)`
- `extern s32 DeleteThread(s32 thread_id)`
- `extern s32 StartThread(s32 thread_id, void *args)`
- `extern void ExitThread(void)`
- `extern void ExitDeleteThread(void)`
- `extern s32 TerminateThread(s32 thread_id)`
- `extern s32 iTerminateThread(s32 thread_id)`
- `extern void DisableDispatchThread(void)`
- `extern void EnableDispatchThread(void)`
- `not supported extern s32 ChangeThreadPriority(s32 thread_id, s32 priority)`
- `extern s32 iChangeThreadPriority(s32 thread_id, s32 priority)`
- … +102 (ver header)

Macros/constantes: `ALIGNED`, `CPU_DATA_CACHE`, `CPU_INSTRUCTION_CACHE`, `DI`, `DISABLE_EXTRA_TIMERS_FUNCTIONS`, `DISABLE_PATCHED_ALARMS`, `DISABLE_PATCHED_ExecPS2`, `DISABLE_PATCHED_Exit`, `DISABLE_PATCHED_FUNCTIONS`, `DISABLE_PATCHED_LoadExecPS2`, `DISABLE_PATCHED_THREADS`, `DISABLE_PATCHED_TLBFunctions`, `DISABLE_TimerAlarm`, `DISABLE_TimerSystemTime`, `EE_SYNC`, `EE_SYNCL`, `EE_SYNCP`, `EI`, `ExitHandler`, `INIT_DMAC`, `INIT_GIF`, `INIT_IPU`, `INIT_VIF0`, `INIT_VIF1`, `INIT_VU0`, `INIT_VU1`, `INVALIDATE_CACHE`, `INVALIDATE_DCACHE`, `INVALIDATE_ICACHE`, `IS_UNCACHED_SEG`, `MAX_ALARMS`, `MAX_HANDLERS`, `MAX_PRIORITY`, `MAX_SEMAPHORES`, `MAX_THREADS`, `NO_PATCHED_ExecOSD`, `POPDATA`, `PUSHDATA`, `SetModuleGP`, `SifGetReg`, `SifSetDChain`, `SifSetReg`, `SifStopDma`, `THS_DORMANT`, `THS_READY`, `THS_RUN`, `THS_SUSPEND`, `THS_WAIT`, `THS_WAITSUSPEND`, `TH_SELF`, `TSW_NONE`, `TSW_SEMA`, `TSW_SLEEP`, `UCAB_SEG`, `UNCACHED_SEG`, `WRITEBACK_DCACHE`, `__KERNEL_H__`, `iSifDmaStat`, `iSifSetDChain`, `iSifSetDma` …

### `ee/include/kernel_util.h`

Funções:

- `extern s32 WaitSemaEx(s32 semaid, int signal, u64 *timeout)`

Macros/constantes: `__KERNEL_UTIL_H__`

### `ee/include/libcdvd.h`

Funções:

- `param buf buffer to read to(in iop memory) * @param mode read mode * @return 1 if successful, 0 if error */ extern int sceCdReadIOPMem(u32 lbn, u32 sectors, void *buf, sceCdRMode *mode)`
- `extern int sceCdNCmdDiskReady(void)`
- `extern int sceCdSpinCtrlEE(u32 speed)`

Macros/constantes: `__LIBCDVD_H__`

### `ee/include/libgs.h`

Funções:

- `QWORD __attribute__((aligned(16)))`
- `GS_GIF_DMACHAIN_TAG __attribute__((aligned(16)))`
- `GS_DISPENV __attribute__((aligned(8)))`
- `GS_GIF_PACKET __attribute__((aligned(16)))`
- `extern int GsSetXYOffset1(u16 x, u16 y)`
- `extern int GsSetXYOffset2(u16 x, u16 y)`
- `extern int GsSetScissor1(u16 upper_x, u16 upper_y, u16 lower_x, u16 lower_y)`
- `extern int GsSetScissor2(u16 upper_x, u16 upper_y, u16 lower_x, u16 lower_y)`
- `extern int GsSetFrame1(u16 framebuffer_addr, u8 framebuffer_width, u8 psm, u32 draw_mask)`
- `extern int GsSetFrame2(u16 framebuffer_addr, u8 framebuffer_width, u8 psm, u32 draw_mask)`
- `extern int GsTextureFlush(void)`
- `extern int GsSetPixelTest1(u8 enable_alpha_test, u8 alpha_test_method, u8 alpha_reference, u8 alpha_fail_method, u8 enable_dest_alpha_test, u8 dest_alpha_test_mode, u8 enable_zbuff_test, u8 alpha_zbuff_method)`
- `extern int GsSetPixelTest2(u8 enable_alpha_test, u8 alpha_test_method, u8 alpha_reference, u8 alpha_fail_method, u8 enable_dest_alpha_test, u8 dest_alpha_test_mode, u8 enable_zbuff_test, u8 alpha_zbuff_method)`
- `extern int GsSelectTexure1(u16 tex_addr, u8 addr_width, u8 tex_pixmode, u16 tex_width, u16 tex_height, u16 clut_addr, u8 clut_pixmode, u8 clut_storagemode,u8 clut_offset)`
- `extern int GsSelectTexure2(u16 tex_addr, u8 addr_width, u8 tex_pixmode, u16 tex_width, u16 tex_height, u16 clut_addr, u8 clut_pixmode, u8 clut_storagemode,u8 clut_offset)`
- `extern void GsSetFogColor(u8 r, u8 g, u8 b)`
- `extern void GsEnableColorClamp(u16 enable)`
- `extern GsGParam_t * GsGetGParam(void)`
- `extern void GsResetGraph(short int mode, short int interlace, short int omode, short int ffmode)`
- `extern void GsResetPath(void)`
- `extern void GsSetCRTCSettings(u64 settings, u8 alpha_value)`
- `extern void GsSetDefaultDrawEnv(GS_DRAWENV *drawenv, u16 psm, u16 w, u16 h)`
- `extern void GsSetDefaultDrawEnvAddress(GS_DRAWENV *drawenv, u16 vram_addr)`
- `extern void GsSetDefaultDisplayEnv(GS_DISPENV *dispenv, u16 psm, u16 w, u16 h, u16 dx, u16 dy)`
- `extern void GsSetDefaultDisplayEnvAddress(GS_DISPENV *dispenv, u16 vram_addr)`
- `extern void GsSetDefaultZBufferEnv(GS_ZENV *zenv, u8 update_mask)`
- `extern void GsSetDefaultZBufferEnvAddress(GS_ZENV *zenv, u16 vram_addr, u8 psm)`
- `s data(Environments)*/ extern void GsPutDrawEnv1(GS_DRAWENV *drawenv)`
- `extern void GsPutDrawEnv2(GS_DRAWENV *drawenv)`
- `extern void GsPutDisplayEnv1(GS_DISPENV *dispenv)`
- `extern void GsPutDisplayEnv2(GS_DISPENV *dispenv)`
- `extern void GsPutZBufferEnv1(GS_ZENV *zenv)`
- `extern void GsPutZBufferEnv2(GS_ZENV *zenv)`
- `bg_color color(contex 1) */ extern void GsClearDrawEnv1(GS_DRAWENV *drawenv)`
- `bg_color color(contex 2) */ extern void GsClearDrawEnv2(GS_DRAWENV *drawenv)`
- `extern QWORD * GsGifPacketsAlloc(GS_PACKET_TABLE *table, u32 num_qwords)`
- `extern void GsGifPacketsClear(GS_PACKET_TABLE *table)`
- `extern int GsGifPacketsExecute(GS_PACKET_TABLE *table, u16 wait)`
- `extern int GsLoadImage(const void *source_addr, GS_IMAGE *dest)`
- `extern void GsOverridePrimAttributes(s8 override, s8 iip, s8 tme, s8 fge, s8 abe, s8 aa1, s8 fst, s8 ctxt, s8 fix)`
- `extern void GsEnableDithering(u8 enable, int mode)`
- `extern void GsEnableAlphaTransparency1(u16 enable,u16 method,u8 alpha_ref,u16 fail_method)`
- `extern void GsEnableAlphaTransparency2(u16 enable,u16 method,u8 alpha_ref,u16 fail_method)`
- `extern void GsEnableZbuffer1(u16 enable,u16 test_method)`
- `extern void GsEnableZbuffer2(u16 enable,u16 test_method)`
- `extern void GsEnableAlphaBlending1(u16 enable)`
- `extern void GsEnableAlphaBlending2(u16 enable)`
- `extern void GsDrawSync(int mode)`
- `extern void GsHSync(int mode)`
- `extern void GsVSync(int mode)`
- `extern int GsVramAllocFrameBuffer(s16 w, s16 h, s16 psm)`
- `extern int GsVramAllocTextureBuffer(s16 w, s16 h, s16 psm)`
- `extern void GsVramFreeAllTextureBuffer(void)`
- `extern void GsVramFreeAll(void)`
- `extern int GsDbGetDrawBuffer(void)`
- `extern int GsDbGetDisplayBuffer(void)`
- `extern void GsDbSwapBuffer(void)`

Macros/constantes: `CRTC_SETTINGS_BLENDRC1`, `CRTC_SETTINGS_BLENDVAL`, `CRTC_SETTINGS_DEFAULT1`, `CRTC_SETTINGS_DEFAULT2`, `CRTC_SETTINGS_EN1`, `CRTC_SETTINGS_EN2`, `CRTC_SETTINGS_ENBOTH`, `CRTC_SETTINGS_OUTRC1`, `CRTC_SETTINGS_OUTRC2`, `CRTC_SETTINGS_STYLEBG`, `CRTC_SETTINGS_STYLERC1`, `GS_CLUT_16`, `GS_CLUT_16S`, `GS_CLUT_32`, `GS_DISABLE`, `GS_ENABLE`, `GS_FFMD_FIELD`, `GS_FFMD_FRAME`, `GS_GET_CSR_current_field`, `GS_GET_CSR_fifo_status`, `GS_GET_CSR_finish_evnt`, `GS_GET_CSR_gs_id`, `GS_GET_CSR_gs_rev_number`, `GS_GET_CSR_hsync_intrupt`, `GS_GET_CSR_signal_evnt`, `GS_GET_CSR_vsync_intrupt`, `GS_GET_CSR_write_nfield`, `GS_GET_CSR_write_terminate`, `GS_INIT_DRAW_RESET`, `GS_INIT_RESET`, `GS_INTERLACED`, `GS_NONINTERLACED`, `GS_PACKET_DATA_QWORD_MAX`, `GS_PIXMODE_16`, `GS_PIXMODE_16S`, `GS_PIXMODE_24`, `GS_PIXMODE_32`, `GS_SET_BGCOLOR`, `GS_SET_BUSDIR`, `GS_SET_CSR`, `GS_SET_CSR_finish_evnt`, `GS_SET_CSR_flush`, `GS_SET_CSR_hsync_intrupt`, `GS_SET_CSR_reset`, `GS_SET_CSR_signal_evnt`, `GS_SET_CSR_vsync_intrupt`, `GS_SET_CSR_write_terminate`, `GS_SET_DISPFB1`, `GS_SET_DISPFB2`, `GS_SET_DISPLAY1`, `GS_SET_DISPLAY2`, `GS_SET_EXTBUF`, `GS_SET_EXTDATA`, `GS_SET_EXTWRITE`, `GS_SET_IMR`, `GS_SET_PMODE`, `GS_SET_SIGLBLID`, `GS_SET_SMODE1`, `GS_SET_SMODE2`, `GS_TEX_16` …

### `ee/include/libhdd.h`

Funções:

- `extern int hddCheckFormatted()`
- `extern int hddFormat()`
- `extern int hddGetFilesystemList(t_hddFilesystem hddFs[], int maxEntries)`
- `extern void hddGetInfo(t_hddInfo *info)`
- `extern int hddMakeFilesystem(int fsSizeMB, char *name, int type)`
- `extern int hddRemoveFilesystem(t_hddFilesystem *fs)`
- `extern int hddExpandFilesystem(t_hddFilesystem *fs, int extraMB)`

Macros/constantes: `ATTR_MAIN_PARTITION`, `ATTR_SUB_PARTITION`, `FS_COMMON_PREFIX`, `FS_GROUP_APPLICATION`, `FS_GROUP_COMMON`, `FS_GROUP_SYSTEM`, `FS_TYPE_CFS`, `FS_TYPE_EMPTY`, `FS_TYPE_EXT2`, `FS_TYPE_EXT2_SWAP`, `FS_TYPE_PFS`, `FS_TYPE_REISER`, `PFS_MT_ROBUST`, `__LIBHDD_H__`, `hddPowerOff`, `hddPreparePoweroff`, `hddSetUserPoweroffCallback`

### `ee/include/libkbd.h`

Funções:

- `extern int PS2KbdInit(void)`
- `extern int PS2KbdRead(char *key)`
- `extern int PS2KbdReadRaw(PS2KbdRawKey *key)`
- `extern int PS2KbdSetReadmode(u32 readMode)`
- `extern int PS2KbdSetBlockingMode(u32 readMode)`
- `extern int PS2KbdSetRepeatRate(u32 repeat)`
- `extern int PS2KbdSetLeds(u8 leds)`
- `extern int PS2KbdSetKeymap(PS2KbdKeyMap *keymaps)`
- `extern int PS2KbdSetCtrlmap(u8 *ctrlmap)`
- `extern int PS2KbdSetAltmap(u8 *altmap)`
- `extern int PS2KbdSetSpecialmap(u8 *special)`
- `extern int PS2KbdFlushBuffer(void)`
- `extern int PS2KbdResetKeymap(void)`
- `extern int PS2KbdClose(void)`

Macros/constantes: `__LIBKBD_H__`

### `ee/include/libmc.h`

Funções:

- `sceMcTblGetDir __attribute__((aligned(64)))`
- `mcTable __attribute__((deprecated, aligned (64)))`
- `extern int mcInit(int type)`
- `extern int mcGetInfo(int port, int slot, int* type, int* free, int* format)`
- `extern int mcOpen(int port, int slot, const char *name, int mode)`
- `extern int mcClose(int fd)`
- `extern int mcSeek(int fd, int offset, int origin)`
- `extern int mcRead(int fd, void *buffer, int size)`
- `extern int mcWrite(int fd, const void *buffer, int size)`
- `extern int mcFlush(int fd)`
- `extern int mcMkDir(int port, int slot, const char* name)`
- `extern int mcChdir(int port, int slot, const char* newDir, char* currentDir)`
- `extern int mcGetDir(int port, int slot, const char *name, unsigned mode, int maxent, sceMcTblGetDir* table)`
- `extern int mcSetFileInfo(int port, int slot, const char* name, const sceMcTblGetDir* info, unsigned flags)`
- `extern int mcDelete(int port, int slot, const char *name)`
- `extern int mcFormat(int port, int slot)`
- `extern int mcUnformat(int port, int slot)`
- `extern int mcGetEntSpace(int port, int slot, const char* path)`
- `extern int mcRename(int port, int slot, const char* oldName, const char* newName)`
- `extern int mcEraseBlock(int port, int slot, int block, int mode)`
- `extern int mcReadPage(int port, int slot, unsigned int page, void *buffer)`
- `extern int mcWritePage(int port, int slot, int page, const void *buffer)`
- `extern int mcChangeThreadPriority(int level)`
- `function is still executing(mode=1)`
- `extern int mcSync(int mode, int *cmd, int *result)`
- `extern int mcReset(void)`
- `extern int xfromInit(int type)`
- `extern int xfromGetInfo(int port, int slot, int* type, int* free, int* format)`
- `extern int xfromOpen(int port, int slot, const char *name, int mode)`
- `extern int xfromClose(int fd)`
- `extern int xfromSeek(int fd, int offset, int origin)`
- `extern int xfromRead(int fd, void *buffer, int size)`
- `extern int xfromWrite(int fd, const void *buffer, int size)`
- `extern int xfromFlush(int fd)`
- `extern int xfromMkDir(int port, int slot, const char* name)`
- `extern int xfromChdir(int port, int slot, const char* newDir, char* currentDir)`
- `extern int xfromGetDir(int port, int slot, const char *name, unsigned mode, int maxent, sceMcTblGetDir* table)`
- `extern int xfromSetFileInfo(int port, int slot, const char* name, const sceMcTblGetDir* info, unsigned flags)`
- `extern int xfromDelete(int port, int slot, const char *name)`
- `extern int xfromFormat(int port, int slot)`
- `extern int xfromUnformat(int port, int slot)`
- `extern int xfromGetEntSpace(int port, int slot, const char* path)`
- `extern int xfromRename(int port, int slot, const char* oldName, const char* newName)`
- `extern int xfromEraseBlock(int port, int slot, int block, int mode)`
- `extern int xfromReadPage(int port, int slot, unsigned int page, void *buffer)`
- `extern int xfromWritePage(int port, int slot, int page, const void *buffer)`
- `extern int xfromChangeThreadPriority(int level)`
- `function is still executing(mode=1)`
- `extern int xfromSync(int mode, int *cmd, int *result)`
- `extern int xfromReset(void)`

Macros/constantes: `MC_ATTR_CLOSED`, `MC_ATTR_EXECUTABLE`, `MC_ATTR_FILE`, `MC_ATTR_HIDDEN`, `MC_ATTR_OBJECT`, `MC_ATTR_PDAEXEC`, `MC_ATTR_PROTECTED`, `MC_ATTR_PSX`, `MC_ATTR_READABLE`, `MC_ATTR_SUBDIR`, `MC_ATTR_WRITEABLE`, `MC_FORMATTED`, `MC_NOWAIT`, `MC_TYPE_MC`, `MC_TYPE_NONE`, `MC_TYPE_POCKET`, `MC_TYPE_PS2`, `MC_TYPE_PSX`, `MC_TYPE_XMC`, `MC_UNFORMATTED`, `MC_WAIT`, `__LIBMC_H__`

### `ee/include/libmouse.h`

Funções:

- `extern int PS2MouseInit(void)`
- `Reads the current mouse data(depending on read mode) */ extern int PS2MouseRead(PS2MouseData *)`
- `extern int PS2MouseSetReadMode(u32 readMode)`
- `extern u32 PS2MouseGetReadMode()`
- `extern int PS2MouseSetThres(u32 thres)`
- `extern u32 PS2MouseGetThres()`
- `extern int PS2MouseSetAccel(float accel)`
- `extern float PS2MouseGetAccel()`
- `extern int PS2MouseSetBoundary(int minx, int maxx, int miny, int maxy)`
- `extern int PS2MouseGetBoundary(int *minx, int *maxx, int *miny, int *maxy)`
- `extern int PS2MouseSetPosition(int x, int y)`
- `extern int PS2MouseReset()`
- `extern u32 PS2MouseEnum()`
- `extern int PS2MouseSetDblClickTime(u32 msec)`
- `extern u32 PS2MouseGetDblClickTIme()`
- `extern u32 PS2MouseGetVersion()`

Macros/constantes: `__LIBMOUSE_H__`

### `ee/include/libmpeg.h`

Funções:

- `extern void MPEG_Destroy(void)`
- `extern int(*MPEG_Picture ) ( void *apData, s64 *apPTS)`

Macros/constantes: `MPEG_CHROMA_FORMAT_420`, `MPEG_CHROMA_FORMAT_422`, `MPEG_CHROMA_FORMAT_444`, `MPEG_LEVEL_HIGH`, `MPEG_LEVEL_HIGH1440`, `MPEG_LEVEL_LOW`, `MPEG_LEVEL_MAIN`, `MPEG_PROFILE_422`, `MPEG_PROFILE_HIGH`, `MPEG_PROFILE_MAIN_`, `MPEG_PROFILE_SIMPLE`, `MPEG_PROFILE_SNR_SCALABLE`, `MPEG_PROFILE_SPT_SCALABLE`, `MPEG_VIDEO_FORMAT_COMPONENT`, `MPEG_VIDEO_FORMAT_MAC`, `MPEG_VIDEO_FORMAT_NTSC`, `MPEG_VIDEO_FORMAT_PAL`, `MPEG_VIDEO_FORMAT_SECAM`, `MPEG_VIDEO_FORMAT_UNSPEC`, `__LIBMPEG_H__`

### `ee/include/libmtap.h`

Funções:

- `extern int mtapInit(void)`
- `extern int mtapPortOpen(int port)`
- `extern int mtapPortClose(int port)`
- `extern int mtapGetConnection(int port)`

Macros/constantes: `__LIBMTAP_H__`

### `ee/include/libpad.h`

Funções:

- `extern int padInit(int mode)`
- `Automatically called by padInit(), there is no need to call this function directly. * @param mode Must be set to 0. * @return == 1 => OK * * Note: PADMAN from release 1.3.4 does not have this function implemented. * As a result, it is impossible to reinitialize libpad after calling padEnd(). * * @return == 1 => OK */ extern int padPortInit(int mode)`
- `it is impossible to reinitialize libpad after calling padEnd(). * This was known as padClose in the really early official SDK releases. * * @return == 1 => OK */ extern int padEnd(void)`
- `byte region(2xsizeof(struct pad_data). * Must be a 64-byte aligned address. For the old libpad, at least 16-bytes alignment. * @return != 0 => OK */ extern int padPortOpen(int port, int slot, void *padArea)`
- `extern int padPortClose(int port, int slot)`
- `extern unsigned char padRead(int port, int slot, struct padButtonStatus *data)`
- `extern int padGetState(int port, int slot)`
- `extern unsigned char padGetReqState(int port, int slot)`
- `Set pad request state(after a param setting) * No need to export this one perhaps.. */ extern int padSetReqState(int port, int slot, int state)`
- `extern void padStateInt2String(int state, char buf[16])`
- `extern void padReqStateInt2String(int state, char buf[16])`
- `slots on the PS2(usally 2) */ extern int padGetPortMax(void)`
- `slots the port has(usually 1) * probably 4 if using a multi tap (not tested) */ extern int padGetSlotMax(int port)`
- `extern int padGetModVersion()`
- `extern int padInfoMode(int port, int slot, int infoMode, int index)`
- `extern int padSetMainMode(int port, int slot, int mode, int lock)`
- `extern int padInfoPressMode(int port, int slot)`
- `extern int padEnterPressMode(int port, int slot)`
- `extern int padExitPressMode(int port, int slot)`
- `extern int padGetButtonMask(int port, int slot)`
- `extern int padSetButtonInfo(int port, int slot, int buttonInfo)`
- `extern unsigned char padInfoAct(int port, int slot, int actuator, int cmd)`
- `extern int padSetActAlign(int port, int slot, const char actAlign[6])`
- `extern int padSetActDirect(int port, int slot, const char actAlign[6])`
- `slot is connected(1 = connected) * Appears to have been removed very early during the PS2's lifetime. * If possible, use the documented padGetState instead. * * NOT SUPPORTED with module rom0:padman */ extern int padGetConnection(int port, int slot)`

Macros/constantes: `PAD_ACTCURR`, `PAD_ACTFUNC`, `PAD_ACTSIZE`, `PAD_ACTSUB`, `PAD_CIRCLE`, `PAD_CROSS`, `PAD_DOWN`, `PAD_L1`, `PAD_L2`, `PAD_L3`, `PAD_LEFT`, `PAD_MMODE_DIGITAL`, `PAD_MMODE_DUALSHOCK`, `PAD_MMODE_LOCK`, `PAD_MMODE_UNLOCK`, `PAD_MODECUREXID`, `PAD_MODECURID`, `PAD_MODECUROFFS`, `PAD_MODETABLE`, `PAD_R1`, `PAD_R2`, `PAD_R3`, `PAD_RIGHT`, `PAD_RSTAT_BUSY`, `PAD_RSTAT_COMPLETE`, `PAD_RSTAT_FAILED`, `PAD_SELECT`, `PAD_SQUARE`, `PAD_START`, `PAD_STATE_DISCONN`, `PAD_STATE_ERROR`, `PAD_STATE_EXECCMD`, `PAD_STATE_FINDCTP1`, `PAD_STATE_FINDPAD`, `PAD_STATE_STABLE`, `PAD_TRIANGLE`, `PAD_TYPE_ANALOG`, `PAD_TYPE_DIGITAL`, `PAD_TYPE_DUALSHOCK`, `PAD_TYPE_EX_JOGCON`, `PAD_TYPE_EX_TSURICON`, `PAD_TYPE_JOGCON`, `PAD_TYPE_KONAMIGUN`, `PAD_TYPE_NAMCOGUN`, `PAD_TYPE_NEJICON`, `PAD_UP`, `__LIBPAD_H__`

### `ee/include/libpwroff.h`

Funções:

- `Last function inside callback should be * poweroffShutdown(). */ typedef void (*poweroff_callback)(void *arg)`
- `extern int poweroffInit(void)`
- `arg Arguments that are sent to cb function(can be <b>NULL</b>) * * @details Callback function should be user-defined elsewhere. There are some * standart specifications. * */ extern void poweroffSetCallback(poweroff_callback cb, void *arg)`
- `extern void poweroffShutdown(void)`
- `u can change the priority * by calling this function after poweroffSetCallback(). * You can call this function before poweroffSetCallback() as well (but after * poweroffInit() ). In that case poweroffSetCallback() will be registered with * the provided priority. */ extern void poweroffChangeThreadPriority(int priority)`

Macros/constantes: `POWEROFF_THREAD_PRIORITY`, `__LIBPWROFF_H__`

### `ee/include/librm.h`

Funções:

- `extern int RMMan_Init(void)`
- `extern int RMMan_End(void)`
- `port Port number to open(0 or 1) * @param [in] slot Slot to open (0 or 1: multitap not supported) * @param [out] pData The address of the buffer for storing the pad status. * Must be a 256-byte region (2xsizeof(struct pad_data). * Must be a 64-byte aligned address. * @return != 0 => OK */ extern int RMMan_Open(int port, int slot, void *pData)`
- `extern int RMMan_Close(int port, int slot)`
- `extern void RMMan_Read(int port, int slot, struct remote_data *data)`
- `extern u32 RMMan_GetModuleVersion(void)`

Macros/constantes: `RM_BD_0`, `RM_BD_1`, `RM_BD_2`, `RM_BD_3`, `RM_BD_4`, `RM_BD_5`, `RM_BD_6`, `RM_BD_7`, `RM_BD_8`, `RM_BD_9`, `RM_BD_AUDIO`, `RM_BD_BLUE`, `RM_BD_DISPLAY`, `RM_BD_DOWN`, `RM_BD_EJECT`, `RM_BD_ENTER`, `RM_BD_FF`, `RM_BD_GREEN`, `RM_BD_HOME`, `RM_BD_LEFT`, `RM_BD_NETFLIX`, `RM_BD_NEXT`, `RM_BD_OPTIONS`, `RM_BD_PAUSE`, `RM_BD_PLAY`, `RM_BD_POP_UP_MENU`, `RM_BD_POWER`, `RM_BD_PREV`, `RM_BD_RED`, `RM_BD_RETURN`, `RM_BD_RIGHT`, `RM_BD_RWD`, `RM_BD_SEN`, `RM_BD_STOP`, `RM_BD_SUBTITLE`, `RM_BD_TOP_MENU`, `RM_BD_UP`, `RM_BD_YELLOW`, `RM_DESR_1`, `RM_DESR_10`, `RM_DESR_11`, `RM_DESR_12`, `RM_DESR_2`, `RM_DESR_3`, `RM_DESR_4`, `RM_DESR_5`, `RM_DESR_6`, `RM_DESR_7`, `RM_DESR_8`, `RM_DESR_9`, `RM_DESR_BS_11`, `RM_DESR_BS_7`, `RM_DESR_CIRCLE`, `RM_DESR_CLEAR`, `RM_DESR_CROSS_BACK`, `RM_DESR_DELETE`, `RM_DESR_DISPLAY`, `RM_DESR_DOWN`, `RM_DESR_DVRP_UNK`, `RM_DESR_EJECT` …

### `ee/include/libsdr.h`

Funções:

- `extern int sceSdRemoteInit(void)`
- `extern int sceSdTransToIOP(void *eeaddr, void *iopaddr, u32 size, u32 blocking)`
- `extern int sceSdRemote(int arg0, ...)`
- `extern int sceSdRemoteCallbackInit(int pri)`
- `extern int sceSdRemoteCallbackQuit(void)`

Macros/constantes: `__LIBSDR_H__`

### `ee/include/libsecr.h`

Funções:

- `extern int SecrInit(void)`
- `extern void SecrDeinit(void)`

### `ee/include/libvux.h`

Funções:

- `VU_MATRIX __attribute__((aligned(16)))`
- `VU_VECTOR __attribute__((aligned(16)))`
- `extern void VuInit(void)`
- `extern void VuSetGeometryXYOffset(unsigned short x, unsigned short y)`
- `extern void VuSetProjection(float z)`
- `extern void VuSetProjectionMatrix(const VU_MATRIX *projection)`
- `extern void VuSetProjectionType(unsigned int type)`
- `extern void VuSetWorldMatrix(const VU_MATRIX *world)`
- `extern void VuSetViewMatrix(const VU_MATRIX *view)`
- `extern void VuSetLocalScreenMatrix(const VU_MATRIX *m)`
- `extern void VuSetProjectionNearPlaneWH(unsigned int w, unsigned int h)`
- `extern void VuSetAmbientLight(float r, float g, float b)`
- `extern void Vu0IdMatrix(VU_MATRIX *m)`
- `extern void Vu0ResetMatrix(VU_MATRIX *m)`
- `extern void VuxRotMatrixX(VU_MATRIX *m, float x)`
- `extern void VuxRotMatrixY(VU_MATRIX *m, float y)`
- `extern void VuxRotMatrixZ(VU_MATRIX *m, float z)`
- `extern void VuxRotMatrixXYZ(VU_MATRIX *m, float x,float y, float z)`
- `extern void Vu0TransMatrix(VU_MATRIX *m, VU_VECTOR *t)`
- `extern void Vu0TransMatrixXYZ(VU_MATRIX *m,float x, float y, float z)`
- `extern void Vu0ScaleMatrix(VU_MATRIX *m, VU_VECTOR *s)`
- `extern void Vu0ScaleMatrixXYZ(VU_MATRIX *m, float x, float y, float z)`
- `extern void Vu0MulMatrix(VU_MATRIX *m0, VU_MATRIX *m1, VU_MATRIX *out)`
- `extern void Vu0InverseMatrix(VU_MATRIX *in, VU_MATRIX *out)`
- `extern void Vu0ApplyMatrix(VU_MATRIX *m, VU_VECTOR *v0, VU_VECTOR *out)`
- `extern void Vu0ApplyRotMatrix(VU_MATRIX *m, VU_VECTOR *v0, VU_VECTOR *out)`
- `extern void Vu0CopyMatrix(VU_MATRIX *dest, VU_MATRIX *src)`
- `extern float Vu0DotProduct(VU_VECTOR *v0, VU_VECTOR *v1)`
- `extern void VuxIdMatrix(VU_MATRIX *m)`
- `extern void VuxResetMatrix(VU_MATRIX *m)`
- `extern void VuxRotMatrix(VU_MATRIX *m, VU_VECTOR *v)`
- `extern void VuxRotMatrixX(VU_MATRIX *m, float x)`
- `extern void VuxRotMatrixY(VU_MATRIX *m, float y)`
- `extern void VuxRotMatrixZ(VU_MATRIX *m, float z)`
- `extern void VuxRotMatrixXYZ(VU_MATRIX *m, float x,float y, float z)`
- `extern void VuxTransMatrix(VU_MATRIX *m, VU_VECTOR *v0)`
- `extern void VuxTransMatrixXYZ(VU_MATRIX *m,float x, float y, float z)`
- `extern void VuxScaleMatrix(VU_MATRIX *m, VU_VECTOR *s)`
- `extern void VuxScaleMatrixXYZ(VU_MATRIX *m, float x, float y, float z)`
- `extern void VuxMulMatrix(VU_MATRIX *m0, VU_MATRIX *m1, VU_MATRIX *out)`
- `extern void VuxInverseMatrix(VU_MATRIX *in, VU_MATRIX *mat)`
- `extern void VuxCopyMatrix(VU_MATRIX *dest, VU_MATRIX *src)`
- `extern void VuxApplyMatrix(VU_MATRIX *m, VU_VECTOR *v0, VU_VECTOR *out)`
- `extern void VuxApplyRotMatrix(VU_MATRIX *m, VU_VECTOR *v0, VU_VECTOR *out)`
- `extern float VuxDotProduct(VU_VECTOR *v0, VU_VECTOR *v1)`
- `extern VU_VECTOR VuxCrossProduct(VU_VECTOR *v0, VU_VECTOR *v1)`
- `extern void VuxCrossProduct0(VU_VECTOR *v0, VU_VECTOR *v1, VU_VECTOR *out)`
- `extern void VuxVectorNormal(VU_VECTOR *v)`
- `extern void VuxVectorNormal0(VU_VECTOR *in, VU_VECTOR *out)`
- `extern void VuxApplyMatrixLS(VU_VECTOR *v0, VU_VECTOR *out)`
- `extern void VuxApplyRotMatrixLS(VU_VECTOR *v0, VU_VECTOR *out)`
- `extern void VuxMakeLocalScreenMatrix(VU_MATRIX *out, VU_MATRIX *world, VU_MATRIX *view)`
- `extern void VuxMakeLocalScreenMatrix2(VU_MATRIX *out, VU_MATRIX *world, VU_MATRIX *view, VU_MATRIX *projection)`
- `extern void VuxMakeViewMatrix(VU_MATRIX *out, VU_VECTOR *rot, VU_VECTOR *pos, VU_VECTOR *scale)`
- `extern void VuxMakeLookAtViewMatrix(VU_MATRIX *out, VU_VECTOR *eye, VU_VECTOR *target, VU_VECTOR *up)`
- `extern void VuxMakeProjectionMatrix(VU_MATRIX *proj, float near_plane_w, float near_plane_h, float near_plane_z, float far_plane_z)`
- `extern void VuxUpdateLocalScreenMatrix(void)`
- `extern void VuxRotTrans(VU_VECTOR *v0, VU_VECTOR *out)`
- `extern void VuxRotTrans3(VU_VECTOR *v0, VU_VECTOR *v1, VU_VECTOR *v2, VU_VECTOR *tv0, VU_VECTOR *tv1, VU_VECTOR *tv2)`
- `extern void VuxRotTransN(VU_VECTOR *verts, VU_VECTOR *tverts, unsigned int num_verts)`
- `extern void VuxPers(VU_VECTOR *v0, VU_SXYZ *sxyz0)`
- `extern void VuxPers3(VU_VECTOR *v0, VU_VECTOR *v1, VU_VECTOR *v2, VU_SXYZ *sxyz0, VU_SXYZ *sxyz1, VU_SXYZ *sxyz2)`
- `extern void VuxPersN(VU_VECTOR *verts, VU_SXYZ *sxyz, unsigned int num_verts)`
- `extern int VuxPersClip3(VU_VECTOR *v0, VU_VECTOR *v1, VU_VECTOR *v2, VU_SXYZ *sxyz0, VU_SXYZ *sxyz1, VU_SXYZ *sxyz2)`
- `extern float VuxRotTransPers(VU_VECTOR *v0, VU_SXYZ *sxyz0)`
- `extern float VuxRotTransPers3(VU_VECTOR *v0, VU_VECTOR *v1, VU_VECTOR *v2, VU_SXYZ *sxyz0, VU_SXYZ *sxyz1, VU_SXYZ *sxyz2)`
- `extern void VuxRotTransPersN(VU_VECTOR *verts, VU_SXYZ *sxyz, unsigned int num_verts)`
- `extern int VuxRotTransPersClip3(VU_VECTOR *v0, VU_VECTOR *v1, VU_VECTOR *v2, VU_SXYZ *sxyz0, VU_SXYZ *sxyz1, VU_SXYZ *sxyz2)`
- `extern int VuxClipSxyz(VU_SXYZ *sxyz0, VU_SXYZ *sxyz1, VU_SXYZ *sxyz2)`
- `extern int VuxLightNormal(VU_VECTOR *normal, VU_CVECTOR *col0, void *light, unsigned int light_type, VU_CVECTOR *out0)`

Macros/constantes: `VU_LIGHT_TYPE_FLAT`, `__LIBVUX_H__`, `deg2radian`, `ftoi4`, `radian2deg`

### `ee/include/loadfile.h`

Funções:

- `extern int SifLoadFileInit(void)`
- `extern void SifLoadFileExit(void)`
- `Each argument string in args must be seperated by NUL('\\0'). * Pass the length of the entire args list, including the NUL seperators in the args_len * parameter. * * @note By default, modules cannot be loaded directly from the memory card. If you need to * load modules from the memory card, you should use ::SifExecModuleFile or link with the * sbv-lite library (http://www.0xd6.org). * * @see SifExecModuleFile */ extern int SifLoadModule(const char *path, int arg_len, const char *args)`
- `s _start() function * * @returns The ID of the loaded module on success, or an error if the module * couldn't be loaded. * * Loads an IRX module from the specified path, and executes it. See ::SifLoadModule for * details on the arg_len and args parameters. If mod_res is non-NULL, the result code from * the module's _start() function is stored here. * * @see SifLoadModule */ extern int SifLoadStartModule(const char *path, int arg_len, const char *args, int *mod_res)`
- `extern int SifLoadModuleEncrypted(const char *path, int arg_len, const char *args)`
- `extern int SifLoadModuleBuffer(void *ptr, int arg_len, const char *args)`
- `s _start() function * * @returns The ID of the loaded module on success, or an error if the module * couldn't be loaded. * * Loads an IRX module from a buffer located in IOP RAM. See ::SifLoadModule for details on * the arg_len and args parameters. If mod_res is non-NULL, the result code from the module's * _start() function is stored here. * * @note If you need to load an IRX module onto the IOP directly from a buffer in EE RAM, then * use ::SifExecModuleBuffer. * * @see SifLoadModule, SifExecModuleBuffer */ extern int SifLoadStartModuleBuffer(void *ptr, int arg_len, const char *args, int *mod_res)`
- `extern int SifStopModule(int id, int arg_len, const char *args, int *mod_res)`
- `extern int SifUnloadModule(int id)`
- `extern int SifSearchModuleByName(const char *name)`
- `extern int SifSearchModuleByAddress(const void *ptr)`
- `extern int SifLoadElfPart(const char *path, const char *secname, t_ExecData *data)`
- `int SifLoadElf(const char *path, t_ExecData *data)`
- `extern int SifLoadElfEncrypted(const char *path, t_ExecData *data)`
- `extern int SifIopGetVal(u32 iop_addr, void *val, int type)`
- `s _start() function * * @returns The ID of the loaded module on success, or an error if the module couldn't * be loaded. * * Transfers an IRX module stored in a buffer in EE RAM to the IOP, and calls ::SifLoadModuleBuffer to * load and execute the module. See ::SifLoadModule for details on the arg_len and args parameters. * If mod_res is non-NULL, the result code from the module's _start() function is stored here. * * @see SifLoadModule, SifLoadModuleBuffer */ extern int SifExecModuleBuffer(void *ptr, u32 size, u32 arg_len, const char *args, int *mod_res)`
- `s _start() function * * @returns The ID of the loaded module on success, or an error if the module * couldn't be loaded. * * Loads an IRX module from the specified path into a buffer on the IOP, then calls ::SifLoadModuleBuffer * to load and execute the module. See ::SifLoadModule for details on the arg_len and args parameters. * This function is useful if you need to load an IRX module from a prohibited device, such as the memory card. * * @see SifLoadModule, SifLoadModuleBuffer */ extern int SifExecModuleFile(const char *path, u32 arg_len, const char *args, int *mod_res)`
- `s _start() function * @param fno Function number of function to execute. * @param dontwait Whether to wait for the module's _start function to finish executing. * * @returns The ID of the loaded module on success, or an error if the module * couldn't be loaded. * * Unless required, use the higher-level functions instead. * * @see SifLoadModule, SifLoadModuleBuffer, SifLoadModuleEncrypted */ extern int _SifLoadModule(const char *path, int arg_len, const char *args, int *modres, int fno, int dontwait)`

Macros/constantes: `SCE_EBINDMISS`, `SCE_ECALLMISS`, `SCE_ELOADMISS`, `__LOADFILE_H__`

### `ee/include/lookupa.h`

Funções:

- `extern void checksum(const char *k, size_t length, uint32_t *state)`

Macros/constantes: `CHECKSTATE`, `hashmask`, `hashsize`

### `ee/include/math3d.h`

Funções:

- `extern void vector_apply(VECTOR output, VECTOR input0, MATRIX input1)`
- `extern void vector_clamp(VECTOR output, VECTOR input0, float min, float max)`
- `extern void vector_copy(VECTOR output, VECTOR input0)`
- `extern float vector_innerproduct(VECTOR input0, VECTOR input1)`
- `extern void vector_multiply(VECTOR output, VECTOR input0, VECTOR input1)`
- `extern void vector_normalize(VECTOR output, VECTOR input0)`
- `extern void vector_outerproduct(VECTOR output, VECTOR input0, VECTOR input1)`
- `extern void vector_add(VECTOR sum, VECTOR addend, VECTOR summand)`
- `extern void vector_cross_product(VECTOR product, VECTOR multiplicand, VECTOR multiplier)`
- `extern void vector_triangle_normal(VECTOR output, VECTOR a, VECTOR b, VECTOR c)`
- `extern void matrix_copy(MATRIX output, MATRIX input0)`
- `extern void matrix_inverse(MATRIX output, MATRIX input0)`
- `extern void matrix_multiply(MATRIX output, MATRIX input0, MATRIX input1)`
- `extern void matrix_rotate(MATRIX output, MATRIX input0, VECTOR input1)`
- `extern void matrix_scale(MATRIX output, MATRIX input0, VECTOR input1)`
- `extern void matrix_translate(MATRIX output, MATRIX input0, VECTOR input1)`
- `extern void matrix_transpose(MATRIX output, MATRIX input0)`
- `extern void matrix_unit(MATRIX output)`
- `extern void create_local_world(MATRIX local_world, VECTOR translation, VECTOR rotation)`
- `extern void create_local_light(MATRIX local_light, VECTOR rotation)`
- `extern void create_world_view(MATRIX world_view, VECTOR translation, VECTOR rotation)`
- `extern void create_view_screen(MATRIX view_screen, float aspect, float left, float right, float bottom, float top, float near, float far)`
- `Commonly used with vector_apply() to transform vertices for rendering. */ extern void create_local_screen(MATRIX local_screen, MATRIX local_world, MATRIX world_view, MATRIX view_screen)`
- `extern void calculate_normals(VECTOR *output, int count, VECTOR *normals, MATRIX local_light)`
- `extern void calculate_lights(VECTOR *output, int count, VECTOR *normals, VECTOR *light_directions, VECTOR *light_colours, const int *light_types, int light_count)`
- `extern void calculate_colours(VECTOR *output, int count, VECTOR *colours, VECTOR *lights)`
- `extern void calculate_vertices(VECTOR *output, int count, VECTOR *vertices, MATRIX local_screen)`

Macros/constantes: `LIGHT_AMBIENT`, `LIGHT_DIRECTIONAL`, `__MATH3D_H__`

### `ee/include/netdb.h`

Funções:

- `__BEGIN_DECLS extern struct hostent * gethostbyaddr(const void *addr, int len, int type)`
- `extern struct hostent * gethostbyname(const char *name)`
- `extern int gethostbyname_r(const char *name, struct hostent *ret, char *buf, size_t buflen, struct hostent **result, int *h_errnop)`
- `extern void freeaddrinfo(struct addrinfo *ai)`
- `extern int getaddrinfo(const char *nodename, const char *servname, const struct addrinfo *hints, struct addrinfo **res)`

Macros/constantes: `__PS2SDK_NETDB_H__`

### `ee/include/osd_config.h`

Funções:

- `param language Language value(See OSD_LANGUAGES above) */ extern void configSetLanguage(int language)`
- `extern int configGetTvScreenType(void)`
- `extern void configSetTvScreenType(int screenType)`
- `extern int configGetDateFormat(void)`
- `extern void configSetDateFormat(int dateFormat)`
- `extern int configGetTimeFormat(void)`
- `extern void configSetTimeFormat(int timeFormat)`
- `extern int configGetTimezone(void)`
- `extern void configSetTimezone(int offset)`
- `extern int configIsSpdifEnabled(void)`
- `extern void configSetSpdifEnabled(int enabled)`
- `extern int configIsDaylightSavingEnabled(void)`
- `extern void configSetDaylightSavingEnabled(int enabled)`
- `extern void configConvertToGmtTime(sceCdCLOCK *time)`
- `extern void configConvertToLocalTime(sceCdCLOCK *time)`
- `extern int IsEarlyJap(ConfigParam config)`

Macros/constantes: `__OSD_CONFIG_H__`

### `ee/include/packet.h`

Funções:

- `qword_t *data __attribute__((aligned(64)))`
- `extern packet_t * packet_init(int qwords, int type)`
- `extern void packet_reset(packet_t *packet)`
- `extern void packet_free(packet_t *packet)`
- `extern qword_t * packet_increment_qwc(packet_t *packet, int num)`

Macros/constantes: `PACKET_NORMAL`, `PACKET_SPR`, `PACKET_UCAB`, `QWC_MAX`, `__PACKET_H__`

### `ee/include/packet2.h`

Funções:

- `param qwords Maximum data size in qwords(128bit). * @param type Memory mapping type. * @param mode Packet mode. Normal or chain. * @param tte Tag transfer enable. * Used only for CHAIN mode! * If >0 transfer tag is set during packet sending and * add_dma_tag() (so also every open_tag()) will move buffer by DWORD, * so remember to align memory! * @returns Pointer to packet2 on success or NULL if memory allocation fail. */ extern packet2_t *packet2_create(u16 qwords, enum Packet2Type type, enum Packet2Mode mode, u8 tte)`
- `param base Pointer to base(start of buffer). * @param next Pointer to next (current position of buffer). * @param qwords Maximum data size in qwords (128bit). * @param type Memory mapping type. * @param mode Packet mode. Normal or chain (so also vif/vu). * @param tte Tag transfer enable. * Used only for CHAIN mode! * If >0 transfer tag is set during packet sending and * add_dma_tag() (so also every open_tag()) will move buffer by DWORD, * so remember to align memory! * @returns Pointer to packet2 on success or NULL if memory allocation fail. */ extern packet2_t *packet2_create_from(qword_t *base, qword_t *next, u16 qwords, enum Packet2Type type, enum Packet2Mode mode, u8 tte)`
- `extern void packet2_free(packet2_t *packet2)`
- `data is cleared via memset(). SLOW! */ extern void packet2_reset(packet2_t *packet2, u8 clear_mem)`
- `extern void packet2_print(packet2_t *packet2, u32 qw_count)`
- `extern void packet2_print_qw_count(packet2_t *packet2)`
- `Copy data from b packet into a packet with memcpy() */ extern void packet2_add(packet2_t *a, packet2_t *b)`

Macros/constantes: `__PACKET2_H__`

### `ee/include/packet2_chain.h`

Funções:

- `so * there is NO need to use packet2_chain_close_tag()`
- `so * there is NO need to use packet2_chain_close_tag()`
- `so * there is NO need to use packet2_chain_close_tag()`

Macros/constantes: `PACKET2_COUNT_QWC`, `__PACKET2_CHAIN_H__`

### `ee/include/packet2_types.h`

Funções:

- `dma_tag_t __attribute__((aligned(16)))`
- `qword_t *base __attribute__((aligned(64)))`

Macros/constantes: `__PACKET2_DATA_TYPES_H__`

### `ee/include/packet2_utils.h`

Macros/constantes: `VU_GS_GIFTAG`, `VU_GS_PRIM`, `__PACKET2_UTILS_H__`

### `ee/include/packet2_vif.h`

Funções:

- `re u32 aligned assert((packet2->vif_code_opened_at->cmd & 0x60) == 0x60)`
- `It was UNPACK assert(unpack_num <= 256)`
- `and * fix num value with qwords counted from last packet2_vif_open_unpack(). * @param packet2 Pointer to packet. * @param wl WL Value (look at STCYCL) * @param cl CL Value (look at STCYCL) * @returns Unpacked qwords count */ u32 packet2_vif_close_unpack_auto(packet2_t *packet2, u32 wl, u32 cl)`
- `param dest VU destination address(divided by 16). * @param start Start address. * @param end End address. */ extern void packet2_vif_add_micro_program(packet2_t *packet2, u32 dest, u32 *start, u32 *end)`

Macros/constantes: `MAKE_VIF_CODE`, `__PACKET2_VIF_H__`

### `ee/include/ps2_sbus.h`

Funções:

- `typedef void(*SIF2_CmdHandlerFunc)(SIF2_CmdPkt *pkt, void *param)`
- `SBUS extern int SBUS_init(void)`
- `extern int SBUS_deinit(void)`
- `extern void * SBUS_set_irq_handler(int irq, SBUS_IrqHandlerFunc func, void *param)`
- `extern int SBUS_rem_irq_handler(int irq)`
- `extern u32 SBUS_get_reg(int reg_no)`
- `extern void SBUS_set_reg(int reg_no, u32 val)`
- `extern int SBUS_interrupt_remote(int irq)`
- `SIF2 extern int SIF2_init(void)`
- `extern int SIF2_deinit(void)`
- `extern int SIF2_set_dma(u32 addr, u32 size, u32 attr)`
- `extern void SIF2_sync_dma(void)`
- `SIF2 Command extern int SIF2_init_cmd(void)`
- `extern int SIF2_set_cmd_handler(int cid, SIF2_CmdHandlerFunc func, void *param)`
- `extern int SIF2_rem_cmd_handler(int cid)`
- `extern void SIF2_send_cmd(u32 cid, void *extra, int extra_size)`
- `extern void SBUS_check_intr(void)`

Macros/constantes: `I_SBUS_check_intr`, `I_SBUS_deinit`, `I_SBUS_get_reg`, `I_SBUS_init`, `I_SBUS_interrupt_remote`, `I_SBUS_rem_irq_handler`, `I_SBUS_set_irq_handler`, `I_SBUS_set_reg`, `I_SIF2_deinit`, `I_SIF2_init`, `I_SIF2_init_cmd`, `I_SIF2_rem_cmd_handler`, `I_SIF2_send_cmd`, `I_SIF2_set_cmd_handler`, `I_SIF2_set_dma`, `I_SIF2_sync_dma`, `__PS2_SBUS_H__`, `sbus_IMPORTS_end`, `sbus_IMPORTS_start`

### `ee/include/ps2cam_rpc.h`

Funções:

- `extern int PS2CamGetIRXVersion(void)`
- `extern int PS2CamGetDeviceCount(void)`
- `extern int PS2CamOpenDevice(int device_index)`
- `extern int PS2CamCloseDevice(int handle)`
- `extern int PS2CamGetDeviceStatus(int handle)`
- `extern int PS2CamGetDeviceInfo(int handle, PS2CAM_DEVICE_INFO *info)`
- `extern int PS2CamSetDeviceBandwidth(int handle, char bandwidth)`
- `extern int PS2CamReadPacket(int handle)`
- `extern int PS2CamSetLEDMode(int handle, int mode)`
- `extern int PS2CamSetDeviceConfig(int handle, PS2CAM_DEVICE_CONFIG *cfg)`
- `extern int PS2CamExtractFrame(int handle, char *buffer, int bufsize)`

Macros/constantes: `CAM_CONFIG_MASK_DIMENSION`, `CAM_CONFIG_MASK_DIVIDER`, `CAM_CONFIG_MASK_FRAMERATE`, `CAM_CONFIG_MASK_OFFSET`, `CAM_ERROR_BADHANDLE`, `CAM_ERROR_BADRANGE`, `CAM_ERROR_COMMUNKNOWN`, `CAM_ERROR_DEVNOTREADY`, `CAM_ERROR_DISCONNECTED`, `CAM_ERROR_INVALIDDEVICE`, `CAM_ERROR_MAXHANDLE`, `CAM_ERROR_NODEVICE`, `CAM_ERROR_NONE`, `CAM_ERROR_NOTINIT`, `CAM_ERROR_UNKNOWN`, `CAM_STATUS_CONNECTED`, `CAM_STATUS_CONNECTEDREADY`, `CAM_STATUS_NOTCONNECTED`, `PS2CAM_LED_MODE_OFF`, `PS2CAM_LED_MODE_ON`, `PS2CAM_RPC_CLOSEDEVICE`, `PS2CAM_RPC_GETDEVCOUNT`, `PS2CAM_RPC_GETDEVINFO`, `PS2CAM_RPC_GETDEVSTATUS`, `PS2CAM_RPC_GETIRXVERSION`, `PS2CAM_RPC_INITIALIZE`, `PS2CAM_RPC_OPENDEVICE`, `PS2CAM_RPC_READPACKET`, `PS2CAM_RPC_SETDEVBANDWIDTH`, `PS2CAM_RPC_SETDEVCONFIG`, `PS2CAM_RPC_SETLEDMODE`, `__PS2CAM_RPC_H__`

### `ee/include/ps2ip.h`

Funções:

- `extern void ps2ipDeinit(void)`
- `sync ticks per milisecond(Default: 16). * Use this function to keep timings accurate, if a mode like 480P (~31KHz H-sync) is used instead of NTSC/PAL (~16KHz H-sync). * This function is obsolete, so it is stubbed for compatibility purposes. */ extern void ps2ipSetHsyncTicksPerMSec(unsigned char ticks)`
- `extern err_t ps2ip_input(struct pbuf *p, struct netif *inp)`
- `extern err_t etharp_output(struct netif *netif, struct pbuf *q, const ip_addr_t *ipaddr)`
- `extern err_t tcpip_input(struct pbuf *p, struct netif *inp)`
- `extern void netif_set_default(struct netif *netif)`
- `extern void netif_set_ipaddr(struct netif *netif, const ip4_addr_t *ipaddr)`
- `extern void netif_set_netmask(struct netif *netif, const ip4_addr_t *netmask)`
- `extern void netif_set_gw(struct netif *netif, const ip4_addr_t *gw)`
- `extern void netif_set_up(struct netif *netif)`
- `extern void netif_set_down(struct netif *netif)`
- `extern struct pbuf* pbuf_alloc(pbuf_layer l, u16 size, pbuf_type type)`
- `extern void pbuf_realloc(struct pbuf *p, u16 size)`
- `extern u8 pbuf_header(struct pbuf *p, s16 header_size)`
- `extern void pbuf_ref(struct pbuf *p)`
- `extern u8 pbuf_free(struct pbuf *p)`
- `extern u8 pbuf_clen(struct pbuf *p)`
- `extern void pbuf_chain(struct pbuf *h, struct pbuf *t)`
- `extern struct pbuf* pbuf_dechain(struct pbuf *p)`
- `extern struct pbuf* pbuf_take(struct pbuf *f)`
- `extern struct pbuf* pbuf_coalesce(struct pbuf *p, pbuf_layer layer)`

Macros/constantes: `LIBCGLUE_SYS_SOCKET_ALIASES`, `__PS2IP_H__`

### `ee/include/ps2ips.h`

Funções:

- `extern void ps2ip_deinit(void)`

Macros/constantes: `LIBCGLUE_SYS_SOCKET_ALIASES`, `__PS2IPS_H__`

### `ee/include/ps2kbd.h`

Macros/constantes: `PS2KBD_ALT`, `PS2KBD_BLOCKING`, `PS2KBD_CTRL`, `PS2KBD_DEVFILE`, `PS2KBD_ESCAPE_KEY`, `PS2KBD_FSNAME`, `PS2KBD_GUI`, `PS2KBD_IOCTL_FLUSHBUFFER`, `PS2KBD_IOCTL_RESETKEYMAP`, `PS2KBD_IOCTL_SETALTMAP`, `PS2KBD_IOCTL_SETBLOCKMODE`, `PS2KBD_IOCTL_SETCTRLMAP`, `PS2KBD_IOCTL_SETKEYMAP`, `PS2KBD_IOCTL_SETLEDS`, `PS2KBD_IOCTL_SETREADMODE`, `PS2KBD_IOCTL_SETREPEATRATE`, `PS2KBD_IOCTL_SETSPECIALMAP`, `PS2KBD_KBDFILE`, `PS2KBD_KEYMAP_SIZE`, `PS2KBD_LED_CAPSLOCK`, `PS2KBD_LED_COMPOSE`, `PS2KBD_LED_KANA`, `PS2KBD_LED_MASK`, `PS2KBD_LED_NUMLOCK`, `PS2KBD_LED_SCRLOCK`, `PS2KBD_LEFT_ALT`, `PS2KBD_LEFT_CTRL`, `PS2KBD_LEFT_GUI`, `PS2KBD_LEFT_SHIFT`, `PS2KBD_NONBLOCKING`, `PS2KBD_RAWKEY_DOWN`, `PS2KBD_RAWKEY_UP`, `PS2KBD_READMODE_NORMAL`, `PS2KBD_READMODE_RAW`, `PS2KBD_RIGHT_ALT`, `PS2KBD_RIGHT_CTRL`, `PS2KBD_RIGHT_GUI`, `PS2KBD_RIGHT_SHIFT`, `PS2KBD_SHIFT`, `__PS2KBD_H__`

### `ee/include/ps2mouse.h`

Macros/constantes: `PS2MOUSE_BIND_RPC_ID`, `PS2MOUSE_BTN1`, `PS2MOUSE_BTN1DBL`, `PS2MOUSE_BTN2`, `PS2MOUSE_BTN2DBL`, `PS2MOUSE_BTN3`, `PS2MOUSE_BTN3DBL`, `PS2MOUSE_ENUM`, `PS2MOUSE_GETACCEL`, `PS2MOUSE_GETBOUNDARY`, `PS2MOUSE_GETDBLCLICKTIME`, `PS2MOUSE_GETREADMODE`, `PS2MOUSE_GETTHRES`, `PS2MOUSE_GETVERSION`, `PS2MOUSE_READ`, `PS2MOUSE_READMODE_ABS`, `PS2MOUSE_READMODE_DIFF`, `PS2MOUSE_RESET`, `PS2MOUSE_SETACCEL`, `PS2MOUSE_SETBOUNDARY`, `PS2MOUSE_SETDBLCLICKTIME`, `PS2MOUSE_SETPOSITION`, `PS2MOUSE_SETREADMODE`, `PS2MOUSE_SETTHRES`, `__PS2MOUSE_H__`

### `ee/include/ps2prof.h`

Funções:

- `extern __attribute__((__no_instrument_function__, __no_profile_instrument_function__)) void gprof_start(void)`
- `extern __attribute__((__no_instrument_function__, __no_profile_instrument_function__)) void gprof_stop(const char* filename, int should_dump)`

Macros/constantes: `__PS2PROF_H__`

### `ee/include/ps2sdkapi.h`

Funções:

- `typedef int(*_libcglue_fdman_getfd_cb_t)(void *userdata)`
- `typedef int(*_libcglue_fdman_close_cb_t)(void *userdata)`
- `typedef int(*_libcglue_fdman_read_cb_t)(void *userdata, void *buf, int nbytes)`
- `typedef int(*_libcglue_fdman_lseek_cb_t)(void *userdata, int offset, int whence)`
- `typedef int64_t(*_libcglue_fdman_lseek64_cb_t)(void *userdata, int64_t offset, int whence)`
- `typedef int(*_libcglue_fdman_write_cb_t)(void *userdata, const void *buf, int nbytes)`
- `typedef int(*_libcglue_fdman_ioctl_cb_t)(void *userdata, int request, void *data)`
- `typedef int(*_libcglue_fdman_ioctl2_cb_t)(void *userdata, int request, void *arg, unsigned int arglen, void *buf, unsigned int buflen)`
- `typedef int(*_libcglue_fdman_dread_cb_t)(void *userdata, struct dirent *dir)`
- `typedef int(*_libcglue_fdman_fcntl_f_setfl_cb_t)(void *userdata, int newfl)`
- `typedef int(*_libcglue_fdman_accept_cb_t)(void *userdata, struct _libcglue_fdman_fd_info_ *info, struct sockaddr *addr, socklen_t *addrlen)`
- `typedef int(*_libcglue_fdman_bind_cb_t)(void *userdata, const struct sockaddr *my_addr, socklen_t addrlen)`
- `typedef int(*_libcglue_fdman_connect_cb_t)(void *userdata, const struct sockaddr *serv_addr, socklen_t addrlen)`
- `typedef int(*_libcglue_fdman_listen_cb_t)(void *userdata, int backlog)`
- `typedef ssize_t(*_libcglue_fdman_recv_cb_t)(void *userdata, void *buf, size_t len, int flags)`
- `typedef ssize_t(*_libcglue_fdman_recvfrom_cb_t)(void *userdata, void *buf, size_t len, int flags, struct sockaddr *from, socklen_t *fromlen)`
- `typedef ssize_t(*_libcglue_fdman_recvmsg_cb_t)(void *userdata, struct msghdr *msg, int flags)`
- `typedef ssize_t(*_libcglue_fdman_send_cb_t)(void *userdata, const void *buf, size_t len, int flags)`
- `typedef ssize_t(*_libcglue_fdman_sendto_cb_t)(void *userdata, const void *buf, size_t len, int flags, const struct sockaddr *to, socklen_t tolen)`
- `typedef ssize_t(*_libcglue_fdman_sendmsg_cb_t)(void *userdata, const struct msghdr *msg, int flags)`
- `typedef int(*_libcglue_fdman_getsockopt_cb_t)(void *userdata, int level, int optname, void *optval, socklen_t *optlen)`
- `typedef int(*_libcglue_fdman_setsockopt_cb_t)(void *userdata, int level, int optname, const void *optval, socklen_t optlen)`
- `typedef int(*_libcglue_fdman_shutdown_cb_t)(void *userdata, int how)`
- `typedef int(*_libcglue_fdman_getpeername_cb_t)(void *userdata, struct sockaddr *name, socklen_t *namelen)`
- `typedef int(*_libcglue_fdman_getsockname_cb_t)(void *userdata, struct sockaddr *name, socklen_t *namelen)`
- `typedef int(*_libcglue_fdman_open_cb_t)(_libcglue_fdman_fd_info_t *info, const char *buf, int flags, mode_t mode)`
- `typedef int(*_libcglue_fdman_remove_cb_t)(const char *path)`
- `typedef int(*_libcglue_fdman_rename_cb_t)(const char *old, const char *new_)`
- `typedef int(*_libcglue_fdman_mkdir_cb_t)(const char *path, int mode)`
- `typedef int(*_libcglue_fdman_rmdir_cb_t)(const char *path)`
- `typedef int(*_libcglue_fdman_stat_cb_t)(const char *path, struct stat *buf)`
- `typedef int(*_libcglue_fdman_readlink_cb_t)(const char *path, char *buf, size_t bufsiz)`
- `typedef int(*_libcglue_fdman_symlink_cb_t)(const char *target, const char *linkpath)`
- `typedef int(*_libcglue_fdman_setconfig_cb_t)(const t_ip_info *ip_info)`
- `typedef int(*_libcglue_fdman_getconfig_cb_t)(char *netif_name, t_ip_info *ip_info)`
- `typedef void(*_libcglue_fdman_dns_setserver_cb_t)(u8 numdns, const ip_addr_t *dnsserver)`
- `typedef int(*_libcglue_fdman_socket_cb_t)(_libcglue_fdman_fd_info_t *info, int domain, int type, int protocol)`
- `typedef int(*_libcglue_fdman_select_cb_t)(int n, fd_set *readfds, fd_set *writefds, fd_set *exceptfds, struct timeval *timeout)`
- `typedef int(*_libcglue_fdman_gethostbyname_r_cb_t)(const char *name, struct hostent *ret, char *buf, size_t buflen, struct hostent **result, int *h_errnop)`
- `typedef void(*_libcglue_fdman_freeaddrinfo_cb_t)(struct addrinfo *ai)`
- `typedef int(*_libcglue_fdman_getaddrinfo_cb_t)(const char *nodename, const char *servname, const struct addrinfo *hints, struct addrinfo **res)`
- `typedef u32(*_libcglue_fdman_inet_addr_cb_t)(const char *cp)`
- `typedef int(*_libcglue_fdman_inet_aton_cb_t)(const char *cp, ip4_addr_t *addr)`
- `extern int __path_absolute(const char *in, char *out, int len)`
- `DEPRECATED VERSION USE INSTEAD GetTimerSystemTime return(ps2_clock_t)(GetTimerSystemTime() >> 8)`
- `s64 _libcglue_rtc_get_offset_from_busclk(void)`
- `extern void _libcglue_rtc_update_impl()`
- `extern void _libcglue_rtc_update()`
- `extern off64_t lseek64(int fd, off64_t offset, int whence)`
- `Functions to be used related to timezone extern void _libcglue_timezone_update_impl()`
- `extern void _libcglue_timezone_update()`
- `extern void ps2sdk_setTimezone(int timezone)`
- `extern void ps2sdk_setDaylightSaving(int daylightSaving)`
- `extern _libcglue_fdman_fd_info_t * libcglue_get_fd_info(int fd)`
- `extern int __libcglue_init_stdio(_libcglue_fdman_fd_info_t *info, int fd)`
- `extern int ps2sdk_get_iop_fd(int fd)`
- `extern char * ps2sdk_get_iop_filename(int fd)`
- `extern int _ps2sdk_close(int fd)`
- `extern int _ps2sdk_dclose(int fd)`
- `extern int _ps2sdk_read(int fd, void *buf, int nbytes)`
- `extern int _ps2sdk_lseek(int fd, int offset, int whence)`
- `extern int64_t _ps2sdk_lseek64(int fd, int64_t offset, int whence)`
- `extern int _ps2sdk_write(int fd, const void *buf, int nbytes)`
- `extern int _ps2sdk_ioctl(int fd, int request, void *data)`
- `extern int _ps2sdk_ioctl2(int fd, int request, void *arg, unsigned int arglen, void *buf, unsigned int buflen)`
- `extern int _ps2sdk_dread(int fd, struct dirent *dir)`

Macros/constantes: `LIBCGLUE_SUPPORT_NAMCO_SYSTEM_2x6`, `PS2_CLOCKS_PER_MSEC`, `PS2_CLOCKS_PER_SEC`, `PS2_DISABLE_AUTOSTART_PTHREAD`, `__PS2SDKAPI_H__`

### `ee/include/recycle.h`

Funções:

- `extern reroot * remkroot(size_t mysize)`
- `extern void refree(struct reroot *r)`
- `get a new(cleared) item from the root */ #define renew(r) ((r)->numleft ? \ (((char *)((r)->list+1))+((r)->numleft-=(r)->size)) : renewx(r)) extern char *renewx(struct reroot *r)`
- `void redel(/o_ struct reroot *r, struct recycle *item _o/)`
- `use plain free() to free memory allocated by remalloc() */ extern char *remalloc(size_t len)`

Macros/constantes: `REMAX`, `RESTART`, `redel`, `renew`

### `ee/include/rom0_info.h`

Funções:

- `extern void SetupRomInfo(void)`
- `extern int IsDESRMachine(void)`
- `extern int IsT10K(void)`
- `param romname buffer to hold romname(14 chars long) * @return pointer to buffer containing romname */ extern char *GetRomName(char *romname)`

Macros/constantes: `__ROM0_INFO_H__`

### `ee/include/sbv_patches.h`

Funções:

- `making it impossible(by default) to IOP load modules from EE RAM. * Newer LOADFILE modules do not have this limitation. * Unlike the official patch, this version is not dependent on 0x01e00000-0x01e80000. */ extern int sbv_patch_enable_lmb(void)`
- `extern int sbv_patch_disable_prefix_check(void)`
- `error * * LoadExecPS2() wipes all user-space memory above 0x82000. With this patch, * you can define a different start address to prevent your data from being * overwritten. In order to completely disable the memory clear, simply pass * 0x02000000 to it. */ extern int sbv_patch_user_mem_clear(void *start)`

Macros/constantes: `__SBV_PATCHES_H__`

### `ee/include/screenshot.h`

Funções:

- `extern int ps2_screenshot(void* pTemp, unsigned int VramAdress,unsigned int x,unsigned int y, unsigned int Width, unsigned int Height, unsigned int Psm)`

Macros/constantes: `__SCREENSHOT_H__`

### `ee/include/sifcmd.h`

Macros/constantes: `__SIFCMD_H__`

### `ee/include/sifdma.h`

Funções:

- `extern int sceSifSetDma(SifDmaTransfer_t *dmat, int count)`

Macros/constantes: `SIF_DMA_ERT`, `SIF_DMA_INT_I`, `SIF_DMA_INT_O`, `SIF_REG_ID_SYSTEM`, `SIF_STAT_BOOTEND`, `SIF_STAT_CMDINIT`, `SIF_STAT_SIFINIT`, `SifDmaStat`, `SifSetDma`, `__SIFDMA_H__`

### `ee/include/sifrpc.h`

Macros/constantes: `__SIFRPC_H__`

### `ee/include/sio.h`

Funções:

- `extern int sio_putc(int c)`
- `extern int sio_getc(void)`
- `extern int sio_getc_block(void)`
- `extern size_t sio_write(void *buf, size_t size)`
- `from the serial port until size characters have been read or EOF(RX FIFO is empty). */ extern size_t sio_read(void *buf, size_t size)`
- `extern int sio_puts(const char *str)`
- `extern int sio_putsn(const char *str)`
- `extern char * sio_gets(char *str)`
- `extern void sio_flush(void)`

Macros/constantes: `SIO_BGR`, `SIO_CAUSE_BIT`, `SIO_FCR`, `SIO_FCR_FRSTE`, `SIO_FCR_RFRST`, `SIO_FCR_TFRST`, `SIO_IER`, `SIO_IER_ELSI`, `SIO_IER_ERDAI`, `SIO_ISR`, `SIO_ISR_RX_DATA`, `SIO_ISR_RX_ERROR`, `SIO_ISR_TX_EMPTY`, `SIO_LCR`, `SIO_LCR_UEPS_EVEN`, `SIO_LCR_UEPS_ODD`, `SIO_LCR_UMODE_7BIT`, `SIO_LCR_UMODE_8BIT`, `SIO_LCR_UPEN_OFF`, `SIO_LCR_UPEN_ON`, `SIO_LCR_USBL_1BIT`, `SIO_LCR_USBL_2BITS`, `SIO_LSR`, `SIO_LSR_DR`, `SIO_LSR_FE`, `SIO_LSR_OE`, `SIO_LSR_PE`, `SIO_RXFIFO`, `SIO_TXFIFO`, `__SIO_H__`

### `ee/include/sior_rpc.h`

Macros/constantes: `SIOR_IRX`, `__SIOR_RPC_H__`

### `ee/include/sjis.h`

Funções:

- `extern int strcpy_sjis(short* sjis_buff, const char* ascii_buff)`
- `extern int strcpy_ascii(char* ascii_buff, const short* sjis_buff)`

Macros/constantes: `__SJIS_H__`

### `ee/include/slib.h`

Funções:

- `extern slib_exp_lib_list_t * slib_exp_lib_list()`
- `extern int slib_get_exp_lib(const char *name, slib_exp_lib_t *library)`

Macros/constantes: `__SLIB_H__`

### `ee/include/smem.h`

Funções:

- `extern u32 smem_write(void *addr, void *buf, u32 size)`

Macros/constantes: `SUB_VIRT_MEM`, `__SMEM_H__`

### `ee/include/smod.h`

Funções:

- `extern int smod_get_next_mod(smod_mod_info_t *cur_mod, smod_mod_info_t *next_mod)`
- `extern int smod_get_mod_by_name(const char *name, smod_mod_info_t *info)`

Macros/constantes: `__SMOD_H__`

### `ee/include/standard.h`

Macros/constantes: `SB1MAXVAL`, `SB2MAXVAL`, `SB4MAXVAL`, `SB8MAXVAL`, `UB1BITS`, `UB1MAXVAL`, `UB2BITS`, `UB2MAXVAL`, `UB4BITS`, `UB4MAXVAL`, `UB8BITS`, `UB8MAXVAL`, `align`

### `ee/include/startup.h`

Macros/constantes: `MAX_ARGLEN`, `MAX_ARGS`, `__STARTUP_H__`

### `ee/include/syscallnr.h`

Macros/constantes: `__NR_AddDmacHandler`, `__NR_AddDmacHandler2`, `__NR_AddIntcHandler`, `__NR_AddIntcHandler2`, `__NR_AddSbusIntcHandler`, `__NR_CancelWakeupThread`, `__NR_ChangeThreadPriority`, `__NR_Copy`, `__NR_CpuConfig`, `__NR_CreateEventFlag`, `__NR_CreateSema`, `__NR_CreateThread`, `__NR_Deci2Call`, `__NR_DeleteEventFlag`, `__NR_DeleteSema`, `__NR_DeleteThread`, `__NR_DisableCache`, `__NR_DisableDispatchThread`, `__NR_DisableDmacHandler`, `__NR_DisableIntcHandler`, `__NR_EnableCache`, `__NR_EnableDispatchThread`, `__NR_EnableDmacHandler`, `__NR_EnableIntcHandler`, `__NR_EndOfHeap`, `__NR_ExecPSX`, `__NR_ExitDeleteThread`, `__NR_ExitThread`, `__NR_ExpandScratchPad`, `__NR_FindAddress`, `__NR_FlushCache`, `__NR_GetCop0`, `__NR_GetEntryAddress`, `__NR_GetGsHParam`, `__NR_GetGsVParam`, `__NR_GetMemoryMode`, `__NR_GetMemorySize`, `__NR_GetOsdConfigParam`, `__NR_GetOsdConfigParam2`, `__NR_GetTLBEntry`, `__NR_GetThreadId`, `__NR_GsGetIMR`, `__NR_GsPutIMR`, `__NR_Interrupt2Iop`, `__NR_KExit`, `__NR_KSeg0`, `__NR_MachineType`, `__NR_PSMode`, `__NR_PollSema`, `__NR_ProbeTLBEntry`, `__NR_PutTLBEntry`, `__NR_RFU009`, `__NR_RFU059`, `__NR_RFU060`, `__NR_RFU061`, `__NR_RFU105`, `__NR_ReferSemaStatus`, `__NR_ReferThreadStatus`, `__NR_ReleaseAlarm`, `__NR_ReleaseWaitThread` …

### `ee/include/timer.h`

Funções:

- `extern void _ps2sdk_init_timer(void)`
- `extern void _ps2sdk_deinit_timer_impl(void)`
- `extern void _ps2sdk_deinit_timer(void)`
- `extern s32 InitTimer(s32 in_mode)`
- `extern s32 EndTimer(void)`
- `extern s32 GetTimerPreScaleFactor(void)`
- `extern s32 StartTimerSystemTime(void)`
- `extern s32 StopTimerSystemTime(void)`
- `extern void SetNextComp(u64 time)`
- `extern u64 iGetTimerSystemTime(void)`
- `extern u64 GetTimerSystemTime(void)`
- `extern s32 iAllocTimerCounter(void)`
- `extern s32 AllocTimerCounter(void)`
- `extern s32 iFreeTimerCounter(s32 id)`
- `extern s32 FreeTimerCounter(s32 id)`
- `extern s32 iGetTimerUsedUnusedCounters(u32 *used_counters, u32 *unused_counters)`
- `extern s32 GetTimerUsedUnusedCounters(u32 *used_counters, u32 *unused_counters)`
- `extern s32 iStartTimerCounter(s32 id)`
- `extern s32 StartTimerCounter(s32 id)`
- `extern s32 iStopTimerCounter(s32 id)`
- `extern s32 StopTimerCounter(s32 id)`
- `extern u64 SetTimerCount(s32 id, u64 timer_count)`
- `extern u64 iGetTimerBaseTime(s32 id)`
- `extern u64 GetTimerBaseTime(s32 id)`
- `extern u64 iGetTimerCount(s32 id)`
- `extern u64 GetTimerCount(s32 id)`
- `extern s32 iSetTimerHandler(s32 id, u64 scheduled_time, timer_alarm_handler_t callback_handler, void *arg)`
- `extern s32 SetTimerHandler(s32 id, u64 scheduled_time, timer_alarm_handler_t callback_handler, void *arg)`
- `extern void TimerBusClock2USec(u64 clocks, u32 *seconds_result, u32 *microseconds_result)`
- `extern u64 TimerUSec2BusClock(u32 seconds, u32 microseconds)`
- `extern float TimerBusClock2Freq(s64 clocks)`
- `extern u64 TimerFreq2BusClock(float timer_frequency)`
- `extern u32 cpu_ticks(void)`

Macros/constantes: `K_T2_COMP`, `K_T2_COUNT`, `K_T2_MODE`, `T0_COMP`, `T0_COUNT`, `T0_HOLD`, `T0_MODE`, `T1_COMP`, `T1_COUNT`, `T1_HOLD`, `T1_MODE`, `T2_COMP`, `T2_COUNT`, `T2_MODE`, `T3_COMP`, `T3_COUNT`, `T3_MODE`, `Tn_MODE`, `__TIMER_H__`, `kBUSCLK`, `kBUSCLKBY16`, `kBUSCLKBY256`, `kHBLNK_DTV1080i`, `kHBLNK_DTV480p`, `kHBLNK_NTSC`, `kHBLNK_PAL`

### `ee/include/timer_alarm.h`

Funções:

- `extern s32 iSetTimerAlarm(u64 clock_cycles, timer_alarm_handler_t callback_handler, void *arg)`
- `extern s32 SetTimerAlarm(u64 clock_cycles, timer_alarm_handler_t callback_handler, void *arg)`
- `extern s32 iReleaseTimerAlarm(s32 id)`
- `extern s32 ReleaseTimerAlarm(s32 id)`

Macros/constantes: `__TIMER_ALARM_H__`

### `ee/include/tty.h`

Funções:

- `int sceTtyInit()`
- `int sceTtyWrite(const char *buf, int len)`
- `int sceTtyRead(char *buf, int len)`

Macros/constantes: `TTY_H_`

---

## Semânticas verificadas (a parte que os headers NÃO dizem)

### Semaforos do EE (kernel.h) — VERIFICADO
Fontes: ps2tek (EE_Syscalls 40h/42h/44h/45h) + medida em hardware no forum
ps2dev (viewtopic.php?t=3923) + `ee/kernel/src/thread.c` (uso canónico).

| Chamada | Efeito | Retorno |
|---|---|---|
| `CreateSema(ee_sema_t*)` | cria; só `init_count` e `max_count` são obrigatórios (thread.c do ps2sdk não preenche mais nada) | id (>=0) ou <0 |
| `WaitSema(id)` | se count>0 decrementa; senão **bloqueia** até Signal (decrementa quando acorda) | >=0 ok, <0 erro |
| `SignalSema(id)` | incrementa (pode passar do max_count — medido) e força reschedule se há waiters | >=0 ok, <0 erro |
| `PollSema(id)` | **DECREMENTA SEMPRE o contador, mesmo a negativos** (medido em hardware!) e devolve -1 se "unsuccessful" | -1 falha |
| `DeleteSema(id)` | destroi | |

**LIÇÃO 0.2.170 (congelamento da 0.2.169)**: `PollSema` **não é um trylock
seguro**. Com `max_count=1`, um `if(PollSema(id)!=0) return 0;` podia roubar o
contador (1→0→-1) e o `WaitSema` seguinte ficava bloqueado **para sempre**.
REGRA DO PROJETO: **nunca usar PollSema**; trylock = testar o dono em RAM
(`GetThreadId`) e só tocar no semáforo com `WaitSema`/`SignalSema`.

Recursividade: semaforos do EE **não são recursivos**. Portão com dono
(`io_owner`) + profundidade, como em `src/audio_ring.c` (io_gate_*).

### Identidade de thread
`int GetThreadId(void)` (kernel.h, syscall EE): devolve o id da thread corrente.
É o que identifica o dono do portão de I/O.

### I/O de mass: (iomanX/fileXio)
- `open()` com `O_CREAT|O_WRONLY|O_TRUNC` cria/recria; `O_APPEND` só para anexar.
- O tamanho do ficheiro no FAT **só é publicado no `close()`** (usbhdfsd do
  ps2sdk mantém cache de tamanho): `fsync()` sem `close()` NÃO publica o
  tamanho. Medido no console do utilizador (0.2.162).
- Dois escritores no mesmo ficheiro corrompem o driver (causa raiz do
  congelamento da sessão 0.2.167). TODA a I/O de mass: passa por um portão.
- USB medida no console: leitura sustentada 0,70–0,75 MB/s.

### GS/Draw (gs_gp.h, draw.h, graph.h)
- `texbuffer_t.width` em **pixels**; alinhamento `GRAPH_ALIGN_PAGE` (64 B).
- VRAM total 4 194 304 B; FB duplo 2×640×448 PSMCT16 = 1 146 880 B; o resto é
  do alocador (`graph_vram_allocate`) após `graph_vram_clear()`.
- `draw_texturebuffer`/`draw_texturebuffer_fast` vêm de `libdraw.a` (sem fonte
  no sdk; o comportamento medido está em `docs/PS2SDK_REFERENCE.md`).

### Threads (kernel.h)
`CreateThread(ee_thread_t*)` com `func/stack/stack_size/gp_reg/initial_priority`;
`StartThread(id,arg)`; `SleepThread()` **para a thread para sempre** (proibido em
caminhos de falha — a "rede de segurança" 0.2.160); `DelayThread(us)` dorme.

### Audio (audsrv.h)
`audsrv_set_format/apply/played/submit` + anel de 512 blocos em RAM
(`audio_ring.c`) por causa da leitura USB lenta; sincronia 60 fps ÷ 44100 Hz =
735 amostras/quadro exactas (usado pelo tutorial).

---
_Ficheiro vivo: crescer sempre que uma semântica nova for verificada. Números
de página/linha do fonte do ps2sdk ficam nos headers referenciados acima._

