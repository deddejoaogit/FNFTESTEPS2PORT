/* The note kind (`k` in the chart JSON) has to survive the whole pipeline:
 * JSON -> converter -> FCH5 -> chart.c -> the string the game compares against.
 *
 * Why this test exists: the port dropped `k` on the floor for 81 charts, and
 * nine of them use it. Two of those kinds are code (not scripts), so their
 * behaviour is provable from the original's source:
 *   `noanim`        -- NoAnimNoteKind.hx, no singing animation on a hit;
 *   `non_scoreable` -- NonScoreableNoteKind.hx, no points/health/combo.
 * Everything else (weekend-1-*, mom, ugh, hehPrettyGood) is a script this
 * repository does not have, so the port must at least *carry* the name: a
 * future build that receives the scripts then needs no format change.
 *
 * Build: gcc -O2 -Wall -Wextra -Werror -std=gnu11 -Isrc -Isrc/engine \
 *            -Itest/ps2sdk-shim test/test_note_kinds.c src/chart.c \
 *            src/engine/song_registry.c
 * Run:   test_note_kinds <chart> <kind> <expected_count>
 *        test_note_kinds <chart> --all-none 0
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "chart.h"

static int fails = 0;

static void ck(int cond, const char *what)
{
    if (!cond) { printf("  FAIL: %s\n", what); fails++; }
    else printf("  ok:   %s\n", what);
}

int main(int argc, char **argv)
{
    chart_info_t info;
    int rc;
    u32 i, hits = 0, semkind = 0;

    if (argc < 4) {
        printf("usage: %s <chart> <kind|--all-none> <expected>\n", argv[0]);
        return 2;
    }
    rc = chart_load(argv[1], &info);
    if (rc != 0) { printf("  loader returned %d for %s\n", rc, argv[1]); return 1; }
    /* --all-none also runs on the old formats, which have no note kinds at
       all: "every note says none" is exactly the right answer there too. */
    if (strcmp(argv[2], "--all-none") != 0)
        ck(info.version == 5u, "the file is FCH5");

    for (i = 0; i < info.note_count; i++) {
        chart_note_t n;
        const char *kind;
        if (chart_note(i, &n) != 0) { ck(0, "every note reads"); break; }
        kind = chart_note_kind_name(n.kind);
        if (strcmp(argv[2], "--all-none") == 0) {
            if (n.kind != CHART_KIND_NONE || kind != NULL) semkind++;
        } else if (kind && strcmp(kind, argv[2]) == 0) {
            hits++;
        }
    }

    if (strcmp(argv[2], "--all-none") == 0) {
        ck(semkind == 0, "a chart with no `k` (or an old format) gives CHART_KIND_NONE on every note");
    } else {
        char msg[128];
        snprintf(msg, sizeof msg, "the %s notes carry their kind name", argv[2]);
        ck(hits == (u32)strtoul(argv[3], NULL, 10), msg);
    }

    /* The comparison the game uses, proven here so main.c's callers rest on a
       tested function: the right name matches, another name does not, and "no
       kind" matches nothing. */
    {
        u16 k_noanim = CHART_KIND_NONE, k_other = CHART_KIND_NONE;
        for (i = 0; i < info.note_count; i++) {
            chart_note_t n;
            if (chart_note(i, &n) != 0) break;
            if (k_noanim == CHART_KIND_NONE && chart_note_kind_is(n.kind, "noanim")) k_noanim = n.kind;
            if (k_other == CHART_KIND_NONE && n.kind != CHART_KIND_NONE &&
                !chart_note_kind_is(n.kind, "noanim")) k_other = n.kind;
        }
        if (k_noanim != CHART_KIND_NONE) {
            ck(chart_note_kind_is(k_noanim, "noanim") == 1, "chart_note_kind_is matches the exact name");
            ck(chart_note_kind_is(k_noanim, "non_scoreable") == 0, "and not another name");
        }
        if (k_other != CHART_KIND_NONE)
            ck(chart_note_kind_is(k_other, "noanim") == 0, "a different kind is not `noanim`");
        ck(chart_note_kind_is(CHART_KIND_NONE, "noanim") == 0, "no kind matches nothing");
        ck(chart_note_kind_is(CHART_KIND_NONE, NULL) == 0, "a NULL name matches nothing");
    }

    /* The accessor's two boundary answers, so a caller never reads past the
       string table and never mistakes "no kind" for the first string. */
    ck(chart_note_kind_name(CHART_KIND_NONE) == NULL, "CHART_KIND_NONE has no name");
    ck(chart_note_kind_name((u16)(CHART_MAX_STRINGS + 7u)) == NULL,
       "an out-of-range kind index has no name");

    return fails ? 1 : 0;
}
