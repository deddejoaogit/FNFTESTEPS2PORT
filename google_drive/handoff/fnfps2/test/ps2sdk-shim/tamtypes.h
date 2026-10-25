/* Minimal stand-in for ps2sdk's <tamtypes.h>, for PC-side testing only.
 *
 * chart.c is otherwise pure POSIX C (open/read/close/lseek/memset),
 * so the only thing keeping it off the PC is the u8/u16/u32 typedefs. Rather
 * than fork the loader or #ifdef it, this directory is put on the include
 * path for the offline tests and supplies those four typedefs. Nothing in
 * the loader's logic changes, and the PS2 build keeps using the real header
 * because this directory is never on its include path.
 */
#ifndef TAMTYPES_H_SHIM
#define TAMTYPES_H_SHIM

#include <stdint.h>

typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef int8_t   s8;
typedef int16_t  s16;
typedef int32_t  s32;
typedef uint64_t u64;
typedef int64_t  s64;

#endif
