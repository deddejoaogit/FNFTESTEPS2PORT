#ifndef FNF_TELEMETRY_H
#define FNF_TELEMETRY_H
#include <tamtypes.h>
int telemetry_init(const char *build_id, const char *elf_path);
void telemetry_event(const char *name, int a, int b);
void telemetry_frame(u32 frame, u64 begin, u64 after_draw, u64 after_vsync,
                     unsigned short pressed, int animation, int animation_frame,
                     int asset_frame, u32 textured_quads, int qwords, int state,
                     int transitions, u32 audio_heard, u32 audio_queued,
                     u32 ring_level, u32 underruns);
int telemetry_finish(void);
#endif