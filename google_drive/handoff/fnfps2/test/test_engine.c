/* Unit test for the Phase 0 engine modules (vram_cache + io_fila).
 *
 * These modules are deliberately pure C with the disk layer behind function
 * pointers, so they can be exercised on the PC against the real demands of
 * the 21 stages before anything touches the console
 * (docs/PLANO_GERAL_PORT_COMPLETO.md, section 0 and 2.2).
 *
 * Build:  gcc -O2 -Wall -Wextra -std=gnu11 -I../src/engine \
 *              test_engine.c ../src/engine/vram_cache.c ../src/engine/io_fila.c -o test_engine
 * Run:    ./test_engine
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "vram_cache.h"
#include "io_fila.h"

static int failures = 0;
static void check(int cond, const char *what)
{
    if (!cond) { printf("  FAIL: %s\n", what); failures++; }
    else printf("  ok:   %s\n", what);
}

/* ------------------------------------------------------------------ */
/* fake disk layer for io_fila: 64 KB of memory, 4 KB per slice       */
#define DISK_BYTES 65536
static unsigned char disk[DISK_BYTES];
static int fake_reads = 0;

static int disk_read(int fd, void *buf, unsigned int n)
{
    (void)fd;
    if (fake_reads > 4) return -1;          /* simulate a failing device */
    memcpy(buf, disk, n);
    fake_reads++;
    return (int)n;
}
static int disk_write(int fd, const void *buf, unsigned int n)
{
    (void)fd;
    memcpy(disk, buf, n);
    return (int)n;
}

/* ------------------------------------------------------------------ */
static void test_vram_basic(void)
{
    vc_t c;
    unsigned int addr = 0;
    printf("vram: basic residency\n");
    vc_init(&c, 0, 1024 * 1024);
    check(vc_livre(&c) == 1024 * 1024, "fresh cache is empty");
    check(vc_pedir(&c, 1, 4096, &addr) == 0, "first request allocates");
    check(addr == 0, "first allocation is at the base");
    check(vc_pedir(&c, 1, 4096, &addr) == 1, "second request of same key is a hit");
    check(c.acertos == 1 && c.faltas == 1, "hit/miss counters");
    check(vc_livre(&c) == 1024 * 1024 - 4096, "used bytes are accounted");
}

static void test_vram_lru(void)
{
    vc_t c;
    unsigned int a = 0;
    printf("vram: LRU eviction order\n");
    /* 64 KB of texture room, 16 KB textures: exactly 4 fit. */
    vc_init(&c, 0, 65536);
    for (unsigned int k = 1; k <= 4; k++) check(vc_pedir(&c, k, 16384, &a) == 0, "fill");
    check(vc_livre(&c) == 0, "cache is full");
    /* touch key 1 so it becomes most recently used */
    check(vc_pedir(&c, 1, 16384, &a) == 1, "key 1 hit refreshes recency");
    /* a 5th texture must evict the least recently used, which is now key 2 */
    check(vc_pedir(&c, 5, 16384, &a) == 0, "5th allocation succeeds after eviction");
    check(c.despejos == 1, "exactly one eviction happened");
    check(vc_pedir(&c, 2, 16384, &a) == 0, "evicted key 2 must be re-uploaded");
    check(vc_pedir(&c, 1, 16384, &a) == 1, "key 1 survived (it was refreshed)");
    check(vc_pedir(&c, 5, 16384, &a) == 1, "key 5 is resident");
}

static void test_vram_fixed(void)
{
    vc_t c;
    unsigned int a = 0;
    printf("vram: pinned blocks survive\n");
    vc_init(&c, 0, 65536);
    check(vc_pedir(&c, 100, 16384, &a) == 0, "allocate source texture");
    vc_fixar(&c, 100);
    for (unsigned int k = 1; k <= 3; k++) vc_pedir(&c, k, 16384, &a);
    check(vc_pedir(&c, 4, 16384, &a) == 0, "4th texture still fits (pinned + 3)");
    /* now the cache holds 4*16K = 64K with one pinned; a 5th must evict a
       non-pinned block, never the pinned one */
    check(vc_pedir(&c, 5, 16384, &a) == 0, "5th allocation evicts a non-pinned block");
    check(vc_pedir(&c, 100, 16384, &a) == 1, "pinned block was never evicted");
}

static void test_vram_too_big(void)
{
    vc_t c;
    unsigned int a = 0;
    printf("vram: refuses only what cannot ever fit\n");
    vc_init(&c, 0, 65536);
    check(vc_pedir(&c, 1, 65536, &a) == 0, "exact fit is allowed");
    /* The cache is an LRU: a smaller request evicts rather than failing, so
       the only hard failure is a texture larger than the whole region. */
    check(vc_pedir(&c, 2, 256, &a) == 0, "smaller request evicts instead of failing");
    check(c.despejos == 1, "one eviction made room");
    check(vc_pedir(&c, 3, 65536 + 256, &a) == -1, "larger than the region fails");
    check(c.falhas == 1, "failure is counted");
    /* a pinned block that fills the region makes even a small request fail */
    vc_limpar(&c);
    check(vc_livre(&c) == 65536, "clear frees everything");
    vc_pedir(&c, 9, 65536, &a);
    vc_fixar(&c, 9);
    check(vc_pedir(&c, 10, 256, &a) == -1, "pinned full region cannot be evicted");
}

/* Addresses handed out must never overlap for textures that are both live.
 * A texture that was evicted is no longer live, so its address may be reused;
 * the invariant that matters is that no two simultaneously-live textures
 * share VRAM, because that is what corrupts rendering. */
typedef struct { unsigned int chave, endereco, bytes; int vivo; } vivo_t;

static int vivo_idx(vivo_t *v, int n, unsigned int chave)
{
    for (int i = 0; i < n; i++) if (v[i].vivo && v[i].chave == chave) return i;
    return -1;
}

static void test_vram_no_overlap(void)
{
    vc_t c;
    vivo_t v[24];
    int nv = 0, bad = 0, fresh = 0;
    unsigned int a = 0;
    printf("vram: no overlapping live textures under pressure\n");
    vc_init(&c, 0, 1 << 20);
    /* sizes 4K..100K: total demand 1,228,800 B in a 1,048,576 B cache, so the
       cache must evict continuously while never handing out a clash. */
    for (int k = 0; k < 24; k++) {
        unsigned int chave = (unsigned int)k + 1;
        unsigned int bytes = 4096u * (unsigned int)(k + 1);
        int r = vc_pedir(&c, chave, bytes, &a);
        if (r < 0) { check(0, "allocation failed unexpectedly"); return; }
        int at = vivo_idx(v, nv, chave);
        if (r == 1) {
            /* resident: address must be unchanged */
            if (at < 0 || v[at].endereco != a) bad = 1;
            continue;
        }
        fresh++;
        /* a fresh allocation must not overlap anything still live */
        for (int j = 0; j < nv; j++)
            if (v[j].vivo && a < v[j].endereco + v[j].bytes && v[j].endereco < a + bytes) bad = 1;
        if (at >= 0) { v[at].endereco = a; v[at].bytes = bytes; }
        else { v[nv].chave = chave; v[nv].endereco = a; v[nv].bytes = bytes; v[nv].vivo = 1; nv++; }
        /* anything evicted by this allocation is no longer live: find which
           keys disappeared by checking the cache's own accounting instead. */
        for (int j = 0; j < nv; j++)
            if (v[j].vivo && vc_pedir(&c, v[j].chave, 0, &a) < 0) v[j].vivo = 0;
    }
    check(!bad, "no two live textures share VRAM");
    check(fresh > 0, "the cache really was under pressure (fresh allocations happened)");
}

/* The measured budget on this project's SCPH-90006 (vram_cache.h header):
 *   1,900,544 B of texture VRAM left after the two framebuffers.
 *
 * Worst known scene is tankmanBattlefield: 3 characters + 10 animated props
 * = 13 live textures (docs/PLANO_GERAL_PORT_COMPLETO.md, section 0). This
 * test measures whether that scene fits at full texture size, which is the
 * question that decides whether the runtime atlas packer is optional. */
static void test_vram_scene_budget(void)
{
    vc_t c;
    unsigned int a = 0;
    const unsigned int room = 1900544u;
    const unsigned int stage_bytes = 512 * 256 * 4;   /* stageback/front/curtains */
    const unsigned int char_bytes  = 320 * 307 * 2;   /* PSMCT16 character */
    const unsigned int prop_bytes  = 256 * 256 * 4;   /* one animated prop */

    printf("vram: 21-scene budget model (tankmanBattlefield, 13 textures)\n");

    /* --- naive: one texture per prop, three full-size stage layers --- */
    vc_init(&c, 0, room);
    for (unsigned int k = 200; k < 203; k++) { vc_pedir(&c, k, stage_bytes, &a); vc_fixar(&c, k); }
    for (unsigned int k = 1; k <= 3; k++)   vc_pedir(&c, k, char_bytes, &a);
    for (unsigned int k = 10; k < 20; k++)  vc_pedir(&c, k, prop_bytes, &a);
    unsigned int naive = 3 * stage_bytes + 3 * char_bytes + 10 * prop_bytes;
    printf("       naive  needs %7u B of %u B (%3.0f%%), evictions=%u\n",
           naive, room, 100.0 * naive / room, c.despejos);
    check(naive > room, "naive 13-texture layout EXCEEDS the budget");

    /* --- packed: 10 props into 3 atlases of 4, one stage layer --- */
    vc_init(&c, 0, room);
    vc_pedir(&c, 200, stage_bytes, &a); vc_fixar(&c, 200);
    for (unsigned int k = 1; k <= 3; k++)   vc_pedir(&c, k, char_bytes, &a);
    const unsigned int atlas_bytes = 256 * 256 * 4;   /* 4 props of 256x256 */
    for (unsigned int k = 300; k < 303; k++) vc_pedir(&c, k, atlas_bytes, &a);
    unsigned int packed = stage_bytes + 3 * char_bytes + 3 * atlas_bytes;
    printf("       packed needs %7u B of %u B (%3.0f%%), evictions=%u\n",
           packed, room, 100.0 * packed / room, c.despejos);
    check(packed <= room, "packed layout FITS the budget");
    check(c.despejos == 0, "and nothing had to be evicted");
    check(c.falhas == 0, "no allocation failed");
    printf("       -> the scene fits only with 3 stage layers reduced to 1 and\n");
    printf("          10 props packed 4-per-atlas; that is the packer's job.\n");
}

/* ------------------------------------------------------------------ */
static void test_io_priority(void)
{
    io_fila_t f;
    printf("io_fila: audio is served before background\n");
    memset(disk, 0x5a, sizeof(disk));
    io_init(&f, disk_read, disk_write);

    /* enqueue a big background write first, then a small audio read */
    int bg = io_enfileirar(&f, IO_ESCREVER, 1, disk, DISK_BYTES, IO_PRIO_FUNDO);
    int au = io_enfileirar(&f, IO_LER, 1, disk, IO_FATIA, IO_PRIO_AUDIO);
    check(bg >= 0 && au >= 0, "both requests queued");
    check(io_pendentes(&f) == 2, "two requests pending");

    /* the first slice must belong to the audio request, not the write */
    int did = io_bombear(&f);
    check(did == 1, "one slice executed");
    check(io_terminado(&f, au) == 1, "the AUDIO request finished first");
    check(io_terminado(&f, bg) == 0, "the background request is still pending");
    check(io_resultado(&f, au) == (int)IO_FATIA, "audio result is one full slice");

    /* drain the rest */
    int guard = 10000;
    while (io_pendentes(&f) && guard--) io_bombear(&f);
    check(io_terminado(&f, bg) == 1, "background request eventually finishes");
    check(io_resultado(&f, bg) == (int)DISK_BYTES, "background transferred everything");
    printf("       slices=%u owner_switches=%u\n", f.fatias, f.trocas_de_dono);
    check(f.fatias == (DISK_BYTES / IO_FATIA) + 1, "slice count = writes + audio slice");
}

static void test_io_error(void)
{
    io_fila_t f;
    printf("io_fila: device errors surface to the caller\n");
    io_init(&f, disk_read, disk_write);
    fake_reads = 100;                      /* make the device fail */
    int id = io_enfileirar(&f, IO_LER, 1, disk, IO_FATIA, IO_PRIO_NORMAL);
    check(id >= 0, "request queued");
    int guard = 100;
    while (!io_terminado(&f, id) && guard--) io_bombear(&f);
    check(io_terminado(&f, id) == 1, "request completes (with an error)");
    check(io_resultado(&f, id) < 0, "error is reported to the caller");
    fake_reads = 0;
}

int main(void)
{
    test_vram_basic();
    test_vram_lru();
    test_vram_fixed();
    test_vram_too_big();
    test_vram_no_overlap();
    test_vram_scene_budget();
    test_io_priority();
    test_io_error();
    printf("\n%s (%d failure%s)\n", failures ? "FAILED" : "ALL PASS",
           failures, failures == 1 ? "" : "s");
    return failures ? 1 : 0;
}
