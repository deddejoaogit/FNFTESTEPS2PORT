#include "chart_events.h"

#include <math.h>

#include "chart.h"

/*
 * The state this module owns. Everything here is driven by chart events, and
 * every default is the original's.
 */
static chart_event_state_t st;
static u32 samples_per_step = 7200u;   /* the tutorial's, until told otherwise */
static const chart_info_t *bound = 0;

/* The pending PlayAnimation queue. Two can land on one sample (the tutorial
   fires 'hey' and 'cheer' together), so a single slot would drop one. */
static u32 anim_ref_q[CHART_ANIM_QUEUE];
static int anim_target_q[CHART_ANIM_QUEUE];
static int anim_force_q[CHART_ANIM_QUEUE];
static int anim_head = 0, anim_tail = 0;

static void anim_push(u32 ref, int target, int force)
{
    int next = (anim_tail + 1) % CHART_ANIM_QUEUE;
    if (next == anim_head) return;      /* full: refuse rather than overwrite */
    anim_ref_q[anim_tail] = ref;
    anim_target_q[anim_tail] = target;
    anim_force_q[anim_tail] = force;
    anim_tail = next;
}

/* The 13 ease names the charts actually use, in the order the converter
   assigns indices. tools/fnf_to_fch3.py writes the index into the event's `b`
   word; this table and chart_ease_eval() are what turn it back into a curve.
   The names were measured across all 27 charts, not guessed -- see
   docs/FCH4_FORMAT.md. */
const char *const chart_ease_names[13] = {
    "CLASSIC", "INSTANT", "circ", "elastic", "elasticInOut", "expo",
    "expoIn", "expoOut", "linear", "quad", "quadInOut", "quart", "quintInOut"
};

/* ------------------------------------------------------------------ easing */

static float ease_linear(float t) { return t; }

static float ease_circ(float t, int dir)
{
    if (t <= 0.0f) return 0.0f;
    if (t >= 1.0f) return 1.0f;
    if (dir == 0) return -(sqrtf(1.0f - t * t) - 1.0f);            /* circIn */
    if (dir == 1) return sqrtf(1.0f - (t - 1.0f) * (t - 1.0f));    /* circOut */
    if (t < 0.5f) return -0.5f * (sqrtf(1.0f - t * t * 4.0f) - 1.0f);
    return 0.5f * (sqrtf(1.0f - (t * 2.0f - 2.0f) * (t * 2.0f - 2.0f)) + 1.0f);
}

static float ease_quad(float t, int dir)
{
    if (dir == 0) return t * t;                                   /* quadIn */
    if (dir == 1) return t * (2.0f - t);                          /* quadOut */
    if (t < 0.5f) return t * t * 2.0f;
    return -1.0f + (4.0f - 2.0f * t) * t;
}

static float ease_quart(float t, int dir)
{
    if (dir == 0) return t * t * t * t;                           /* quartIn */
    if (dir == 1) {
        float u = 1.0f - t;
        return 1.0f - u * u * u * u;
    }
    if (t < 0.5f) return 8.0f * t * t * t * t;
    return 1.0f - 8.0f * (1.0f - t) * (1.0f - t) * (1.0f - t) * (1.0f - t);
}

static float ease_quint_in_out(float t)
{
    if (t < 0.5f) return 16.0f * t * t * t * t * t;
    return 1.0f - powf(-2.0f * t + 2.0f, 5.0f) * 0.5f;
}

static float ease_expo(float t, int dir)
{
    if (dir == 0) return (t <= 0.0f) ? 0.0f : powf(2.0f, 10.0f * (t - 1.0f));
    if (dir == 1) return (t >= 1.0f) ? 1.0f : 1.0f - powf(2.0f, -10.0f * t);
    if (t <= 0.0f) return 0.0f;
    if (t >= 1.0f) return 1.0f;
    if (t < 0.5f) return 0.5f * powf(2.0f, 10.0f * (2.0f * t - 1.0f));
    return 1.0f - 0.5f * powf(2.0f, -10.0f * (2.0f * t - 1.0f));
}

/*
 * elasticInOut, byte for byte the curve main.c has always used for the
 * tutorial's zoom (fnf_elastic_in_out), which is what makes the working build
 * match. The original builds a function name from ease + easeDir and looks it
 * up, so 'elastic' + 'InOut' and 'elasticInOut' are the same lookup -- which is
 * why the charts use both spellings for one curve.
 *
 * elasticIn and elasticOut are never used: no chart in the game carries
 * 'elastic' with easeDir In or Out. Rather than invent them, both fall back to
 * the InOut curve. If a chart ever needs them, this is where they go.
 */
static float ease_elastic_in_out(float t)
{
    const float TAU_045 = 13.962634016f;   /* 2*pi/0.45 */
    float u;
    if (t <= 0.0f) return 0.0f;
    if (t >= 1.0f) return 1.0f;
    u = t * 2.0f - 1.0f;
    if (t < 0.5f) {
        return -0.5f * (powf(2.0f, 10.0f * u) * sinf((u - 0.1125f) * TAU_045));
    }
    return powf(2.0f, -10.0f * u) * sinf((u - 0.1125f) * TAU_045) * 0.5f + 1.0f;
}

/*
 * Apply the ease named by `index`, in direction `dir` (0 In, 1 Out, 2 InOut).
 *
 * The original builds a function name like 'expoIn' and looks it up in FlxEase,
 * so the names that carry no direction -- linear, elasticInOut, quintInOut --
 * ignore dir, and CLASSIC and INSTANT are not curves at all: CLASSIC just sets
 * the follow point with no tween, and INSTANT forces the duration to 0. Both
 * land on the target immediately, which is what returning 1.0 does here.
 */
float chart_ease_eval(u32 index, u32 dir, float t)
{
    if (t <= 0.0f) return 0.0f;
    if (t >= 1.0f) return 1.0f;
    switch (index) {
    case 0:  return 1.0f;                        /* CLASSIC */
    case 1:  return 1.0f;                        /* INSTANT */
    case 2:  return ease_circ(t, (int)dir);
    case 3:  return ease_elastic_in_out(t);       /* elastic, any dir */
    case 4:  return ease_elastic_in_out(t);       /* elasticInOut */
    case 5:  return ease_expo(t, (int)dir);
    case 6:  return ease_expo(t, 0);             /* expoIn */
    case 7:  return ease_expo(t, 1);             /* expoOut */
    case 8:  return ease_linear(t);
    case 9:  return ease_quad(t, (int)dir);
    case 10: return ease_quad(t, 2);             /* quadInOut */
    case 11: return ease_quart(t, (int)dir);
    case 12: return ease_quint_in_out(t);
    default: return t;                           /* unknown index: linear */
    }
}

/* ------------------------------------------------------------------- state */

void chart_events_reset(float default_zoom, float default_scroll)
{
    st.cam_char = CHART_FOCUS_BF;
    st.cam_x = 0.0f;
    st.cam_y = 0.0f;
    st.cam_mode = CHART_FOCUS_MODE_CLASSIC;
    st.cam_ease = CHART_EASE_LINEAR;
    st.cam_ease_dir = 0u;
    st.cam_dur_s = 0.0f;
    st.cam_seq = 0u;

    st.zoom = default_zoom;
    st.zoom_from = default_zoom;
    st.zoom_to = default_zoom;
    st.zoom_t = 1.0f;
    st.zoom_ease = 0u;
    st.zoom_ease_dir = 0u;
    st.zoom_dur_s = 0.0f;

    st.scroll = default_scroll;
    st.scroll_from = default_scroll;
    st.scroll_to = default_scroll;
    st.scroll_t = 1.0f;
    st.scroll_ease = 0u;
    st.scroll_ease_dir = 0u;
    st.scroll_dur_s = 0.0f;

    st.bop_intensity = CHART_DEFAULT_BOP_INTENSITY;
    st.bop_rate = CHART_DEFAULT_BOP_RATE;
    st.bop_offset = CHART_DEFAULT_BOP_OFFSET;
    st.bop = 1.0f;
    st.bop_step = 0xFFFFFFFFu;

    st.icon_char = 0;
    st.icon_flipx = 0;
    st.icon_ref = 0xFFFFFFFFu;

    st.anim_ref = 0xFFFFFFFFu;
    st.anim_target = CHART_ANIM_BF;
    st.anim_force = 0;
    st.anim_pending = 0;
    st.anim_queued = 0;
    anim_head = anim_tail = 0;

    st.seen = 0;
    st.last_type = 0;
    st.last_sample = 0;
}

/*
 * Map an ease name to the index chart_ease_eval takes. The names are the ones
 * the charts actually use, measured across all 27 -- not a guess at what
 * FlxEase might offer.
 *
 * The ease arrives as a STRING-TABLE index, not as one of these. The converter
 * writes `b`'s ease field as an index into the file's own string table
 * (tools/fnf_to_fch3.py:452 says so in as many words), so a fixed list would
 * silently point at the wrong curve -- which is the bug the FCH4 round trip
 * already caught once. Resolving by name is what keeps it from coming back.
 */
u32 chart_events_ease_id(const char *name)
{
    u32 i;
    if (!name) return CHART_EASE_LINEAR;
    for (i = 0; i < 13u; i++) {
        const char *a = name;
        const char *b = chart_ease_names[i];
        while (*a && *a == *b) { a++; b++; }
        if (*a == *b) return i;      /* both ended together */
    }
    return CHART_EASE_LINEAR;        /* unknown name: linear, the original's default */
}

void chart_events_init(u32 sps)
{
    samples_per_step = sps ? sps : 7200u;
}

void chart_events_bind(const chart_info_t *info)
{
    bound = info;
}

void chart_events_set_bop(float intensity, float rate, float offset)
{
    st.bop_intensity = intensity;
    st.bop_rate = rate;
    st.bop_offset = offset;
}

/* Nomes de ease comparados sem depender de <string.h>: o modulo e' C puro e nao
   inclui cabecalho de plataforma. */
static int str_eq(const char *a, const char *b)
{
    if (!a || !b) return 0;
    while (*a && *a == *b) { a++; b++; }
    return *a == *b;
}

/* Fixed-point x1000 back to a float. The converter stores it this way so a
   value survives the round trip exactly, which is what test_fch4.py proves.
   O CAST PARA s32 NAO E' DECORATIVO: FocusCamera carrega x/y NEGATIVOS
   (`{"char":-1,"x":-440,"y":150}`), e o conversor grava o complemento de dois.
   Sem o cast, -440 virava 4294527 e a camera ia para fora do palco. Zoom,
   scroll e intensidade sao positivos e nao sentem a diferenca. */
static float fx(u32 v) { return (float)(s32)v / 1000.0f; }

/* The original's durations are in steps and it converts with stepLengthMs.
   The port works in samples, so a step is samples_per_step of them. */
static float steps_to_seconds(float steps)
{
    return steps * ((float)samples_per_step / 48000.0f);
}

/*
 * Apply one event. This is the whole of the module's knowledge of what an event
 * means; chart_events_pump() below is only a convenience loop over a bound
 * chart. A caller that already has its own event loop -- main.c does, because
 * it owns the cursor and the telemetry -- applies events one at a time and
 * keeps its own bookkeeping.
 */
void chart_events_apply(const chart_event_t *e)
{
    {
        u32 ease_dir = (e->b >> 16) & 3u;
        st.last_type = e->type;
        st.last_sample = e->sample;
        st.seen++;

        switch (e->type) {
        case CHART_EV_FOCUS_CAMERA: {
            /* char is the primary small integer. v3 packed it into `value`,
               and a bare int in the chart means the same thing --
               FocusCameraSongEvent does `if (char == null) char = cast
               data.value`, so the 408 events written as
               {"e":"FocusCamera","v":1} are char 1, not a position.
               x/y are an OFFSET from that character's focus point (the
               ABSOLUTE point when char is -1), and how the camera gets there
               depends on the ease: FocusCameraSongEvent.hx L138-160. */
            const char *en = (e->b & CHART_B_EASE_ABSENT)
                             ? 0 : chart_event_string(e->b & CHART_B_EASE_MASK);
            float dur = fx(e->p0);
            st.cam_char = (int)e->a;
            st.cam_x = fx(e->p1);
            st.cam_y = fx(e->p2);
            st.cam_ease_dir = ease_dir;
            if (!en) {
                /* No `ease` in the chart: the class default is 'CLASSIC'
                   (FocusCameraSongEvent L64), not index 0 of the string table,
                   which is just the first name the converter interned. */
                st.cam_mode = CHART_FOCUS_MODE_CLASSIC;
                st.cam_ease = CHART_EASE_LINEAR;
            } else if (str_eq(en, "CLASSIC")) {
                st.cam_mode = CHART_FOCUS_MODE_CLASSIC;
                st.cam_ease = CHART_EASE_LINEAR;
            } else if (str_eq(en, "INSTANT")) {
                st.cam_mode = CHART_FOCUS_MODE_INSTANT;
                st.cam_ease = CHART_EASE_LINEAR;
            } else {
                st.cam_mode = CHART_FOCUS_MODE_TWEEN;
                st.cam_ease = chart_events_ease_id(en);
            }
            if (dur <= 0.0f) dur = CHART_FOCUS_DEFAULT_DURATION;
            st.cam_dur_s = steps_to_seconds(dur);
            st.cam_seq++;
            break;
        }

        case CHART_EV_ZOOM_CAMERA: {
            float dur = fx(e->p0);
            float zoom = fx(e->p1);
            /* mode 'stage' (a == 1) multiplies the stage's own zoom rather than
               replacing it, so 1.0 means "the stage's zoom". The port has one
               stage, so it resolves against the zoom the song started with. */
            if (e->a == 1u) zoom = st.zoom_from * zoom;
            st.zoom_from = st.zoom;
            st.zoom_to = zoom;
            st.zoom_t = (dur > 0.0f) ? 0.0f : 1.0f;
            /* No `ease` in the chart: SongEvent.DEFAULT_EASE is 'linear' and
               DEFAULT_EASE_DIR 'In'. Before the absent bit existed, an event
               without `ease` read index 0 of the string table, which is
               whichever name the converter interned first -- a real curve that
               nobody asked for. */
            st.zoom_ease = (e->b & CHART_B_EASE_ABSENT)
                           ? CHART_EASE_LINEAR
                           : chart_events_ease_id(chart_event_string(e->b & CHART_B_EASE_MASK));
            st.zoom_ease_dir = ease_dir;
            st.zoom_dur_s = steps_to_seconds(dur);
            break;
        }

        case CHART_EV_SCROLL_SPEED: {
            float dur = fx(e->p0);
            float scroll = fx(e->p1);
            /* absolute false means scroll * the chart's own scrollSpeed. The
               chart's value is what chart_events_reset was given. */
            if (!(e->b & CHART_B_ABSOLUTE)) scroll = st.scroll_from * scroll;
            st.scroll_from = st.scroll;
            st.scroll_to = scroll;
            st.scroll_t = (dur > 0.0f) ? 0.0f : 1.0f;
            st.scroll_ease = (e->b & CHART_B_EASE_ABSENT)
                             ? CHART_EASE_LINEAR
                             : chart_events_ease_id(chart_event_string(e->b & CHART_B_EASE_MASK));
            st.scroll_ease_dir = ease_dir;
            st.scroll_dur_s = steps_to_seconds(dur);
            break;
        }

        case CHART_EV_SET_CAMERA_BOP:
            /* SetCameraBopSongEvent.hx:
                 cameraBopIntensity   = (DEFAULT_BOP_INTENSITY - 1) * intensity + 1
                 cameraZoomRate       = rate      (beats)
                 cameraZoomRateOffset = offset    (beats)
               with intensity defaulting to 1.0, which lands back on the
               constant. So an empty {} resets the bop, exactly as that class's
               own doc comment says. */
            chart_events_set_bop((CHART_DEFAULT_BOP_INTENSITY - 1.0f) * fx(e->p0) + 1.0f,
                                 fx(e->p2), fx(e->p1));
            break;

        case CHART_EV_SET_HEALTH_ICON:
            st.icon_char = (int)e->a;
            st.icon_flipx = (e->b & CHART_B_FLIPX) ? 1 : 0;
            st.icon_ref = e->ref;
            break;

        case CHART_EV_PLAY_ANIMATION:
            /* An action, not a state: every one of these is a discrete thing
               the original does, so each is queued and none overwrites another. */
            anim_push(e->ref, (int)e->a, (e->b & CHART_B_FORCE) ? 1 : 0);
            st.anim_ref = e->ref;
            st.anim_target = (int)e->a;
            st.anim_force = (e->b & CHART_B_FORCE) ? 1 : 0;
            st.anim_pending = (anim_head != anim_tail);
            break;

        default:
            /* ChangeCharacter, AltIdleAnimation and the scripted sserafim*
               events have no port-side state yet. They are counted and
               reported rather than dropped silently, so telemetry shows them. */
            break;
        }
    }
}

u32 chart_events_pump(u32 sample)
{
    u32 n = 0;
    u32 count = bound ? bound->event_count : 0u;

    while (n < count) {
        const chart_event_t *e = chart_event(n);
        if (!e || e->sample > sample) break;
        chart_events_apply(e);
        n++;
    }
    return n;
}

void chart_events_tick(float dt, u32 step)
{
    if (st.zoom_t < 1.0f && st.zoom_dur_s > 0.0f) {
        st.zoom_t += dt / st.zoom_dur_s;
        if (st.zoom_t > 1.0f) st.zoom_t = 1.0f;
        st.zoom = st.zoom_from + (st.zoom_to - st.zoom_from) *
                  chart_ease_eval(st.zoom_ease, st.zoom_ease_dir, st.zoom_t);
    }
    if (st.scroll_t < 1.0f && st.scroll_dur_s > 0.0f) {
        st.scroll_t += dt / st.scroll_dur_s;
        if (st.scroll_t > 1.0f) st.scroll_t = 1.0f;
        st.scroll = st.scroll_from + (st.scroll_to - st.scroll_from) *
                    chart_ease_eval(st.scroll_ease, st.scroll_ease_dir, st.scroll_t);
    }

    /* The camera bop. PlayState.hx:1846-1859 sets the multiplier on the step
       that satisfies (step + offset*4) % (rate*4) == 0, and :1217-1227 decays
       it every frame with lerp(1, bop, 0.95^(dt*60)). The port's TUT_DT gives
       dt*60 = 1.0010 and 0.95^1.0010 = 0.949951, which is what main.c used. */
    {
        u32 period = (u32)(st.bop_rate * (float)CHART_STEPS_PER_BEAT);
        u32 offset = (u32)(st.bop_offset * (float)CHART_STEPS_PER_BEAT);
        if (period > 0u && step != st.bop_step &&
            ((step + offset) % period) == 0u) {
            st.bop_step = step;
            st.bop = st.bop_intensity;
        }
    }
    st.bop = 1.0f + (st.bop - 1.0f) * powf(CHART_BOP_DECAY, dt * 60.0f);
}

const chart_event_state_t *chart_events_state(void) { return &st; }

int chart_events_take_anim(u32 *anim_ref, int *target, int *force)
{
    if (anim_head == anim_tail) {
        st.anim_pending = 0;
        st.anim_queued = 0;
        return 0;
    }
    if (anim_ref) *anim_ref = anim_ref_q[anim_head];
    if (target) *target = anim_target_q[anim_head];
    if (force) *force = anim_force_q[anim_head];
    anim_head = (anim_head + 1) % CHART_ANIM_QUEUE;
    st.anim_pending = (anim_head != anim_tail);
    st.anim_queued = (anim_tail - anim_head + CHART_ANIM_QUEUE) % CHART_ANIM_QUEUE;
    return 1;
}
