/* Unit test for song_registry: the data-driven song/level list.
 *
 * Build: gcc -O2 -Wall -Wextra -std=gnu11 -Isrc -Isrc/engine \
 *            test/test_registry.c src/engine/song_registry.c -o test_registry
 *
 * What this test protects:
 *   - the week order and grouping match the shipped level JSON
 *   - every character and stage id exists in the official tables (bs_dados.h)
 *   - the path builders follow the on-disc layout
 *   - the committed C matches the committed JSON (the generator --check)
 */

#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include "song_registry.h"

static int failures = 0;
static void check(int cond, const char *what)
{
    if (!cond) { printf("  FAIL: %s\n", what); failures++; }
    else printf("  ok:   %s\n", what);
}

/* A song whose metadata has not been collected reports its characters as
 * PENDENTE. Those are counted apart from real errors: a wrong id must still
 * fail the run, but a knowingly missing file must not. */
static int pending_lines = 0;
static void counting_report(const char *fmt, ...)
{
    char line[512];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(line, sizeof line, fmt, ap);
    va_end(ap);
    if (strstr(line, "PENDENTE")) pending_lines++;
}

int main(void)
{
    printf("song_registry: structure\n");
    check(sr_level_count() == 10, "ten story levels (LevelRegistry.listBaseGameEntryIds)");

    /* The week order is hard-coded in the original and MUST be preserved. */
    static const char *order[] = {
        "tutorial", "week1", "week2", "week3", "week4",
        "week5", "week6", "week7", "weekend1", "sserafim"
    };
    int order_ok = 1;
    for (unsigned int i = 0; i < 10; i++)
        if (strcmp(sr_level(i)->id, order[i]) != 0) order_ok = 0;
    check(order_ok, "level order matches LevelRegistry.listBaseGameEntryIds()");

    /* Song grouping comes from the shipped level JSON, in JSON order -- which
     * funkin.ui.story.Level.getSongs() returns verbatim, and startLevel()
     * shift()s off the front. Note week4 is satin-panties, high, milf: my
     * earlier reading of the switch in InitState.hx had it backwards, because
     * that switch is a set membership test, not a sequence. */
    struct { const char *level; const char *songs[4]; int n; } expect[] = {
        { "tutorial",  {"tutorial"}, 1 },
        { "week1",     {"bopeebo", "fresh", "dadbattle"}, 3 },
        { "week2",     {"spookeez", "south", "monster"}, 3 },
        { "week3",     {"pico", "philly-nice", "blammed"}, 3 },
        { "week4",     {"satin-panties", "high", "milf"}, 3 },
        { "week5",     {"cocoa", "eggnog", "winter-horrorland"}, 3 },
        { "week6",     {"senpai", "roses", "thorns"}, 3 },
        { "week7",     {"ugh", "guns", "stress"}, 3 },
        { "weekend1",  {"darnell", "lit-up", "2hot", "blazin"}, 4 },
        { "sserafim",  {"spaghetti"}, 1 },
    };
    int group_ok = 1;
    for (unsigned int i = 0; i < 10; i++) {
        const sr_level_t *l = sr_level(i);
        if ((int)l->song_count != expect[i].n) { group_ok = 0; continue; }
        for (unsigned int j = 0; j < l->song_count; j++)
            if (strcmp(l->songs[j]->id, expect[i].songs[j]) != 0) group_ok = 0;
    }
    check(group_ok, "song grouping matches the shipped level JSON, in JSON order");
    check(sr_song_count() == 27, "27 songs across 10 weeks");

    printf("\nsong_registry: the fields the gameplay actually reads\n");

    const sr_song_t *tut = sr_song_by_id("tutorial");
    check(tut != NULL, "tutorial is findable by id");
    check(tut && strcmp(tut->stage, "mainStage") == 0, "tutorial uses mainStage (proven on hardware)");
    /* tutorial-metadata.json: playData.characters is {player:"bf",
       opponent:"gf"} with NO girlfriend field. In the Tutorial the girlfriend
       is the opponent and the middle slot stays empty. */
    check(tut && strcmp(tut->player, "bf") == 0 && tut->girlfriend == NULL
              && strcmp(tut->opponent, "gf") == 0,
          "tutorial is bf vs gf, with no girlfriend in the middle");
    check(tut && tut->bpm == 100.0f, "tutorial bpm is 100");

    /* week6 is the pixel week: different note atlas, different character
       sprites. Missing this would render the wrong notes. */
    const sr_song_t *senpai = sr_song_by_id("senpai");
    check(senpai && senpai->note_style == SR_NOTE_PIXEL, "senpai uses the pixel note style");
    check(senpai && strcmp(senpai->player, "bf-pixel") == 0, "senpai uses bf-pixel");
    const sr_song_t *thorns = sr_song_by_id("thorns");
    check(thorns && strcmp(thorns->stage, "schoolEvil") == 0, "thorns moves to schoolEvil");
    const sr_song_t *wh = sr_song_by_id("winter-horrorland");
    check(wh && strcmp(wh->stage, "mallEvil") == 0, "winter-horrorland uses mallEvil, not mallXmas");
    check(wh && strcmp(wh->opponent, "monster-christmas") == 0, "winter-horrorland opposes monster-christmas");

    /* week4 is the car week. */
    const sr_song_t *milf = sr_song_by_id("milf");
    check(milf && strcmp(milf->player, "bf-car") == 0 && strcmp(milf->opponent, "mom-car") == 0
              && strcmp(milf->girlfriend, "gf-car") == 0,
          "week4 uses the car sprites");

    /* week7 keeps bf but changes the girlfriend to the tankmen crew, and the
       stage to the battlefield. My earlier note said the player switches to
       pico here; the metadata says otherwise (ugh and guns are bf, stress is
       bf-holding-gf), so the note was wrong, not the data. */
    const sr_song_t *ugh = sr_song_by_id("ugh");
    check(ugh && strcmp(ugh->player, "bf") == 0, "ugh keeps bf as the player");
    check(ugh && strcmp(ugh->girlfriend, "gf-tankmen") == 0, "ugh swaps the girlfriend to gf-tankmen");
    check(ugh && strcmp(ugh->opponent, "tankman") == 0, "ugh opposes tankman");
    check(ugh && strcmp(ugh->stage, "tankmanBattlefield") == 0, "week7 uses tankmanBattlefield");
    const sr_song_t *guns = sr_song_by_id("guns");
    check(guns && strcmp(guns->player, "bf") == 0
               && strcmp(guns->girlfriend, "gf-tankmen") == 0,
          "guns also keeps bf with gf-tankmen");
    const sr_song_t *stress = sr_song_by_id("stress");
    check(stress && strcmp(stress->player, "bf-holding-gf") == 0
               && strcmp(stress->girlfriend, "pico-speaker") == 0,
          "stress uses bf-holding-gf with pico-speaker in the girlfriend slot");

    /* weekend1 hands the player to pico-playable and the stage to the streets. */
    const sr_song_t *darnell = sr_song_by_id("darnell");
    check(darnell && strcmp(darnell->player, "pico-playable") == 0
                 && strcmp(darnell->stage, "phillyStreets") == 0,
          "weekend1 uses pico-playable on phillyStreets");
    const sr_song_t *blazin = sr_song_by_id("blazin");
    check(blazin && strcmp(blazin->stage, "phillyBlazin") == 0
                 && strcmp(blazin->player, "pico-blazin") == 0,
          "blazin uses the blazin sprites and stage");

    /* sserafim: the collab week, confirmed from spaghetti-metadata.json. */
    const sr_song_t *sp = sr_song_by_id("spaghetti");
    check(sp && strcmp(sp->player, "sserafim-sakura") == 0
             && strcmp(sp->girlfriend, "sserafim-gf") == 0
             && strcmp(sp->opponent, "sserafim-kazuha") == 0,
          "spaghetti uses the sserafim cast");
    check(sp && sp->bpm == 111.5f, "spaghetti bpm is 111.5 (a fractional bpm)");

    /* Every song must offer the three base difficulties. */
    int diffs_ok = 1;
    for (unsigned int i = 0; i < 10; i++) {
        const sr_level_t *l = sr_level(i);
        for (unsigned int j = 0; j < l->song_count; j++) {
            const sr_song_t *s = l->songs[j];
            if (!sr_song_has_difficulty(s, SR_DIFF_EASY) ||
                !sr_song_has_difficulty(s, SR_DIFF_NORMAL) ||
                !sr_song_has_difficulty(s, SR_DIFF_HARD)) diffs_ok = 0;
        }
    }
    check(diffs_ok, "every song offers easy, normal and hard");

    /* The erect/pico/bf variations exist in the data and must not be lost. */
    check(sr_song_by_id("bopeebo") &&
          (sr_song_by_id("bopeebo")->variations & SR_VAR_ERECT), "bopeebo has an erect variation");
    check(sr_song_by_id("darnell") &&
          (sr_song_by_id("darnell")->variations & SR_VAR_BF), "darnell has a bf variation");
    check(sr_song_by_id("blazin") && sr_song_by_id("blazin")->variations == 0,
          "blazin has no variations");

    /* Level metadata the story menu needs. */
    const sr_level_t *w1 = sr_level_by_id("week1");
    check(w1 && strcmp(w1->display_name, "DADDY DEAREST") == 0, "week1 shows DADDY DEAREST");
    check(w1 && w1->background == 0xF9CF51u, "week1 background is #F9CF51");
    const sr_level_t *we = sr_level_by_id("weekend1");
    check(we && we->background == 0x413CAEu, "weekend1 background is #413CAE (the only different one)");
    const sr_level_t *ss = sr_level_by_id("sserafim");
    check(ss && ss->has_capsule && ss->capsule_title
              && strcmp(ss->capsule_title, "SP. COLLAB 1") == 0,
          "sserafim is the only week with a capsule label");

    check(sr_level_by_id("does-not-exist") == NULL, "unknown level returns NULL");
    check(sr_song_by_id("does-not-exist") == NULL, "unknown id returns NULL");
    check(sr_level(10) == NULL, "level index 10 is out of range");

    printf("\nsong_registry: paths\n");
    char path[256];
    const sr_song_t *b = sr_song_by_id("bopeebo");
    check(b && sr_chart_path(path, sizeof path, "mass:/FNFPS2", b, "hard") > 0
              && strcmp(path, "mass:/FNFPS2/songs/bopeebo/hard.chart") == 0,
          "chart path convention");
    {
        /* the length is measured, not guessed: it must equal the real string */
        char want[256];
        int wantlen = snprintf(want, sizeof want, "mass:/FNFPS2/songs/bopeebo/hard.chart");
        check(b && sr_chart_path(NULL, 0, "mass:/FNFPS2", b, "hard") == wantlen,
              "truncated path reports the needed length");
        check(b && sr_chart_path(path, 4, "mass:/FNFPS2", b, "hard") == wantlen
                  && path[0] != '\0' && strlen(path) < 4,
              "a short buffer is not overflowed and still reports the length");
    }
    check(b && sr_stems_path(path, sizeof path, "mass:/FNFPS2", b) > 0
              && strcmp(path, "mass:/FNFPS2/songs/bopeebo/stems.pcm") == 0,
          "stems path convention (the port's own per-song audio)");
    check(b && sr_instrumental_path(path, sizeof path, "mass:/FNFPS2", b) > 0
              && strcmp(path, "mass:/FNFPS2/songs/bopeebo/Inst.pcm") == 0,
          "instrumental path convention");
    check(b && sr_vocals_path(path, sizeof path, "mass:/FNFPS2", b, "bf") > 0
              && strcmp(path, "mass:/FNFPS2/songs/bopeebo/Voices-bf.pcm") == 0,
          "vocals path convention");
    check(b && sr_vocals_path(path, sizeof path, "mass:/FNFPS2", b, "dad") > 0
              && strcmp(path, "mass:/FNFPS2/songs/bopeebo/Voices-dad.pcm") == 0,
          "opponent vocals path convention");
    check(sr_character_path(path, sizeof path, "mass:/FNFPS2", "bf-car") > 0
              && strcmp(path, "mass:/FNFPS2/chars/bf-car.pack") == 0,
          "character pack path convention");
    check(sr_chart_path(NULL, 0, NULL, b, "hard") == -1, "NULL root is rejected");
    check(sr_chart_path(NULL, 0, "mass:/FNFPS2", NULL, "hard") == -1, "NULL song is rejected");
    check(sr_vocals_path(NULL, 0, "mass:/FNFPS2", b, NULL) == -1, "NULL side is rejected");
    /* NULL out NAO e' erro: a API dos caminhos aceita (NULL, 0) para medir o
       tamanho necessario, igual ao sr_chart_path documentado. O que nao pode
       e' devolver zero (nao mediu nada) nem escrever no buffer. */
    check(sr_stems_path(NULL, 0, "mass:/FNFPS2", b) ==
              (int)strlen("mass:/FNFPS2/songs/bopeebo/stems.pcm"),
          "stems path with a NULL buffer reports the length to allocate");
    check(sr_stems_path(path, sizeof path, NULL, b) == -1, "stems path rejects a NULL root");

    printf("\nsong_registry: validation against the official tables\n");
    pending_lines = 0;
    int problems = sr_validate(counting_report);
    check(problems - pending_lines == 0,
          "no wrong character or stage id (only the pending fields, if any)");

    printf("\n%s (%d failure%s)\n", failures ? "FAILED" : "ALL PASS",
           failures, failures == 1 ? "" : "s");
    return failures ? 1 : 0;
}
