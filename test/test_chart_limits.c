/* Assert the loader's capacity against every converted chart.
 *
 * Build: gcc -O2 -Wall -Wextra -Werror -std=gnu11 -Isrc -Isrc/engine \
 *            -Itest/ps2sdk-shim test/test_chart_limits.c -o test_chart_limits
 * Run:   test_chart_limits <dir-of-.chart-files>
 *
 * Why this exists: 2hot-hard has 1.035 notes and the loader capped notes at
 * 1.024, so the hardest chart in the game failed to load with -5 and nothing
 * said why. Loading every chart already proves the limits are adequate -- the
 * loader rejects anything over -- but a bare "loader returned -5" is a poor
 * error message. This walks the same files and reports, per limit, the
 * observed maximum and which chart set it, so a regression names the song.
 */

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "chart.h"

static int fails = 0;
static void ck(int c, const char *what)
{
    if (!c) { printf("  FAIL: %s\n", what); fails++; }
    else printf("  ok:   %s\n", what);
}

static unsigned int rd16(const unsigned char *p)
{
    return (unsigned int)p[0] | ((unsigned int)p[1] << 8);
}
static unsigned long rd32(const unsigned char *p)
{
    return (unsigned long)p[0] | ((unsigned long)p[1] << 8)
         | ((unsigned long)p[2] << 16) | ((unsigned long)p[3] << 24);
}

int main(int argc, char **argv)
{
    const char *dir = argc >= 2 ? argv[1] : "build/charts";
    DIR *d;
    struct dirent *ent;
    char path[1024];
    unsigned long max_notes = 0, max_bpms = 0, max_sections = 0, max_events = 0;
    char who_notes[256] = "", who_bpms[256] = "", who_sections[256] = "", who_events[256] = "";
    unsigned long stride = 16;
    int files = 0, bad = 0;

    d = opendir(dir);
    if (!d) {
        printf("  FAIL: cannot open %s (run the converter first)\n", dir);
        return 1;
    }

    while ((ent = readdir(d)) != NULL) {
        unsigned char header[40];
        FILE *f;
        size_t got;
        unsigned long notes, bpms, sections, events;
        const char *ext;

        ext = strrchr(ent->d_name, '.');
        if (!ext || strcmp(ext, ".chart") != 0) continue;
        snprintf(path, sizeof path, "%s/%s", dir, ent->d_name);

        f = fopen(path, "rb");
        if (!f) { bad++; continue; }
        got = fread(header, 1, sizeof header, f);
        fclose(f);
        /* all three containers: the first 28 bytes of the header mean the same
           thing in each, so only the magic, the note stride (v5) and the event
           stride differ */
        if (got != sizeof header
            || (memcmp(header, "FCH3", 4) != 0 && memcmp(header, "FCH4", 4) != 0
                && memcmp(header, "FCH5", 4) != 0)) {
            bad++;
            continue;
        }

        files++;
        notes = rd32(header + 8);
        bpms = rd16(header + 12);
        sections = rd16(header + 14);
        events = rd16(header + 16);
        /* v4 events are 32 B, v3 events 16 B; the counts are what matter here */
        stride = rd16(header + 24);
        if (stride != 16 && stride != 32) { bad++; continue; }
        /* v5 grows the note record to 16 B; v3/v4 keep 12 B */
        if (memcmp(header, "FCH5", 4) == 0 ? rd16(header + 6) != 16
                                          : rd16(header + 6) != 12) { bad++; continue; }

        if (notes > max_notes) { max_notes = notes; snprintf(who_notes, sizeof who_notes, "%s", ent->d_name); }
        if (bpms > max_bpms) { max_bpms = bpms; snprintf(who_bpms, sizeof who_bpms, "%s", ent->d_name); }
        if (sections > max_sections) { max_sections = sections; snprintf(who_sections, sizeof who_sections, "%s", ent->d_name); }
        if (events > max_events) { max_events = events; snprintf(who_events, sizeof who_events, "%s", ent->d_name); }

        /* report the actual breach, not a bare -5 */
        if (notes > CHART_MAX_NOTES)
            printf("  FAIL: %s has %lu notes, over the %u limit\n",
                   ent->d_name, notes, CHART_MAX_NOTES), fails++;
        if (bpms > CHART_MAX_BPMS)
            printf("  FAIL: %s has %lu bpm changes, over the %u limit\n",
                   ent->d_name, bpms, CHART_MAX_BPMS), fails++;
        if (sections > CHART_MAX_SECTIONS)
            printf("  FAIL: %s has %lu sections, over the %u limit\n",
                   ent->d_name, sections, CHART_MAX_SECTIONS), fails++;
        if (events > CHART_MAX_EVENTS)
            printf("  FAIL: %s has %lu events, over the %u limit\n",
                   ent->d_name, events, CHART_MAX_EVENTS), fails++;
    }
    closedir(d);

    printf("  scanned %d charts (%d unreadable)\n", files, bad);
    printf("  max notes    %4lu  (limit %4u)  %s\n", max_notes, CHART_MAX_NOTES, who_notes);
    printf("  max bpm      %4lu  (limit %4u)  %s\n", max_bpms, CHART_MAX_BPMS, who_bpms);
    printf("  max sections %4lu  (limit %4u)  %s\n", max_sections, CHART_MAX_SECTIONS, who_sections);
    printf("  max events   %4lu  (limit %4u)  %s\n", max_events, CHART_MAX_EVENTS, who_events);

    ck(bad == 0, "every .chart file in the directory is a readable FCH3/FCH4/FCH5");
    ck(files > 0, "at least one chart was scanned");
    ck(fails == 0, "no chart exceeds any loader limit");

    printf("  %s\n", fails ? "FAILED" : "ok");
    return fails ? 1 : 0;
}
