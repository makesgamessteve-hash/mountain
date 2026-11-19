/* Mountain engine - WAV loading and a small software mixer (platform independent). MIT License. */
#ifndef MTN_AUDIO_H
#define MTN_AUDIO_H

#include <stddef.h>
#include <stdint.h>

#define MTN_MAX_SOUNDS 64
#define MTN_MAX_VOICES 32

typedef struct {
    char name[32];
    int16_t *samples; /* mono */
    int count;
    int rate;
} mtn_sound;

typedef struct {
    int sound;
    double pos;
    float step;
    float vol, target, fade;  /* fade: volume units per second */
    float pan;                /* -1 left .. 1 right */
    int loop;
    int active;
    int ambient;              /* ambient loops are addressed by sound */
} mtn_voice;

typedef struct {
    mtn_sound sounds[MTN_MAX_SOUNDS];
    int sound_count;
    mtn_voice voices[MTN_MAX_VOICES];
    float master;
    int rate;
} mtn_mixer;

int mtn_wav_parse(const uint8_t *data, size_t size, mtn_sound *out);
void mtn_mixer_init(mtn_mixer *m, int rate);
void mtn_mixer_free(mtn_mixer *m);
int mtn_mixer_add(mtn_mixer *m, const char *name, const mtn_sound *s); /* takes ownership of samples */
int mtn_mixer_find(const mtn_mixer *m, const char *name);
int mtn_mixer_play(mtn_mixer *m, int sound, float vol, float pan, float pitch);
/* Fades an ambient loop towards a volume (starting it if needed, stopping at 0). */
void mtn_mixer_ambient(mtn_mixer *m, int sound, float vol, float fade_seconds);
float mtn_mixer_ambient_volume(const mtn_mixer *m, int sound);
void mtn_mixer_stop_all(mtn_mixer *m);
int mtn_mixer_active_voices(const mtn_mixer *m);
/* interleaved stereo float output */
void mtn_mixer_mix(mtn_mixer *m, float *out, int frames);

#endif
