#ifndef AUDIO_RING_H
#define AUDIO_RING_H
int audio_ring_start(int fd);
int audio_ring_start_loop(int fd, int loop);
int audio_ring_take(void *dst, int bytes);
int audio_ring_level(void);
int audio_ring_level_snapshot(void);
void audio_ring_stats(unsigned int *produced, unsigned int *reads, unsigned int *loops, unsigned int *errors, unsigned int *underruns);
void audio_ring_stop(void);
void audio_ring_pause(int p);
int audio_ring_eof(void);
int audio_ring_reset(void);
int audio_ring_switch(int fd, int loop);
void audio_ring_debug(int *fd, unsigned int *loop, unsigned int *switches, unsigned int *reset_pending, unsigned int *switch_pending);
#endif
