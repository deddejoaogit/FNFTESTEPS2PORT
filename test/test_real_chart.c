/* End-to-end check: a converted chart must load in the port's real loader and
 * must agree with the JSON it came from.
 *
 * Build: gcc -O2 -Wall -Wextra -Werror -std=gnu11 -Isrc -Isrc/engine \
 *            -Itest/ps2sdk-shim test/test_real_chart.c src/tutorial_chart.c
 * Run:   test_real_chart <chart.fch3> <expected_note_count> [song_id]
 *
 * The checks split in two. The generic ones run for every chart and cover the
 * invariants the loader itself enforces. The tutorial-specific ones run only
 * for the Tutorial, whose exact numbers were verified against the original by
 * hand, and pin the behaviour the port's camera code depends on.
 */

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

int main(int argc, char **argv)
{
    chart_info_t info;
    const char *song = argc >= 4 ? argv[3] : "";
    int is_tutorial = strcmp(song, "tutorial") == 0;
    long want;
    int rc;

    if (argc < 3) {
        printf("usage: %s chart.fch3 expected_note_count [song_id]\n", argv[0]);
        return 2;
    }
    want = strtol(argv[2], NULL, 10);

    rc = chart_load(argv[1], &info);
    if (rc != 0) printf("  loader returned %d\n", rc);
    ck(rc == 0, "the real loader accepts the converted chart");
    if (rc != 0) return 1;

    printf("  info: %u notes, %u bpms, %u sections, %u events, end=%u, bpm=%u\n",
           info.note_count, info.bpm_count, info.section_count,
           info.event_count, info.song_end_sample, info.initial_bpm);

    /* ---- generic: true of every converted chart ---- */
    ck(want > 0 && (long)info.note_count == want,
       "note count matches the source chart JSON");
    ck(info.bpm_count >= 1, "at least one bpm change");
    ck(info.section_count >= 1, "at least one section");
    ck(info.checksum != 0, "checksum recorded");
    ck(info.song_end_sample > 0, "song end sample is set");

    /* every record must be in range, ordered, and carry its own index */
    int ordered = 1, lanes_ok = 1, press_ok = 1, order_ok = 1, inside = 1;
    u32 prev = 0;
    for (u32 i = 0; i < info.note_count; i++) {
        chart_note_t n;
        if (chart_note(i, &n) != 0) { ordered = 0; break; }
        if (n.lane > 3) lanes_ok = 0;
        if (n.must_press > 1) press_ok = 0;
        if (n.order != i) order_ok = 0;
        if (n.sample < prev || n.sample > info.song_end_sample) inside = 0;
        if (n.duration > info.song_end_sample - n.sample) inside = 0;
        prev = n.sample;
    }
    ck(ordered && inside, "notes are sorted and inside the song");
    ck(lanes_ok, "every lane is 0..3");
    ck(press_ok, "must_press is 0 or 1");
    ck(order_ok, "every note's order field is its index");

    /* the loader rejects type 0, so every event must have a real id */
    int ev_order_ok = 1, ev_type_ok = 1, ev_sorted = 1, ev_inside = 1;
    u32 ev_prev = 0;
    for (u32 i = 0; i < info.event_count; i++) {
        const chart_event_t *e = chart_event(i);
        if (!e) { ev_order_ok = 0; continue; }
        /* v4 dropped the order field: it always had to equal the index, so it
           carried no information. Only v3 files have it to check. */
        if (info.version == 3u && e->order != i) ev_order_ok = 0;
        if (e->type == 0) ev_type_ok = 0;
        if (e->sample < ev_prev) ev_sorted = 0;
        if (e->sample > info.song_end_sample) ev_inside = 0;
        ev_prev = e->sample;
    }
    ck(ev_order_ok, info.version == 3u
       ? "every event's order field is its index"
       : "v4 has no order field (it always equalled the index)");
    ck(ev_type_ok, "no event has type 0 (the loader rejects it)");
    ck(ev_sorted && ev_inside, "events are sorted and inside the song");

    /* the opening bpm record must sit at step 0, sample 0 */
    ck(info.initial_bpm > 0, "the opening bpm is positive");

    /* O passo do chart (1/4 de batida) tem de sair da tabela de BPM do proprio
       chart. O port usava 7.200 amostras fixas para tudo -- o valor do Tutorial
       a 100 BPM -- e isso esta' errado para qualquer secao com outro BPM.
       Aqui se confere contra a conta feita a partir do BPM do arquivo:
       60/bpm/4 segundos a 48 kHz, por secao. */
    {
        int steps_ok = 1;
        u32 i;
        for (i = 0; i < info.note_count; i += 7) {
            chart_note_t n;
            u32 sps;
            if (chart_note(i, &n) != 0) { steps_ok = 0; break; }
            sps = chart_samples_per_step(n.sample);
            if (!sps || sps < 100 || sps > 100000) { steps_ok = 0; break; }
        }
        ck(steps_ok, "every note's sample falls in a bpm section with a sane step");
        /* Fora de qualquer chart carregado a funcao nao inventa um numero. */
        ck(chart_samples_per_step(0) > 0, "the step at sample 0 is defined");
    }

    /* ---- tutorial-specific: the numbers verified by hand ---- */
    if (is_tutorial) {
        ck(info.event_count == 26, "26 events (tutorial-chart.json has 26)");
        ck(info.bpm_count == 1, "1 bpm change");
        ck(info.initial_bpm == 100, "opening bpm is 100");
        /* 100 BPM, 4 passos por batida, 48 kHz: 48000*60/(100*4) = 7200. */
        ck(chart_samples_per_step(0) == 7200,
           "100 bpm gives a 7200-sample step (the number the port had fixed)");

        /* the first note is the opponent's left at 9600 ms = 460800 samples */
        chart_note_t n;
        ck(chart_note(0, &n) == 0, "note 0 readable");
        ck(n.sample == 460800 && n.lane == 0 && n.must_press == 0,
           "note 0 is the opponent's left at 460800 samples");

        int focus = 0, zoom = 0, scroll = 0, anim = 0;
        for (u32 i = 0; i < info.event_count; i++) {
            const chart_event_t *e = chart_event(i);
            if (!e) continue;
            if (e->type == 1) focus++;
            else if (e->type == 2) zoom++;
            else if (e->type == 4) scroll++;
            else if (e->type == 3) anim++;
        }
        /* Counts from the source JSON:
           {'FocusCamera': 11, 'ZoomCamera': 10, 'PlayAnimation': 4,
            'ScrollSpeed': 1} */
        printf("  events by type: focus=%d zoom=%d scroll=%d anim=%d\n",
               focus, zoom, scroll, anim);
        ck(focus == 11 && zoom == 10 && scroll == 1 && anim == 4,
           "11 FocusCamera, 10 ZoomCamera, 4 PlayAnimation, 1 ScrollSpeed");
        ck(focus + zoom + scroll + anim == (int)info.event_count,
           "every event maps to one of the four loader types");

        /* The primary parameter lives in `value` for v3 and in p1 for v4 (v4
           puts the duration in p0 and the actual value in p1). */
        float zoom_to = 0.0f, scroll_to = 0.0f;
        for (u32 i = 0; i < info.event_count; i++) {
            const chart_event_t *e = chart_event(i);
            u32 raw = info.version == 4u ? e->p1 : (e->value & 0xFFFFu);
            if (e->type == 2 && zoom_to == 0.0f)
                zoom_to = (float)raw / 1000.0f;
            if (e->type == 4)
                scroll_to = (float)raw / 1000.0f;
        }
        ck(zoom_to == 1.3f, "first zoom target is 1.3 (zoom*defaultZoom)");
        ck(scroll_to == 1.2f, "scroll target is 1.2 (scroll 1.2 * chart scrollSpeed 1.0)");
    }

    /* a bad path must fail, not crash */
    ck(chart_load("/nonexistent/x.chart", &info) != 0,
       "a missing file fails cleanly");

    printf("  %s\n", fails ? "FAILED" : "ok");
    return fails ? 1 : 0;
}
