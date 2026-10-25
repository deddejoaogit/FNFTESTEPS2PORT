#ifndef FNF_CHART_H
#define FNF_CHART_H

#include <tamtypes.h>

/* chart_load_song() takes a registry entry, so this file includes the
   registry. That is the one dependency the chart layer has, and it is a cheap
   one: the registry is generated data plus path helpers, with no console or
   filesystem dependency of its own. Keeping it here means a caller that has a
   song does not have to assemble the path itself, which is how the port ended
   up hardcoding "mass:/FNFPS2/assets/test.chart" in the first place. */
#include "song_registry.h"

typedef struct {
    u32 note_count;
    u32 bpm_count;
    u32 section_count;
    u32 event_count;
    u32 song_end_sample;
    u32 checksum;
    u32 initial_bpm;
    u32 version;         /* 3 or 4 */
    u32 string_count;    /* v4 only, 0 for v3 */
    u32 block_count;     /* v4 only, 0 for v3 */
} chart_info_t;

/* One chart event, carrying both formats' fields.
 *
 * FCH3 packed everything into `value` (a fixed-point primary parameter in the
 * low 16 bits, a duration in the high 16) and validated an `order` field that
 * always had to equal the record's index. FCH4 gives each event its own
 * parameters, because the real charts carry up to six of them and 445 events
 * in the 27 shipped charts had parameters v3 silently dropped. See
 * docs/FCH4_FORMAT.md.
 *
 * Both sets of fields are present so one struct serves both files: v3 fills
 * `value`/`order` and leaves the rest zero, v4 does the reverse.
 */
typedef struct {
    u32 sample;
    u32 type;
    u32 p0, p1, p2;      /* v4: fixed-point x1000 numeric parameters */
    u32 a;               /* v4: primary small integer (char/mode/target/strumline) */
    u32 b;               /* v4: ease index, easeDir and booleans, packed */
    u32 ref;             /* v4: string or block index, 0xFFFFFFFF when unused */
    u32 value;           /* v3: the packed u32 */
    u32 order;           /* v3 only; v4 drops it because it always equalled the index */
} chart_event_t;

/* v4 flag bits in an event's `b` word. Keep in step with the converter. */
#define CHART_B_EASE_MASK   0x0000FFFFu
#define CHART_B_EASEDIR_IN  0u
#define CHART_B_EASEDIR_OUT 1u
#define CHART_B_EASEDIR_INOUT 2u
#define CHART_B_FORCE       (1u<<18)
#define CHART_B_FLIPX       (1u<<19)
#define CHART_B_ABSOLUTE    (1u<<20)
#define CHART_B_REF_IS_STRING (1u<<21)
#define CHART_B_REF_IS_BLOCK  (1u<<22)

/* Capacity of the v4 string and block tables. The real charts need 16 strings
   and 177 bytes at most (spaghetti-easy), so these are generous, and the
   loader rejects a file that would overflow them rather than truncating. */
#define CHART_MAX_STRINGS 64u
#define CHART_MAX_STRING_BYTES 1024u
#define CHART_MAX_BLOCK_BYTES 2048u

/* v4's string and block tables. Both return 0 for a v3 file, which has no
   tables, and for an out-of-range index -- a caller that treats 0 as "absent"
   needs no version check. */
const char *chart_event_string(u32 index);
const unsigned char *chart_event_block(u32 index,u32 *length);

/* Capacity of the loader's static arrays. These are a contract, not an
 * implementation detail: a chart that exceeds any of them is rejected with
 * -5, which is what happened to 2hot-hard (1.035 notes) until 0.2.105 raised
 * the note limit from 1.024 to 1.280. test/test_chart_limits.c asserts these
 * against every converted chart, so lowering one fails loudly instead of
 * silently dropping a song. Keep the note-state array in main.c in step with
 * CHART_MAX_NOTES. */
#define CHART_MAX_NOTES 1280u
#define CHART_MAX_BPMS 16u
#define CHART_MAX_SECTIONS 512u
#define CHART_MAX_EVENTS 512u

typedef struct {
    u32 sample;
    u32 duration;
    u8 lane;
    u8 must_press;
    u16 order;
} chart_note_t;

int chart_load(const char *path, chart_info_t *info);

/*
 * Load a song's chart by its registry entry, resolving the path the way the
 * game lays its assets out: "<root>/songs/<id>/<difficulty>.chart".
 *
 * difficulty is "easy", "normal" or "hard". The port's asset root is whatever
 * mass:/ path the build installs to; the registry does not know it, which is
 * why it is a parameter.
 *
 * Returns chart_load()'s result, or -1 when an argument is missing and -2 when
 * the path would not fit the buffer.
 */
int chart_load_song(const char *root, const sr_song_t *song,
                    const char *difficulty, chart_info_t *info);

/* Load a song's chart trying every pendrive layout sr_chart_path_at() knows.
 * The first layout that opens wins. On success *layout_out (may be NULL)
 * receives the layout index that worked; on total failure it receives -1 and
 * the return value is chart_load()'s code for layout 0, the original game's
 * tree -- so a log line still says why the chart did not load. */
int chart_load_song_layout(const char *root, const sr_song_t *song,
                           const char *difficulty, chart_info_t *info,
                           int *layout_out);
int chart_note(u32 index, chart_note_t *out);
const chart_event_t *chart_event(u32 index);
u32 chart_step_at(u32 sample);

#endif
