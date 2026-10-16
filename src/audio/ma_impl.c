/* miniaudio (public domain / MIT-0) implementation unit for the Living World client. Built as plain C, apart from the engine's compiler flags. */
#define MA_NO_ENCODING
#define MA_NO_GENERATION
#define MA_NO_FLAC
#define MA_NO_RESOURCE_MANAGER
#define MA_NO_NODE_GRAPH_EXTRA
#define MINIAUDIO_IMPLEMENTATION
#include "miniaudio.h"
#include "um_ma.h"
#include <stdlib.h>
#include <string.h>

struct UmaSound { unsigned char *data; ma_decoder dec; ma_sound snd; int loop; int ready; };
static ma_engine gEngine;
static int gOk = 0;
static char gName[256] = "none";

int uma_init(void)
{
	ma_engine_config cfg;
	if (gOk) return 1;
	cfg = ma_engine_config_init();
	cfg.channels = 2;
	cfg.sampleRate = 44100;
	if (ma_engine_init(&cfg, &gEngine) != MA_SUCCESS) return 0;
	{
		ma_device *dev = ma_engine_get_device(&gEngine);
		if (dev) ma_device_get_name(dev, ma_device_type_playback, gName, sizeof gName, NULL);
	}
	gOk = 1;
	return 1;
}
void uma_shutdown(void) { if (gOk) { ma_engine_uninit(&gEngine); gOk = 0; } }
const char *uma_device_name(void) { return gName; }

UmaSound *uma_create(const void *data, unsigned size, int loop)
{
	UmaSound *s; ma_decoder_config dc;
	if (!gOk || !data || size < 16) return NULL;
	s = (UmaSound *)calloc(1, sizeof *s);
	if (!s) return NULL;
	s->data = (unsigned char *)malloc(size);
	if (!s->data) { free(s); return NULL; }
	memcpy(s->data, data, size);
	dc = ma_decoder_config_init(ma_format_f32, 2, 44100);
	if (ma_decoder_init_memory(s->data, size, &dc, &s->dec) != MA_SUCCESS) { free(s->data); free(s); return NULL; }
	if (ma_sound_init_from_data_source(&gEngine, &s->dec, MA_SOUND_FLAG_NO_SPATIALIZATION, NULL, &s->snd) != MA_SUCCESS) { ma_decoder_uninit(&s->dec); free(s->data); free(s); return NULL; }
	s->loop = loop; s->ready = 1;
	if (loop) ma_sound_set_looping(&s->snd, MA_TRUE);
	return s;
}
void uma_start(UmaSound *s) { if (s && s->ready) ma_sound_start(&s->snd); }
void uma_stop(UmaSound *s) { if (s && s->ready) ma_sound_stop(&s->snd); }
void uma_pause(UmaSound *s, int paused) { if (!s || !s->ready) return; if (paused) ma_sound_stop(&s->snd); else ma_sound_start(&s->snd); }
void uma_restart(UmaSound *s) { if (!s || !s->ready) return; ma_sound_seek_to_pcm_frame(&s->snd, 0); ma_sound_start(&s->snd); }
void uma_destroy(UmaSound *s)
{
	if (!s) return;
	if (s->ready) { ma_sound_uninit(&s->snd); ma_decoder_uninit(&s->dec); }
	free(s->data); free(s);
}
void uma_set_volume(UmaSound *s, float v) { if (s && s->ready) ma_sound_set_volume(&s->snd, v < 0 ? 0 : v); }
void uma_set_pan(UmaSound *s, float p) { if (s && s->ready) ma_sound_set_pan(&s->snd, p); }
void uma_set_pitch(UmaSound *s, float p) { if (s && s->ready && p > 0.05f) ma_sound_set_pitch(&s->snd, p); }
int uma_finished(UmaSound *s) { return s && s->ready && !s->loop && ma_sound_at_end(&s->snd); }
void uma_seek_ms(UmaSound *s, unsigned ms)
{
	ma_uint32 rate = 0;
	if (!s || !s->ready) return;
	if (ma_sound_get_data_format(&s->snd, NULL, NULL, &rate, NULL, 0) != MA_SUCCESS || rate == 0) return;
	ma_sound_seek_to_pcm_frame(&s->snd, (ma_uint64)ms * rate / 1000);
}
unsigned uma_cursor_ms(UmaSound *s)
{
	float c = 0;
	if (!s || !s->ready) return 0;
	if (ma_sound_get_cursor_in_seconds(&s->snd, &c) != MA_SUCCESS) return 0;
	return (unsigned)(c * 1000.0f);
}
float uma_length_ms(const void *data, unsigned size)
{
	ma_decoder d; ma_decoder_config dc; ma_uint64 frames = 0; float ms = 0;
	if (!data || size < 16) return 0;
	dc = ma_decoder_config_init(ma_format_f32, 2, 44100);
	if (ma_decoder_init_memory(data, size, &dc, &d) != MA_SUCCESS) return 0;
	if (ma_decoder_get_length_in_pcm_frames(&d, &frames) == MA_SUCCESS && frames) ms = (float)frames * 1000.0f / 44100.0f;
	ma_decoder_uninit(&d);
	return ms;
}
