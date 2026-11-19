/* Mountain engine - WAV loading and a small software mixer (platform independent). MIT License. */
#include "mtn/audio.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint32_t rd32(const uint8_t *p) { return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24); }
static uint16_t rd16(const uint8_t *p) { return (uint16_t)(p[0] | (p[1] << 8)); }

int mtn_wav_parse(const uint8_t *d, size_t size, mtn_sound *out) {
    memset(out, 0, sizeof *out);
    if (size < 12 || memcmp(d, "RIFF", 4) != 0 || memcmp(d + 8, "WAVE", 4) != 0) return 0;
    size_t p = 12;
    int channels = 0, bits = 0, fmt = 0;
    while (p + 8 <= size) {
        uint32_t len = rd32(d + p + 4);
        const uint8_t *c = d + p + 8;
        if (p + 8 + len > size) return 0;
        if (memcmp(d + p, "fmt ", 4) == 0 && len >= 16) {
            fmt = rd16(c);
            channels = rd16(c + 2);
            out->rate = (int)rd32(c + 4);
            bits = rd16(c + 14);
        } else if (memcmp(d + p, "data", 4) == 0) {
            if (fmt != 1 || channels < 1 || (bits != 16 && bits != 8)) return 0;
            int bps = bits / 8;
            int frames = (int)(len / (uint32_t)(bps * channels));
            out->samples = malloc(sizeof(int16_t) * (size_t)(frames > 0 ? frames : 1));
            out->count = frames;
            for (int i = 0; i < frames; i++) {
                int acc = 0;
                for (int ch = 0; ch < channels; ch++) {
                    const uint8_t *s = c + ((size_t)i * (size_t)channels + (size_t)ch) * (size_t)bps;
                    acc += bits == 16 ? (int16_t)rd16(s) : ((int)s[0] - 128) * 256;
                }
                out->samples[i] = (int16_t)(acc / channels);
            }
            return 1;
        }
        p += 8 + len + (len & 1u);
    }
    return 0;
}

void mtn_mixer_init(mtn_mixer *m, int rate) {
    memset(m, 0, sizeof *m);
    m->rate = rate;
    m->master = 0.8f;
}

void mtn_mixer_free(mtn_mixer *m) {
    for (int i = 0; i < m->sound_count; i++) free(m->sounds[i].samples);
    m->sound_count = 0;
    memset(m->voices, 0, sizeof m->voices);
}

int mtn_mixer_add(mtn_mixer *m, const char *name, const mtn_sound *s) {
    int existing = mtn_mixer_find(m, name);
    if (existing >= 0) {
        free(m->sounds[existing].samples);
        m->sounds[existing] = *s;
        snprintf(m->sounds[existing].name, sizeof m->sounds[existing].name, "%s", name);
        return existing;
    }
    if (m->sound_count >= MTN_MAX_SOUNDS) {
        free(s->samples);
        return -1;
    }
    m->sounds[m->sound_count] = *s;
    snprintf(m->sounds[m->sound_count].name, sizeof m->sounds[0].name, "%s", name);
    return m->sound_count++;
}

int mtn_mixer_find(const mtn_mixer *m, const char *name) {
    for (int i = 0; i < m->sound_count; i++)
        if (strcmp(m->sounds[i].name, name) == 0) return i;
    return -1;
}

static int free_voice(mtn_mixer *m) {
    for (int i = 0; i < MTN_MAX_VOICES; i++)
        if (!m->voices[i].active) return i;
    /* steal the quietest one-shot */
    int best = -1;
    for (int i = 0; i < MTN_MAX_VOICES; i++)
        if (!m->voices[i].ambient && (best < 0 || m->voices[i].vol < m->voices[best].vol)) best = i;
    return best;
}

int mtn_mixer_play(mtn_mixer *m, int sound, float vol, float pan, float pitch) {
    if (sound < 0 || sound >= m->sound_count) return -1;
    int v = free_voice(m);
    if (v < 0) return -1;
    mtn_voice *vc = &m->voices[v];
    memset(vc, 0, sizeof *vc);
    vc->sound = sound;
    vc->vol = vc->target = vol;
    vc->pan = pan;
    vc->step = (float)m->sounds[sound].rate / (float)m->rate * (pitch > 0 ? pitch : 1.0f);
    vc->active = 1;
    return v;
}

void mtn_mixer_ambient(mtn_mixer *m, int sound, float vol, float fade_seconds) {
    if (sound < 0 || sound >= m->sound_count) return;
    float fade = fade_seconds > 0.0f ? 1.0f / fade_seconds : 1000.0f;
    for (int i = 0; i < MTN_MAX_VOICES; i++) {
        mtn_voice *v = &m->voices[i];
        if (v->active && v->ambient && v->sound == sound) {
            v->target = vol;
            v->fade = fade;
            return;
        }
    }
    if (vol <= 0.0f) return;
    int vi = -1;
    for (int i = 0; i < MTN_MAX_VOICES && vi < 0; i++)
        if (!m->voices[i].active) vi = i;
    if (vi < 0) return;
    mtn_voice *v = &m->voices[vi];
    memset(v, 0, sizeof *v);
    v->sound = sound;
    v->vol = 0.0f;
    v->target = vol;
    v->fade = fade;
    v->loop = 1;
    v->ambient = 1;
    v->active = 1;
    v->step = (float)m->sounds[sound].rate / (float)m->rate;
}

float mtn_mixer_ambient_volume(const mtn_mixer *m, int sound) {
    for (int i = 0; i < MTN_MAX_VOICES; i++)
        if (m->voices[i].active && m->voices[i].ambient && m->voices[i].sound == sound) return m->voices[i].vol;
    return 0.0f;
}

void mtn_mixer_stop_all(mtn_mixer *m) { memset(m->voices, 0, sizeof m->voices); }

int mtn_mixer_active_voices(const mtn_mixer *m) {
    int n = 0;
    for (int i = 0; i < MTN_MAX_VOICES; i++) n += m->voices[i].active;
    return n;
}

void mtn_mixer_mix(mtn_mixer *m, float *out, int frames) {
    memset(out, 0, sizeof(float) * 2 * (size_t)frames);
    float dt = 1.0f / (float)m->rate;
    for (int i = 0; i < MTN_MAX_VOICES; i++) {
        mtn_voice *v = &m->voices[i];
        if (!v->active) continue;
        const mtn_sound *s = &m->sounds[v->sound];
        float lg = sqrtf(0.5f * (1.0f - v->pan)), rg = sqrtf(0.5f * (1.0f + v->pan));
        for (int f = 0; f < frames; f++) {
            if (v->vol != v->target) {
                float d = v->fade * dt;
                if (v->vol < v->target) v->vol = v->vol + d > v->target ? v->target : v->vol + d;
                else v->vol = v->vol - d < v->target ? v->target : v->vol - d;
            }
            int idx = (int)v->pos;
            if (idx >= s->count) {
                if (v->loop && s->count > 0) {
                    v->pos = fmod(v->pos, (double)s->count); /* step may exceed the loop length */
                    idx = (int)v->pos;
                } else {
                    v->active = 0;
                    break;
                }
            }
            float frac = (float)(v->pos - (double)idx);
            int next = idx + 1 < s->count ? idx + 1 : (v->loop ? 0 : idx);
            float smp = ((float)s->samples[idx] * (1.0f - frac) + (float)s->samples[next] * frac) / 32768.0f;
            smp *= v->vol * m->master;
            out[f * 2] += smp * lg;
            out[f * 2 + 1] += smp * rg;
            v->pos += v->step;
        }
        if (v->ambient && v->target <= 0.0f && v->vol <= 0.0f) v->active = 0;
    }
    for (int f = 0; f < frames * 2; f++) out[f] = tanhf(out[f]); /* soft clip */
}
