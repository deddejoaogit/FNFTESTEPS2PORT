#include "lz4_bloco.h"

#include <string.h>

unsigned int lz4_u32le(const unsigned char *p)
{
    return (unsigned int)p[0] | ((unsigned int)p[1] << 8) |
           ((unsigned int)p[2] << 16) | ((unsigned int)p[3] << 24);
}

int lz4_bloco_decode(const unsigned char *src, unsigned int src_size,
                     unsigned char *dst, unsigned int dst_size)
{
    const unsigned char *ip = src, *iend = src + src_size;
    unsigned char *op = dst, *oend = dst + dst_size;
    if ((!src && src_size) || (!dst && dst_size)) return LZ4_ERR_LIT_FORA;
    while (ip < iend) {
        unsigned int token = *ip++, lit, match, off, v;
        lit = token >> 4;
        if (lit == 15) {
            do {
                if (ip >= iend) return LZ4_ERR_TRUNCADO_LIT;
                v = *ip++;
                lit += v;
            } while (v == 255);
        }
        if ((unsigned int)(iend - ip) < lit) return LZ4_ERR_LIT_FORA;
        if ((unsigned int)(oend - op) < lit) return LZ4_ERR_LIT_FORA;
        memcpy(op, ip, lit);
        ip += lit;
        op += lit;
        if (ip == iend) break;             /* ultima sequencia: so' literais */
        if ((unsigned int)(iend - ip) < 2) return LZ4_ERR_TRUNCADO_OFF;
        off = (unsigned int)ip[0] | ((unsigned int)ip[1] << 8);
        ip += 2;
        if (!off || off > (unsigned int)(op - dst)) return LZ4_ERR_OFF;
        match = token & 15;
        if (match == 15) {
            do {
                if (ip >= iend) return LZ4_ERR_TRUNCADO_MATCH;
                v = *ip++;
                match += v;
            } while (v == 255);
        }
        match += 4;
        if ((unsigned int)(oend - op) < match) return LZ4_ERR_MATCH_FORA;
        /* Byte a byte de proposito: o match pode se sobrepor ao que esta'
           sendo escrito (periodo menor que o proprio match). */
        while (match--) {
            *op = *(op - off);
            op++;
        }
    }
    return op == oend ? LZ4_BLOCO_OK : LZ4_ERR_TAMANHO;
}
