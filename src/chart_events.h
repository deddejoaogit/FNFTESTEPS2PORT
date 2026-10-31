#ifndef FNF_CHART_EVENTS_H
#define FNF_CHART_EVENTS_H

/*
 * Chart event dispatch, on the FCH4 event records.
 *
 * The port used to pump events inline in main.c, reading `event->value` and
 * handling three of the eight ids. That was the v3 shape: one packed u32 per
 * event, primary parameter in the low 16 bits and duration in the high 16. It
 * dropped every other parameter of every event -- 445 events across the 27
 * shipped charts had parameters v3 silently threw away, which is what
 * docs/FCH4_FORMAT.md records and what FCH4 exists to fix.
 *
 * This module reads the v4 fields instead: p0/p1/p2 (fixed-point x1000), a
 * (the primary small integer), b (ease index, easeDir and booleans packed) and
 * ref (an index into the string or block table). It owns the state those
 * events drive, so main.c only has to advance it and read the results.
 *
 * The loader is chart_load() and friends; this module reads what it returns.
 * The event ids and field meanings are the converter's, and they are shared.
 *
 * Every default below is the original's, taken from the event classes in
 * source/funkin/play/event/ and from Constants.hx. Nothing here is inferred.
 */

#include "tamtypes.h"
#include "chart.h"

/* Event ids. Must match tools/fnf_to_fch3.py and main.c's telemetry. */
#define CHART_EV_FOCUS_CAMERA     1u
#define CHART_EV_ZOOM_CAMERA      2u
#define CHART_EV_PLAY_ANIMATION   3u
#define CHART_EV_SCROLL_SPEED     4u
#define CHART_EV_CHANGE_CHARACTER 5u
#define CHART_EV_ALT_IDLE_ANIM    6u
#define CHART_EV_SET_CAMERA_BOP   7u
#define CHART_EV_SET_HEALTH_ICON  8u

/* `b` bit set when the chart carried NO `ease` field at all. The ease field in
   `b` is an index into the file's own string table, and index 0 is a real name,
   so "absent" cannot be told from "CLASSIC" by the value -- and absent means the
   CLASS DEFAULT (FocusCamera: 'CLASSIC', ZoomCamera: 'linear'), not whichever
   string the converter interned first. */
#define CHART_B_EASE_ABSENT (1u << 23)

/* How a FocusCamera event moves the camera (FocusCameraSongEvent.hx L138-160):
     CLASSIC -> resetCamera(false,false,false) + setPosition: no tween, the
                follow point moves and the camera's own lock-on takes it there;
     INSTANT -> tweenCameraToPosition(x,y,0): the camera lands at once;
     other   -> FlxTween over stepLengthMs*duration, with the named ease. */
#define CHART_FOCUS_MODE_CLASSIC 0
#define CHART_FOCUS_MODE_INSTANT 1
#define CHART_FOCUS_MODE_TWEEN   2
#define CHART_FOCUS_DEFAULT_DURATION 4.0f   /* FocusCameraSongEvent L63 */

/* FocusCamera char values, FocusCameraSongEvent.hx. */
#define CHART_FOCUS_STATIC (-1)
#define CHART_FOCUS_BF       0
#define CHART_FOCUS_DAD      1
#define CHART_FOCUS_GF       2

/* PlayAnimation target values. tools/fnf_to_fch3.py maps both 'bf' and
   'boyfriend' to 0 -- they are aliases of one character -- so these two are
   the same number on purpose. */
#define CHART_ANIM_BF         0
#define CHART_ANIM_BOYFRIEND  0
#define CHART_ANIM_DAD        2
#define CHART_ANIM_GF         3

/* Ease ids, matching chart_ease_names. */
#define CHART_EASE_LINEAR 8u

/* The original's defaults, Constants.hx and the event classes. */
#define CHART_DEFAULT_BOP_INTENSITY 1.015f   /* Constants.DEFAULT_BOP_INTENSITY */
#define CHART_DEFAULT_BOP_RATE      4.0f     /* Constants.DEFAULT_ZOOM_RATE, in beats */
#define CHART_DEFAULT_BOP_OFFSET    0.0f     /* Constants.DEFAULT_ZOOM_OFFSET, in beats */
#define CHART_STEPS_PER_BEAT        4        /* Constants.STEPS_PER_BEAT */
#define CHART_BOP_DECAY             0.95f    /* PlayState.hx:1217 */

typedef struct {
    /* Camera focus, FocusCameraSongEvent.hx. char -1 focuses on (x,y) itself,
       0/1/2 add (x,y) as an offset to that character's focus point. */
    int   cam_char;
    float cam_x, cam_y;
    int   cam_mode;          /* CHART_FOCUS_MODE_*                             */
    u32   cam_ease, cam_ease_dir;
    float cam_dur_s;         /* duration in SECONDS, steps -> stepLengthMs     */
    u32   cam_seq;           /* one per applied FocusCamera event              */

    /* ZoomCamera. The tween runs on zoom_t from 0 to 1 over the event's own
       duration and ease, both of which travel with the event. */
    float zoom, zoom_from, zoom_to, zoom_t;
    u32   zoom_ease, zoom_ease_dir;
    float zoom_dur_s;

    /* ScrollSpeed. Same shape. `absolute` false means the value multiplies the
       chart's own scrollSpeed, so the default the caller passed matters. */
    float scroll, scroll_from, scroll_to, scroll_t;
    u32   scroll_ease, scroll_ease_dir;
    float scroll_dur_s;

    /* Camera bop, SetCameraBopSongEvent.hx. intensity replaces the hardcoded
       1.015; rate and offset replace the hardcoded every-16-steps. */
    float bop_intensity;
    float bop_rate;      /* in beats */
    float bop_offset;    /* in beats */
    float bop;           /* the current multiplier, decays toward 1 */
    u32   bop_step;      /* the step the last bop fired on, 0xFFFFFFFF = none */

    /* SetHealthIcon. icon_ref is a string-table index for the icon id, or
       0xFFFFFFFF when the event carried none. */
    int  icon_char;
    int  icon_flipx;
    u32  icon_ref;

    /* PlayAnimation. anim_ref is a string-table index for the animation name;
       force is the event's force flag, which the original also uses to decide
       whether the character plays vocals. These mirror the head of the queue
       for convenience; the queue is what a caller drains. */
    u32 anim_ref;
    int anim_target;
    int anim_force;
    int anim_pending;    /* set on arrival, cleared by chart_events_take_anim() */
    int anim_queued;     /* how many are waiting behind the head */

    /* Bookkeeping, for telemetry and the existing status captures. */
    u32 seen, last_type, last_sample;
} chart_event_state_t;

/*
 * Reset to the state a song starts in. `default_zoom` is the stage's camera
 * zoom (mainStage's 1.1), which is what ZoomCamera tweens away from, and
 * `default_scroll` is the chart's own scrollSpeed.
 */
void chart_events_reset(float default_zoom, float default_scroll);

/*
 * Tell the module how long a step is, in 48 kHz samples. The original converts
 * an event's duration from steps to seconds with stepLengthMs, and the bop
 * condition is expressed in steps, so both need this.
 */
void chart_events_init(u32 samples_per_step);

/*
 * Point the module at a loaded chart. The loader keeps its info struct private,
 * so the caller hands over the one it filled. Call this after every load,
 * before pumping: it is what tells pump how many events there are.
 */
void chart_events_bind(const chart_info_t *info);

/*
 * Set the camera bop directly. This is what the SetCameraBop event handler
 * calls, and what a caller uses to reset the bop to the original's defaults.
 */
void chart_events_set_bop(float intensity, float rate, float offset);

/*
 * Apply one event. This is all of the module's knowledge of what an event
 * means. A caller that already walks the chart itself applies events one at a
 * time and keeps its own cursor and telemetry.
 */
void chart_events_apply(const chart_event_t *event);

/*
 * Convenience: consume every event of the bound chart at or before `sample`, in
 * order. Returns the number consumed. A caller with its own event loop does not
 * need this.
 */
u32 chart_events_pump(u32 sample);

/*
 * Advance the tweens and the bop decay. `dt` is seconds since the last frame
 * and `step` is the current step of the song, which the bop condition needs.
 * Must be called once per frame, after pump.
 */
void chart_events_tick(float dt, u32 step);

const chart_event_state_t *chart_events_state(void);

/*
 * Read and clear the pending PlayAnimation, so a caller acts on it exactly
 * once. Returns 0 when there is nothing pending.
 *
 * This is a queue, not a slot, because two PlayAnimation events can land on the
 * same sample: the tutorial fires 'hey' on boyfriend and 'cheer' on dad
 * together at 18,600 ms, and philly-nice does the same. The original runs both;
 * a single slot would silently drop the first. Measured across all 27 charts
 * the most that ever pile up on one sample is 2, so a queue of 4 is generous,
 * and a chart that somehow exceeded it would be rejected rather than truncated.
 */
#define CHART_ANIM_QUEUE 4
int chart_events_take_anim(u32 *anim_ref, int *target, int *force);

/* The 13 ease names the charts use, as indices into this table. Keep in step
   with tools/fnf_to_fch3.py, which writes the index, not the name. */
extern const char *const chart_ease_names[13];

/* The ease named by `index`, in direction `dir` (0 In, 1 Out, 2 InOut).
   CLASSIC and INSTANT are not curves -- they land on the target immediately,
   which is what returning 1.0 does. */
float chart_ease_eval(u32 index, u32 dir, float t);

/*
 * Resolve an ease NAME to the id chart_ease_eval takes. The ease in an event's
 * `b` word is a string-table index, so the caller looks the name up first and
 * passes it here. An unknown or absent name is linear, which is the original's
 * own default (SongEvent.hx:32).
 */
u32 chart_events_ease_id(const char *name);

#endif
