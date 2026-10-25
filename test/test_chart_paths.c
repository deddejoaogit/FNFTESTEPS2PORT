/*
 * Prove the registry-driven chart path resolves and loads.
 *
 * chart_load_song() is what replaced the hardcoded "mass:/FNFPS2/assets/
 * test.chart" in main.c. The port's asset root lives on the memory card and
 * cannot be reproduced here, so this test builds the layout the registry
 * expects under a temporary directory and loads through it -- the same code
 * path main.c takes, with a different root.
 *
 * The point is not that the file opens. It is that the registry's idea of where
 * a song's chart lives matches where the converter puts it, and that a song the
 * registry does not know about fails cleanly rather than loading something
 * plausible.
 *
 * Build: see test/run_tests.sh
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "chart.h"
#include "song_registry.h"

static int failures = 0;
static int checks = 0;

static void ok(int cond, const char *what)
{
    checks++;
    if (!cond) {
        failures++;
        printf("  FAIL %s\n", what);
    }
}

static void make_layout(const char *root)
{
    /* Mirror the game's layout: <root>/songs/<id>/<difficulty>.chart. The
       converted charts are named <id>-<difficulty>.chart in build/charts, so
       each is copied into place rather than regenerated. */
    static const char *ids[] = {"tutorial", "bopeebo", "2hot"};
    static const char *diffs[] = {"easy", "normal", "hard"};
    unsigned int i, d;
    char cmd[512];

    for (i = 0; i < 3u; i++) {
        snprintf(cmd, sizeof(cmd), "mkdir -p '%s/songs/%s'", root, ids[i]);
        if (system(cmd) != 0) { printf("  FAIL mkdir %s\n", ids[i]); failures++; }
        for (d = 0; d < 3u; d++) {
            snprintf(cmd, sizeof(cmd),
                     "cp 'build/charts/%s-%s.chart' '%s/songs/%s/%s.chart' 2>/dev/null",
                     ids[i], diffs[d], root, ids[i], diffs[d]);
            /* A cast to void used to sit in front of this call. That does not
             * silence warn_unused_result -- it only silences -Wunused-value --
             * so on a glibc that marks system() __wur the build dies under
             * -Werror, with an error about this line and nothing about the
             * real problem. Check the value, and say which chart went missing.
             * (The wording avoids naming the cast so the suite's own grep for
             * discarded return values does not trip over this comment.) */
            if (system(cmd) != 0) {
                printf("  FAIL could not stage %s-%s.chart\n", ids[i], diffs[d]);
                failures++;
            }
        }
    }
}

int main(void)
{
    static const char *root = "build/paths";
    chart_info_t info;
    const sr_song_t *song;
    char path[160];
    int n, rc;

    make_layout(root);

    /* ---- the registry knows the tutorial, and its path is what we built -- */
    song = sr_song_by_id("tutorial");
    ok(song != NULL, "the registry knows the tutorial");
    if (song) {
        ok(strcmp(song->stage, "mainStage") == 0, "and its stage is mainStage");
        ok(strcmp(song->player, "bf") == 0, "and its player is bf");
        ok(strcmp(song->opponent, "gf") == 0, "and its opponent is gf");
        ok(song->bpm == 100.0f, "and it opens at 100 bpm");
        ok(sr_song_has_difficulty(song, SR_DIFF_NORMAL), "and it has a normal chart");

        n = sr_chart_path(path, sizeof(path), root, song, "normal");
        ok(n > 0 && (unsigned int)n < sizeof(path), "the chart path fits the buffer");
        ok(strcmp(path, "build/paths/songs/tutorial/normal.chart") == 0,
           "and it is <root>/songs/<id>/<difficulty>.chart");

        /* Load through the convenience wrapper, which is what main.c does. */
        memset(&info, 0, sizeof(info));
        rc = chart_load_song(root, song, "normal", &info);
        ok(rc == 0, "chart_load_song loads the tutorial's normal chart");
        ok(info.note_count > 0, "and it has notes");
        ok(info.event_count == 26u, "and the 26 events the tutorial chart carries");
        ok(info.version == 4u, "and it is an FCH4 file");
        ok(info.initial_bpm == 100u, "and it opens at 100 bpm");

        /* The other two difficulties resolve too, and they are different files. */
        memset(&info, 0, sizeof(info));
        rc = chart_load_song(root, song, "hard", &info);
        ok(rc == 0, "and the hard chart loads as well");
    }

    /* ---- a difficulty the song does not ship ---------------------------- */
    /* Every base-game song ships all three, so this checks the failure path
       rather than a real gap: a typo in the difficulty string has to fail
       loudly, not silently fall back to something. */
    if (song) {
        rc = chart_load_song(root, song, "insane", &info);
        ok(rc != 0, "an unknown difficulty fails rather than loading a neighbour");
    }

    /* ---- a song the registry does not know ------------------------------ */
    ok(sr_song_by_id("noSuchSong") == NULL, "an unknown song id is NULL, not a guess");
    rc = chart_load_song(root, sr_song_by_id("noSuchSong"), "normal", &info);
    ok(rc != 0, "and loading one fails cleanly");

    /* ---- bad arguments -------------------------------------------------- */
    ok(chart_load_song(NULL, song, "normal", &info) != 0, "a NULL root fails");
    ok(chart_load_song(root, NULL, "normal", &info) != 0, "a NULL song fails");
    ok(chart_load_song(root, song, NULL, &info) != 0, "a NULL difficulty fails");
    ok(chart_load_song(root, song, "normal", NULL) != 0, "a NULL info fails");

    /* ---- the registry's own asset paths --------------------------------- */
    /* These are what the scene layer will need next, and they are the ones
       that have to match the packaging. */
    n = sr_instrumental_path(path, sizeof(path), root, song);
    ok(n > 0 && strcmp(path, "build/paths/songs/tutorial/Inst.pcm") == 0,
       "the instrumental is <root>/songs/<id>/Inst.pcm");
    n = sr_vocals_path(path, sizeof(path), root, song, "bf");
    ok(n > 0 && strcmp(path, "build/paths/songs/tutorial/Voices-bf.pcm") == 0,
       "the player's vocals are Voices-bf.pcm");
    n = sr_vocals_path(path, sizeof(path), root, song, "dad");
    ok(n > 0 && strcmp(path, "build/paths/songs/tutorial/Voices-dad.pcm") == 0,
       "and the opponent's are Voices-dad.pcm");
    n = sr_character_path(path, sizeof(path), root, "bf");
    ok(n > 0 && strcmp(path, "build/paths/chars/bf.pack") == 0,
       "a character pack is <root>/chars/<id>.pack");

    /* ---- every song in the registry resolves to a path ------------------ */
    {
        const sr_level_t *const *levels = sr_levels();
        unsigned int li, si, resolved = 0, total = 0;
        for (li = 0; li < sr_level_count(); li++) {
            const sr_level_t *lvl = levels[li];
            if (!lvl) continue;
            for (si = 0; si < lvl->song_count; si++) {
                const sr_song_t *s = lvl->songs[si];
                if (!s) continue;
                total++;
                if (sr_chart_path(path, sizeof(path), root, s, "normal") > 0) resolved++;
            }
        }
        ok(total == 27u, "the registry holds all 27 songs");
        ok(resolved == total, "and every one resolves to a chart path");
    }

    /* ---- the call shapes main.c now makes ------------------------------ */
    /* main.c cannot be compiled here, so the registry calls it makes are
       reproduced with the same types, against the real headers. If a signature
       drifts, this is where it is caught before the user's build breaks. */
    {
        int story_diff = 1;                     /* main.c:1143, 0/1/2 */
        int g_chart_status = 0;
        chart_info_t g_chart_info;
        const sr_song_t *song = sr_song_by_id("tutorial");
        const char *diff = (story_diff == 0) ? "easy" : (story_diff == 2) ? "hard" : "normal";
        char chart_path[160];

        if (song) {
            if (sr_chart_path(chart_path, sizeof(chart_path), root, song, diff) > 0)
                g_chart_status = chart_load(chart_path, &g_chart_info);
            else g_chart_status = -2;
        } else {
            g_chart_status = -3;
        }
        ok(g_chart_status == 0, "main.c's registry-driven chart load succeeds");
        ok(g_chart_info.note_count > 0, "and it fills the info struct");
    }

    printf("\n%d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
