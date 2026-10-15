// Thin C wrapper around miniaudio for the Living World client (audio is played from memory buffers read out of the game's .big archives).
#ifndef UM_MA_H
#define UM_MA_H
#ifdef __cplusplus
extern "C" {
#endif
typedef struct UmaSound UmaSound;
int  uma_init(void);                                   /* opens the default playback device; 1 on success */
void uma_shutdown(void);
const char *uma_device_name(void);
UmaSound *uma_create(const void *data, unsigned size, int loop);   /* decodes WAV/MP3 from a copy of the buffer; NULL if it cannot be decoded */
void uma_start(UmaSound *s);
void uma_stop(UmaSound *s);
void uma_pause(UmaSound *s, int paused);
void uma_restart(UmaSound *s);                         /* seek to the start and play again */
void uma_destroy(UmaSound *s);
void uma_set_volume(UmaSound *s, float v);             /* 0..1+ */
void uma_set_pan(UmaSound *s, float p);                /* -1 left .. 1 right */
void uma_set_pitch(UmaSound *s, float p);
int  uma_finished(UmaSound *s);                        /* played to its end (never true for looping sounds) */
unsigned uma_cursor_ms(UmaSound *s);                   /* playback position, for diagnostics */
float uma_length_ms(const void *data, unsigned size);  /* duration of a file, 0 if unknown */
#ifdef __cplusplus
}
#endif
#endif
