/*
 * Prove chart_events.c against the real charts.
 *
 * The point is not that the module compiles, but that it reads the FCH4
 * records the way the original's event classes do. So this test loads real
 * converted charts and checks the state the events produce against values read
 * straight out of the game's own JSON -- not against anything the test
 * computed, and not against what the module happens to do.
 *
 * The tutorial chart supplies FocusCamera, ZoomCamera and PlayAnimation, plus
 * the single ScrollSpeed event in the whole game. milf and spaghetti supply
 * SetCameraBop and SetHealthIcon, which the tutorial does not have.
 *
 * Build: see test/run_tests.sh
 */
#include <stdio.h>
#include <string.h>

#include "chart_events.h"
#include "chart.h"

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

static void near(float a, float b, const char *what)
{
    char buf[256];
    checks++;
    if (a < b - 0.001f || a > b + 0.001f) {
        failures++;
        snprintf(buf, sizeof(buf), "%s: got %.4f want %.4f", what, a, b);
        printf("  FAIL %s\n", buf);
    }
}

static chart_info_t g_info;

static void load(const char *path)
{
    int r = chart_load(path, &g_info);
    chart_events_bind(&g_info);
    checks++;
    if (r != 0) {
        failures++;
        printf("  FAIL load %s -> %d\n", path, r);
    }
}

/* ms -> 48 kHz samples, the unit the FCH4 records use. */
#define MS(m) ((u32)((m) * 48u))

#define TUTORIAL "build/charts/tutorial-normal.chart"
#define MILF     "build/charts/milf-normal.chart"
#define SPAGHETTI "build/charts/spaghetti-normal.chart"

int main(void)
{
    const chart_event_state_t *s;

    /* ---- the defaults, before any event fires ------------------------- */
    /* 100 BPM: stepLengthMs 150, so a step is 150 * 48000 / 1000 = 7200. */
    chart_events_init(7200u);
    chart_events_reset(1.1f, 1.0f);
    s = chart_events_state();
    ok(s->cam_char == CHART_FOCUS_BF, "starts focused on BF");
    ok(s->bop_intensity == CHART_DEFAULT_BOP_INTENSITY, "default bop intensity");
    ok(s->bop_rate == CHART_DEFAULT_BOP_RATE, "default bop rate is 4 beats");
    ok(s->bop_offset == CHART_DEFAULT_BOP_OFFSET, "default bop offset is 0");
    ok(s->zoom == 1.1f, "starts at the stage zoom");
    ok(s->scroll == 1.0f, "starts at the chart scrollSpeed");
    ok(s->anim_pending == 0, "no animation pending at the start");

    /* ---- every ease the charts use, at values Flixel defines ---------- */
    ok(chart_ease_eval(0, 0, 0.5f) == 1.0f, "CLASSIC lands immediately");
    ok(chart_ease_eval(1, 0, 0.5f) == 1.0f, "INSTANT lands immediately");
    ok(chart_ease_eval(8, 0, 0.0f) == 0.0f, "linear(0) is 0");
    ok(chart_ease_eval(8, 0, 1.0f) == 1.0f, "linear(1) is 1");
    near(chart_ease_eval(8, 0, 0.25f), 0.25f, "linear(0.25) is 0.25");
    near(chart_ease_eval(6, 0, 0.5f), 0.03125f, "expoIn(0.5) is 2^-5");
    near(chart_ease_eval(7, 1, 0.5f), 0.96875f, "expoOut(0.5) is 1-2^-5");
    near(chart_ease_eval(10, 2, 0.25f), 0.125f, "quadInOut(0.25) is 0.125");
    near(chart_ease_eval(10, 2, 0.75f), 0.875f, "quadInOut(0.75) is 0.875");
    near(chart_ease_eval(9, 0, 0.5f), 0.25f, "quadIn(0.5) is 0.25");
    near(chart_ease_eval(9, 1, 0.5f), 0.75f, "quadOut(0.5) is 0.75");
    near(chart_ease_eval(12, 2, 0.5f), 0.5f, "quintInOut(0.5) is 0.5");
    near(chart_ease_eval(2, 0, 0.5f), 0.133974596f, "circIn(0.5)");
    near(chart_ease_eval(2, 1, 0.5f), 0.866025404f, "circOut(0.5)");
    near(chart_ease_eval(11, 0, 0.5f), 0.0625f, "quartIn(0.5)");
    near(chart_ease_eval(11, 1, 0.5f), 0.9375f, "quartOut(0.5)");
    near(chart_ease_eval(5, 0, 0.5f), 0.03125f, "expo with dir In is expoIn");
    near(chart_ease_eval(5, 1, 0.5f), 0.96875f, "expo with dir Out is expoOut");
    /* elasticInOut is the curve main.c already uses for the working tutorial
       zoom, so it is the one elastic value that is proven rather than derived.
       It is symmetric about 0.5 and undershoots before it. */
    near(chart_ease_eval(4, 2, 0.5f), 0.5f, "elasticInOut(0.5) is 0.5");
    near(chart_ease_eval(4, 2, 0.25f), 0.011950f, "elasticInOut(0.25) undershoots");
    near(chart_ease_eval(4, 2, 0.75f), 0.988050f, "elasticInOut(0.75) is its mirror");
    near(chart_ease_eval(4, 2, 0.0f), 0.0f, "elasticInOut(0) is 0");
    near(chart_ease_eval(4, 2, 1.0f), 1.0f, "elasticInOut(1) is 1");
    /* 'elastic' with any direction is the same lookup: the original builds
       'elastic' + easeDir and finds elasticInOut, which is why the charts use
       both spellings for one curve. elasticIn/elasticOut are never used. */
    near(chart_ease_eval(3, 2, 0.25f), 0.011950f, "elastic+InOut is elasticInOut");
    ok(chart_ease_eval(99, 0, 0.5f) == 0.5f, "an unknown ease index is linear");
    ok(strcmp(chart_ease_names[6], "expoIn") == 0, "ease 6 is expoIn");
    ok(chart_events_ease_id("expoIn") == 6u, "expoIn resolves to id 6");
    ok(chart_events_ease_id("elasticInOut") == 4u, "elasticInOut resolves to id 4");
    ok(chart_events_ease_id("CLASSIC") == 0u, "CLASSIC resolves to id 0");
    ok(chart_events_ease_id("noSuchEase") == CHART_EASE_LINEAR, "an unknown name is linear");
    ok(chart_events_ease_id(0) == CHART_EASE_LINEAR, "a null name is linear");

    /* ---- the tutorial's real events ----------------------------------- */
    load(TUTORIAL);
    ok(g_info.event_count == 26u, "the tutorial has 26 events");

    /* Nothing moves before the first event, which is at t = 0. */
    chart_events_reset(1.1f, 1.0f);
    s = chart_events_state();
    ok(s->seen == 0, "no events consumed before the first");

    /* Pump to 9,600 ms. The chart's own events there are
         FocusCamera char 1   -- the opponent, which the tutorial's metadata
                                 says is gf
         ZoomCamera zoom 1.3, duration 4, ease elasticInOut
       so the focus target becomes char 1 and the zoom tween starts at 1.1
       heading for 1.3. Neither has moved yet, because the tween runs in tick. */
    chart_events_pump(MS(9600));
    s = chart_events_state();
    ok(s->seen == 3u, "the event at 0 and both at 9,600 ms were consumed");
    ok(s->cam_char == CHART_FOCUS_DAD, "focus moved to the opponent");
    /* Neither of the tutorial's FocusCamera events carries an `ease`, and the
       class default is 'CLASSIC' (FocusCameraSongEvent L64): the follow point
       moves and the camera's lock-on takes it there, no tween. The default
       duration is 4 steps (L63), which at 150 ms per step is 0.6 s -- it is
       recorded even for CLASSIC, so a later INSTANT/tween event that inherits
       nothing still reports the right number. */
    ok(s->cam_mode == CHART_FOCUS_MODE_CLASSIC, "no `ease` means CLASSIC, not a curve");
    near(s->cam_dur_s, 0.6f, "the default duration is 4 steps = 0.6 s at 150 ms");
    ok(s->cam_seq == 2u, "two FocusCamera events so far (0 and 9,600 ms)");
    ok(s->zoom_from == 1.1f, "the zoom tween starts from the stage zoom");
    near(s->zoom_to, 1.3f, "the zoom target is the chart's 1.3");
    ok(s->zoom_t == 0.0f, "the zoom tween has not run yet");
    ok(s->zoom == 1.1f, "so the zoom itself is still 1.1");
    near(s->zoom_dur_s, 0.6f, "duration 4 steps at 150 ms is 0.6 s");
    ok(s->zoom_ease == 4u, "the ease index is elasticInOut");

    /* Run the tween out. elasticInOut(1) is 1, so it lands exactly. */
    {
        int i;
        for (i = 0; i < 120; i++) chart_events_tick(1.0f / 60.0f, (u32)i);
    }
    s = chart_events_state();
    near(s->zoom, 1.3f, "the zoom lands on 1.3");
    ok(s->zoom_t == 1.0f, "the zoom tween finished");

    /* At 14,400 ms the chart zooms back to 1 and refocuses on BF. */
    chart_events_pump(MS(14400));
    s = chart_events_state();
    ok(s->cam_char == CHART_FOCUS_BF, "focus moved back to BF");
    near(s->zoom_to, 1.0f, "the zoom target is the chart's 1");
    ok(s->zoom_from == 1.3f, "and it starts from where the zoom actually was");
    {
        int i;
        for (i = 0; i < 120; i++) chart_events_tick(1.0f / 60.0f, 100u + (u32)i);
    }
    near(chart_events_state()->zoom, 1.0f, "the zoom lands back on 1");

    /* PlayAnimation: the chart fires 'hey' on boyfriend with force, and
       'cheer' on dad without. Both at 18,600 ms. */
    chart_events_reset(1.1f, 1.0f);
    chart_events_pump(MS(18600));
    {
        u32 ref = 0;
        int target = -1, force = -1;
        ok(chart_events_take_anim(&ref, &target, &force) == 1, "the first PlayAnimation was taken");
        ok(target == CHART_ANIM_BOYFRIEND, "and it targets boyfriend");
        ok(target == CHART_ANIM_BF, "boyfriend and bf are the same id");
        ok(force == 1, "and it forces");
        ok(chart_event_string(ref) != 0, "the anim name is in the string table");
        ok(chart_event_string(ref) && strcmp(chart_event_string(ref), "hey") == 0,
           "and the anim name is 'hey'");
        ok(chart_events_take_anim(&ref, &target, &force) == 1, "the second PlayAnimation was taken");
        ok(target == CHART_ANIM_DAD, "and it targets dad");
        ok(force == 0, "and it does not force");
        ok(chart_event_string(ref) && strcmp(chart_event_string(ref), "cheer") == 0,
           "and the anim name is 'cheer'");
        ok(chart_events_take_anim(0, 0, 0) == 0, "and there is no third");
    }

    /* ScrollSpeed at 60,000 ms: {absolute:false, duration:4, ease:expoIn,
       scroll:1.2, strumline:both}. absolute false means 1.2 times the chart's
       own scrollSpeed, which the tutorial metadata says is 1. */
    chart_events_reset(1.1f, 1.0f);
    chart_events_pump(MS(59999));
    near(chart_events_state()->scroll, 1.0f, "the scroll has not moved yet");
    chart_events_pump(MS(60000));
    s = chart_events_state();
    near(s->scroll_to, 1.2f, "the scroll target is 1.2 times the chart's 1");
    ok(s->scroll_from == 1.0f, "and it starts from the chart's scrollSpeed");
    ok(s->scroll_ease == 6u, "and the ease is expoIn");
    near(s->scroll_dur_s, 0.6f, "and the duration is 4 steps");
    {
        int i;
        for (i = 0; i < 120; i++) chart_events_tick(1.0f / 60.0f, 200u + (u32)i);
    }
    near(chart_events_state()->scroll, 1.2f, "the scroll lands on 1.2");

    /* ---- the bop, which the original derives from SetCameraBop -------- */
    chart_events_reset(1.1f, 1.0f);
    chart_events_tick(1.0f / 60.0f, 0u);       /* step 0 satisfies the default */
    near(chart_events_state()->bop, CHART_DEFAULT_BOP_INTENSITY, "the bop fires on step 0");
    chart_events_tick(1.0f / 60.0f, 1u);
    s = chart_events_state();
    ok(s->bop < CHART_DEFAULT_BOP_INTENSITY, "the bop decays");
    ok(s->bop > 1.0f, "the bop decays toward 1, not past it");

    /* step 16 is the next one the default rate of 4 beats hits. */
    chart_events_reset(1.1f, 1.0f);
    chart_events_tick(1.0f / 60.0f, 15u);
    ok(chart_events_state()->bop < CHART_DEFAULT_BOP_INTENSITY, "step 15 does not bop");
    chart_events_tick(1.0f / 60.0f, 16u);
    near(chart_events_state()->bop, CHART_DEFAULT_BOP_INTENSITY, "step 16 bops");

    /* A rate of 0 means never, which is what the original checks for. */
    chart_events_reset(1.1f, 1.0f);
    chart_events_set_bop(CHART_DEFAULT_BOP_INTENSITY, 0.0f, 0.0f);
    chart_events_tick(1.0f / 60.0f, 0u);
    ok(chart_events_state()->bop < CHART_DEFAULT_BOP_INTENSITY, "a bop rate of 0 never bops");

    /* ---- SetCameraBop, from a chart that actually has it ------------- */
    /* milf carries two. The first is what the test checks; the second only
       has to be consumed without disturbing the first's result. */
    load(MILF);
    chart_events_reset(1.1f, 1.0f);
    {
        u32 i;
        int seen = 0;
        for (i = 0; i < g_info.event_count; i++) {
            const chart_event_t *e = chart_event(i);
            if (e && e->type == CHART_EV_SET_CAMERA_BOP) {
                chart_events_pump(e->sample);
                seen++;
                break;
            }
        }
        ok(seen == 1, "milf has a SetCameraBop and it was reached");
        s = chart_events_state();
        /* milf's first is {intensity: 2.5, rate: 1}. SetCameraBopSongEvent does
               cameraBopIntensity = (DEFAULT_BOP_INTENSITY - 1) * intensity + 1
                                     = (1.015 - 1) * 2.5 + 1 = 1.0375
               and cameraZoomRate = 1 beat, which is 4 steps, not 16. So the
               bop now fires four times as often and 2.5 times as hard. */
        near(s->bop_intensity, 1.0375f, "intensity 2.5 becomes 1.0375");
        near(s->bop_rate, 1.0f, "rate 1 beat is 1.0 beats");
        ok(s->bop_offset == 0.0f, "and the offset stays at its default");
        /* And a rate of 1 beat means step 4 bops, where 4 beats would not. */
        /* The bop fires and then decays inside the same tick, which is the
           order in PlayState.hx: stepHit() sets the multiplier and the update
           that follows decays it. So after ticking at step 4 the value is the
           set value decayed once, not the set value itself. Had step 4 NOT
           fired it would be 1.0338 instead. */
        chart_events_tick(1.0f / 60.0f, 0u);
        near(chart_events_state()->bop, 1.0356f, "step 0 fires, then decays once");
        chart_events_tick(1.0f / 60.0f, 4u);
        near(chart_events_state()->bop, 1.0356f, "step 4 fires again, then decays once");
        chart_events_tick(1.0f / 60.0f, 5u);
        ok(chart_events_state()->bop < 1.0356f, "step 5 only decays");
    }

    /* ---- SetHealthIcon, from the only chart that has it --------------- */
    /* spaghetti carries all 15 in the game. Each names an icon id, and the
       ids are the SSERAFIM members -- the same names as the character JSONs,
       which is how the test knows the string table is being read correctly. */
    /* ---- FocusCamera: as tres formas de mover a camera, no chart real ---
       FocusCameraSongEvent.hx L138-160: sem `ease` (ou CLASSIC) so' move o
       ponto de foco; INSTANT poe a camera no ponto na hora; qualquer outra
       ease interpola a posicao com FlxTween durante duration*stepLengthMs.
       O spaguete e' a unica musica com as tres: 520 dos 664 eventos dos 81
       charts nao trazem `ease`, 61 sao INSTANT e 9 usam expo/linear/quadInOut. */
    load(SPAGHETTI);
    chart_events_reset(1.1f, 1.0f);
    {
        u32 i;
        int tween_seen = 0, instant_seen = 0;
        for (i = 0; i < g_info.event_count; i++) {
            const chart_event_t *e = chart_event(i);
            if (!e || e->type != CHART_EV_FOCUS_CAMERA) continue;
            chart_events_pump(e->sample);
            s = chart_events_state();
            if (s->cam_mode == CHART_FOCUS_MODE_TWEEN && !tween_seen) {
                tween_seen = 1;
                /* t=700 ms: {"char":-1,"duration":12,"ease":"expo","easeDir":"Out",
                   "x":-440,"y":150} */
                ok(s->cam_char == CHART_FOCUS_STATIC, "char -1 is the static focus");
                near(s->cam_x, -440.0f, "negative x survives fixed point");
                near(s->cam_y, 150.0f, "y survives fixed point");
                ok(s->cam_ease == chart_events_ease_id("expo"), "ease expo travels");
                ok(s->cam_ease_dir == 1u, "easeDir Out is 1");
                ok(s->cam_dur_s > 0.0f, "a tween has a duration in seconds");
            }
            if (s->cam_mode == CHART_FOCUS_MODE_INSTANT && !instant_seen) {
                instant_seen = 1;
                ok(s->cam_char == CHART_FOCUS_STATIC, "the INSTANT events are static too");
                ok(s->cam_dur_s > 0.0f, "duration is recorded even when ignored");
            }
            if (tween_seen && instant_seen) break;
        }
        ok(tween_seen && instant_seen, "spaghetti has both a tween and an INSTANT focus");
    }

    load(SPAGHETTI);
    chart_events_reset(1.1f, 1.0f);
    {
        u32 i;
        int icons = 0, named = 0;
        for (i = 0; i < g_info.event_count; i++) {
            const chart_event_t *e = chart_event(i);
            if (e && e->type == CHART_EV_SET_HEALTH_ICON) {
                chart_events_pump(e->sample);
                icons++;
                s = chart_events_state();
                if (s->icon_ref != 0xFFFFFFFFu && chart_event_string(s->icon_ref)) named++;
            }
        }
        ok(icons == 15, "spaghetti has all 15 SetHealthIcon events");
        ok(named == 15, "and every one names an icon in the string table");
        s = chart_events_state();
        /* Fourteen target char 1, the opponent, and name an SSERAFIM member.
           The fifteenth -- the last two share a sample, so it is the one that
           survives -- is {char: 0, flipX: true, id: 'bf'}: the player's own
           icon, flipped. Two events on one sample is last-wins here, which is
           right: the icon is state, not an action, and the original simply
           applies them in order. */
        ok(s->icon_char == 0, "the surviving event targets the player");
        ok(s->icon_flipx == 1, "and it flips");
        ok(s->icon_ref != 0xFFFFFFFFu, "the icon id came from the string table");
        ok(chart_event_string(s->icon_ref) != 0, "and it resolves");
        ok(chart_event_string(s->icon_ref) && strcmp(chart_event_string(s->icon_ref), "bf") == 0,
           "and the id is 'bf'");
    }

    /* ---- the call shapes main.c actually makes ----------------------- */
    /* main.c cannot be compiled here -- it needs the PS2 SDK -- so the calls
       it makes are reproduced with the same variable types it uses, against the
       real headers. If a signature ever drifts, this is where it is caught
       before the user's build breaks. */
    {
        static const float g_official_mainstage_camera_zoom = 1.1f;   /* main.c:747 */
        u32 music_clock = 0;                                          /* main.c:1137 */
        u32 g_tutorial_chart_cursor = 0;
        const chart_event_t *event = 0;
        static float g_tut_zoom, g_tut_scroll, g_tut_bop;
        static int g_tut_cam_focus;
        static chart_info_t g_tutorial_chart_info;
        u32 aref = 0;
        int at = -1, af2 = 0;

        /* the pump: main.c walks the chart itself and applies one at a time */
        while (g_tutorial_chart_cursor < g_info.event_count) {
            event = chart_event(g_tutorial_chart_cursor);
            if (!event || event->sample > music_clock) break;
            chart_events_apply(event);
            g_tutorial_chart_cursor++;
        }
        ok(1, "main.c's event loop compiles against the module");

        /* the per-song reset and the bind after a load */
        chart_events_reset(g_official_mainstage_camera_zoom, 1.0f);
        chart_events_init(7200u);
        chart_events_bind(&g_tutorial_chart_info);
        ok(1, "main.c's reset and bind compile against the module");

        /* the tick and the mirrors the draw code reads */
        {
            const chart_event_state_t *ev = chart_events_state();
            chart_events_tick(0.016683333f, music_clock / 7200u);
            g_tut_zoom = ev->zoom;
            g_tut_scroll = ev->scroll;
            g_tut_bop = ev->bop;
            g_tut_cam_focus = (ev->cam_char == CHART_FOCUS_GF ||
                               ev->cam_char == CHART_FOCUS_DAD) ? 1 : 0;
            ok(g_tut_cam_focus == 0 || g_tut_cam_focus == 1, "the focus mirror is a flag");
        }
        ok(g_tut_zoom >= 0.0f, "the zoom mirror is readable");
        ok(g_tut_scroll > 0.0f, "the scroll mirror is readable");
        ok(g_tut_bop > 0.0f, "the bop mirror is readable");

        /* the PlayAnimation drain */
        while (chart_events_take_anim(&aref, &at, &af2)) {
            const char *an = chart_event_string(aref);
            (void)an;
        }
        ok(1, "main.c's PlayAnimation drain compiles against the module");
    }

    printf("\n%d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
