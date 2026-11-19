#include "test.h"
#include "mtn/audio.h"

#include <stdlib.h>

/* ---------------------------------------------------------------- WAV builder */

typedef struct {
    uint8_t d[1024];
    size_t n;
} wbuf;

static void w_bytes(wbuf *b, const void *p, size_t n) {
    memcpy(b->d + b->n, p, n);
    b->n += n;
}
static void w_u16(wbuf *b, uint32_t v) {
    uint8_t x[2] = {(uint8_t)(v & 255u), (uint8_t)((v >> 8) & 255u)};
    w_bytes(b, x, 2);
}
static void w_u32(wbuf *b, uint32_t v) {
    uint8_t x[4] = {(uint8_t)(v & 255u), (uint8_t)((v >> 8) & 255u), (uint8_t)((v >> 16) & 255u), (uint8_t)(v >> 24)};
    w_bytes(b, x, 4);
}
static void w_header(wbuf *b) {
    b->n = 0;
    w_bytes(b, "RIFF", 4);
    w_u32(b, 0); /* the parser does not need the RIFF size */
    w_bytes(b, "WAVE", 4);
}
/* chunk with the RIFF pad byte after odd sizes */
static void w_chunk(wbuf *b, const char *id, const void *data, uint32_t len) {
    w_bytes(b, id, 4);
    w_u32(b, len);
    if (len) w_bytes(b, data, len);
    if (len & 1u) {
        uint8_t pad = 0xEE;
        w_bytes(b, &pad, 1);
    }
}
static void w_fmt(wbuf *b, int fmt, int channels, int rate, int bits) {
    w_bytes(b, "fmt ", 4);
    w_u32(b, 16);
    w_u16(b, (uint32_t)fmt);
    w_u16(b, (uint32_t)channels);
    w_u32(b, (uint32_t)rate);
    w_u32(b, (uint32_t)(rate * channels * bits / 8));
    w_u16(b, (uint32_t)(channels * bits / 8));
    w_u16(b, (uint32_t)bits);
}
static void w_data16(wbuf *b, const int16_t *s, int n) {
    uint8_t raw[256];
    for (int i = 0; i < n; i++) {
        raw[i * 2] = (uint8_t)((uint16_t)s[i] & 255u);
        raw[i * 2 + 1] = (uint8_t)((uint16_t)s[i] >> 8);
    }
    w_chunk(b, "data", raw, (uint32_t)(n * 2));
}

static void test_wav_16bit_mono(void) {
    wbuf b;
    mtn_sound s;
    const int16_t src[5] = {0, 1000, -1000, 32767, -32768};
    w_header(&b);
    w_fmt(&b, 1, 1, 22050, 16);
    w_data16(&b, src, 5);
    CHECK(mtn_wav_parse(b.d, b.n, &s));
    CHECK(s.rate == 22050);
    CHECK(s.count == 5);
    for (int i = 0; i < 5 && s.samples; i++) CHECK(s.samples[i] == src[i]);
    free(s.samples);
}

static void test_wav_8bit_and_stereo(void) {
    wbuf b;
    mtn_sound s;
    /* 8-bit mono: unsigned, centred on 128 */
    const uint8_t u8[4] = {128, 255, 0, 192};
    w_header(&b);
    w_fmt(&b, 1, 1, 8000, 8);
    w_chunk(&b, "data", u8, 4);
    CHECK(mtn_wav_parse(b.d, b.n, &s));
    CHECK(s.count == 4 && s.rate == 8000);
    if (s.samples) {
        CHECK(s.samples[0] == 0);
        CHECK(s.samples[1] == 127 * 256);
        CHECK(s.samples[2] == -32768);
        CHECK(s.samples[3] == 64 * 256);
    }
    free(s.samples);

    /* 16-bit stereo is mixed down to mono by averaging */
    const int16_t st[6] = {1000, 3000, -200, -400, 32767, 32767};
    w_header(&b);
    w_fmt(&b, 1, 2, 44100, 16);
    w_data16(&b, st, 6);
    CHECK(mtn_wav_parse(b.d, b.n, &s));
    CHECK(s.count == 3 && s.rate == 44100);
    if (s.samples) {
        CHECK(s.samples[0] == 2000);
        CHECK(s.samples[1] == -300);
        CHECK(s.samples[2] == 32767);
    }
    free(s.samples);

    /* 8-bit stereo, odd data size gets a pad byte (3 bytes -> 1 frame) */
    const uint8_t u8s[3] = {138, 118, 77};
    w_header(&b);
    w_fmt(&b, 1, 2, 11025, 8);
    w_chunk(&b, "data", u8s, 3);
    CHECK(mtn_wav_parse(b.d, b.n, &s));
    CHECK(s.count == 1);
    if (s.samples) CHECK(s.samples[0] == 0);
    free(s.samples);
}

static void test_wav_chunks(void) {
    wbuf b;
    mtn_sound s;
    const int16_t src[2] = {-5, 5};
    /* odd sized LIST chunk before fmt and another chunk between fmt and data */
    w_header(&b);
    w_chunk(&b, "LIST", "abc", 3);
    w_fmt(&b, 1, 1, 16000, 16);
    w_chunk(&b, "fact", "\4\0\0\0", 4);
    w_data16(&b, src, 2);
    CHECK(mtn_wav_parse(b.d, b.n, &s));
    CHECK(s.count == 2 && s.rate == 16000);
    if (s.samples) CHECK(s.samples[0] == -5 && s.samples[1] == 5);
    free(s.samples);

    /* empty data chunk is valid: no frames but a buffer */
    w_header(&b);
    w_fmt(&b, 1, 1, 16000, 16);
    w_chunk(&b, "data", NULL, 0);
    CHECK(mtn_wav_parse(b.d, b.n, &s));
    CHECK(s.count == 0);
    CHECK(s.samples != NULL);
    free(s.samples);
}

static void test_wav_malformed(void) {
    wbuf b;
    mtn_sound s;
    const int16_t src[2] = {1, 2};
    /* too short */
    CHECK(!mtn_wav_parse((const uint8_t *)"RIFF\0\0\0\0WAV", 11, &s));
    CHECK(s.samples == NULL && s.count == 0);
    /* wrong magic */
    w_header(&b);
    memcpy(b.d, "RIFX", 4);
    CHECK(!mtn_wav_parse(b.d, b.n, &s));
    w_header(&b);
    memcpy(b.d + 8, "AVI ", 4);
    CHECK(!mtn_wav_parse(b.d, b.n, &s));
    /* header only, no chunks */
    w_header(&b);
    CHECK(!mtn_wav_parse(b.d, b.n, &s));
    /* chunk length runs past the end */
    w_header(&b);
    w_fmt(&b, 1, 1, 8000, 16);
    w_data16(&b, src, 2);
    CHECK(!mtn_wav_parse(b.d, b.n - 1, &s));
    /* fmt without data */
    w_header(&b);
    w_fmt(&b, 1, 1, 8000, 16);
    CHECK(!mtn_wav_parse(b.d, b.n, &s));
    /* data before fmt */
    w_header(&b);
    w_data16(&b, src, 2);
    w_fmt(&b, 1, 1, 8000, 16);
    CHECK(!mtn_wav_parse(b.d, b.n, &s));
    /* float, 24 bit, zero channels */
    w_header(&b);
    w_fmt(&b, 3, 1, 8000, 16);
    w_data16(&b, src, 2);
    CHECK(!mtn_wav_parse(b.d, b.n, &s));
    w_header(&b);
    w_fmt(&b, 1, 1, 8000, 24);
    w_data16(&b, src, 2);
    CHECK(!mtn_wav_parse(b.d, b.n, &s));
    w_header(&b);
    w_fmt(&b, 1, 0, 8000, 16);
    w_data16(&b, src, 2);
    CHECK(!mtn_wav_parse(b.d, b.n, &s));
    /* truncated fmt chunk (< 16 bytes) is ignored, so data has no format */
    w_header(&b);
    w_chunk(&b, "fmt ", "\1\0\1\0\100\37\0\0", 8);
    w_data16(&b, src, 2);
    CHECK(!mtn_wav_parse(b.d, b.n, &s));
    CHECK(s.samples == NULL);
}

/* ---------------------------------------------------------------- mixer */

static mtn_sound make_sound(const int16_t *v, int n, int rate) {
    mtn_sound s;
    memset(&s, 0, sizeof s);
    s.samples = malloc(sizeof(int16_t) * (size_t)(n > 0 ? n : 1));
    for (int i = 0; i < n; i++) s.samples[i] = v[i];
    s.count = n;
    s.rate = rate;
    return s;
}

static int all_finite_in(const float *o, int n, float lim) {
    for (int i = 0; i < n; i++)
        if (!(o[i] >= -lim && o[i] <= lim)) return 0;
    return 1;
}

static void test_mixer_sounds(void) {
    mtn_mixer m;
    mtn_mixer_init(&m, 48000);
    CHECK(m.rate == 48000);
    CHECK_NEAR(m.master, 0.8f, 1e-6);
    CHECK(m.sound_count == 0);
    CHECK(mtn_mixer_active_voices(&m) == 0);
    const int16_t one[1] = {100};
    mtn_sound a = make_sound(one, 1, 8000), b = make_sound(one, 1, 8000);
    CHECK(mtn_mixer_add(&m, "step", &a) == 0);
    CHECK(mtn_mixer_add(&m, "bird", &b) == 1);
    CHECK(mtn_mixer_find(&m, "bird") == 1);
    CHECK(mtn_mixer_find(&m, "step") == 0);
    CHECK(mtn_mixer_find(&m, "wolf") == -1);
    CHECK_STR(m.sounds[1].name, "bird");
    /* replacing keeps the slot (and frees the old samples) */
    const int16_t two[2] = {1, 2};
    mtn_sound c = make_sound(two, 2, 22050);
    CHECK(mtn_mixer_add(&m, "step", &c) == 0);
    CHECK(m.sound_count == 2);
    CHECK(m.sounds[0].count == 2 && m.sounds[0].rate == 22050);
    CHECK_STR(m.sounds[0].name, "step");
    /* long names are truncated */
    mtn_sound d = make_sound(one, 1, 8000);
    int li = mtn_mixer_add(&m, "a_really_long_sound_name_that_does_not_fit", &d);
    CHECK(li == 2);
    CHECK(strlen(m.sounds[li].name) == sizeof m.sounds[0].name - 1);
    /* capacity */
    char name[32];
    for (int i = m.sound_count; i < MTN_MAX_SOUNDS; i++) {
        snprintf(name, sizeof name, "s%d", i);
        mtn_sound e = make_sound(one, 1, 8000);
        CHECK(mtn_mixer_add(&m, name, &e) == i);
    }
    CHECK(m.sound_count == MTN_MAX_SOUNDS);
    mtn_sound extra = make_sound(one, 1, 8000);
    CHECK(mtn_mixer_add(&m, "overflow", &extra) == -1); /* samples freed by the mixer */
    CHECK(m.sound_count == MTN_MAX_SOUNDS);
    CHECK(mtn_mixer_find(&m, "overflow") == -1);
    mtn_mixer_free(&m);
    CHECK(m.sound_count == 0);
    CHECK(mtn_mixer_active_voices(&m) == 0);
}

static void test_mixer_play_mix(void) {
    mtn_mixer m;
    mtn_mixer_init(&m, 1000);
    const int16_t flat[4] = {16384, 16384, 16384, 16384};
    mtn_sound s = make_sound(flat, 4, 1000);
    int id = mtn_mixer_add(&m, "flat", &s);
    CHECK(mtn_mixer_play(&m, -1, 1, 0, 1) == -1);
    CHECK(mtn_mixer_play(&m, 5, 1, 0, 1) == -1);
    int v = mtn_mixer_play(&m, id, 1.0f, 0.0f, 0.0f); /* pitch <= 0 means 1 */
    CHECK(v == 0);
    CHECK_NEAR(m.voices[v].step, 1.0f, 1e-6);
    CHECK(mtn_mixer_active_voices(&m) == 1);
    float out[16];
    mtn_mixer_mix(&m, out, 8);
    float expect = tanhf(0.5f * 0.8f * sqrtf(0.5f));
    for (int f = 0; f < 4; f++) {
        CHECK_NEAR(out[f * 2], expect, 1e-5);
        CHECK_NEAR(out[f * 2 + 1], expect, 1e-5);
    }
    for (int f = 4; f < 8; f++) CHECK_NEAR(out[f * 2] + out[f * 2 + 1], 0.0f, 0);
    CHECK(mtn_mixer_active_voices(&m) == 0); /* one-shot finished */

    /* hard left pan and pitch */
    m.master = 1.0f;
    v = mtn_mixer_play(&m, id, 0.5f, -1.0f, 2.0f);
    CHECK_NEAR(m.voices[v].step, 2.0f, 1e-6);
    mtn_mixer_mix(&m, out, 3);
    CHECK_NEAR(out[0], tanhf(0.25f), 1e-5);
    CHECK_NEAR(out[1], 0.0f, 1e-6);
    CHECK_NEAR(out[2], tanhf(0.25f), 1e-5);
    CHECK_NEAR(out[4], 0.0f, 0); /* 2 frames at double speed cover the 4 samples */
    CHECK(mtn_mixer_active_voices(&m) == 0);

    /* linear interpolation between samples at half speed (rate ratio) */
    const int16_t ramp[2] = {0, 32767};
    mtn_sound r = make_sound(ramp, 2, 500);
    int rid = mtn_mixer_add(&m, "ramp", &r);
    v = mtn_mixer_play(&m, rid, 1.0f, 1.0f, 1.0f);
    CHECK_NEAR(m.voices[v].step, 0.5f, 1e-6);
    mtn_mixer_mix(&m, out, 6);
    float top = 32767.0f / 32768.0f;
    CHECK_NEAR(out[1], 0.0f, 1e-6);
    CHECK_NEAR(out[3], tanhf(top * 0.5f), 1e-5);
    CHECK_NEAR(out[5], tanhf(top), 1e-5);
    CHECK_NEAR(out[7], tanhf(top), 1e-5); /* last sample holds (no wrap for one-shots) */
    CHECK_NEAR(out[9], 0.0f, 0);
    CHECK_NEAR(out[0] + out[2] + out[4], 0.0f, 1e-6); /* hard right */

    /* many loud voices are soft clipped */
    const int16_t loud[4] = {32767, 32767, 32767, 32767};
    mtn_sound l = make_sound(loud, 4, 1000);
    int lid = mtn_mixer_add(&m, "loud", &l);
    for (int i = 0; i < 8; i++) mtn_mixer_play(&m, lid, 1.0f, 0.0f, 1.0f);
    mtn_mixer_mix(&m, out, 2);
    CHECK(out[0] > 0.99f && out[0] < 1.0f);
    CHECK(all_finite_in(out, 4, 1.0f));
    mtn_mixer_stop_all(&m);
    CHECK(mtn_mixer_active_voices(&m) == 0);
    mtn_mixer_mix(&m, out, 8);
    CHECK(all_finite_in(out, 16, 0.0f));
    mtn_mixer_free(&m);
}

static void test_mixer_ambient(void) {
    mtn_mixer m;
    mtn_mixer_init(&m, 1000);
    m.master = 1.0f;
    const int16_t alt[2] = {8192, -8192};
    mtn_sound s = make_sound(alt, 2, 1000);
    int id = mtn_mixer_add(&m, "wind", &s);
    /* invalid sounds and silent requests do nothing */
    mtn_mixer_ambient(&m, -1, 1.0f, 1.0f);
    mtn_mixer_ambient(&m, 9, 1.0f, 1.0f);
    mtn_mixer_ambient(&m, id, 0.0f, 1.0f);
    CHECK(mtn_mixer_active_voices(&m) == 0);
    CHECK_NEAR(mtn_mixer_ambient_volume(&m, id), 0.0f, 0);

    /* instant fade-in and looping */
    mtn_mixer_ambient(&m, id, 1.0f, 0.0f);
    CHECK(mtn_mixer_active_voices(&m) == 1);
    CHECK(m.voices[0].loop && m.voices[0].ambient);
    CHECK_NEAR(mtn_mixer_ambient_volume(&m, id), 0.0f, 0); /* starts silent */
    float out[400];
    mtn_mixer_mix(&m, out, 6);
    float a = tanhf(0.25f * sqrtf(0.5f));
    for (int f = 0; f < 6; f++) CHECK_NEAR(out[f * 2], (f % 2) ? -a : a, 1e-5);
    CHECK_NEAR(mtn_mixer_ambient_volume(&m, id), 1.0f, 1e-6);
    CHECK(m.voices[0].pos <= 2.0); /* wraps lazily at the next frame */

    /* fade down over 0.1 s, then the loop stops by itself */
    mtn_mixer_ambient(&m, id, 0.0f, 0.1f);
    CHECK(mtn_mixer_active_voices(&m) == 1);
    mtn_mixer_mix(&m, out, 50);
    CHECK_NEAR(mtn_mixer_ambient_volume(&m, id), 0.5f, 1e-3);
    mtn_mixer_mix(&m, out, 60);
    CHECK(mtn_mixer_active_voices(&m) == 0);
    CHECK_NEAR(mtn_mixer_ambient_volume(&m, id), 0.0f, 0);

    /* slow fade in: 2 volume units per second */
    mtn_mixer_ambient(&m, id, 1.0f, 0.5f);
    mtn_mixer_mix(&m, out, 100);
    CHECK_NEAR(mtn_mixer_ambient_volume(&m, id), 0.2f, 1e-3);
    /* retargeting an existing loop does not start a second one */
    mtn_mixer_ambient(&m, id, 0.3f, 1.0f);
    CHECK(mtn_mixer_active_voices(&m) == 1);
    CHECK_NEAR(m.voices[0].target, 0.3f, 1e-6);
    CHECK_NEAR(m.voices[0].fade, 1.0f, 1e-6);
    mtn_mixer_mix(&m, out, 200);
    CHECK_NEAR(mtn_mixer_ambient_volume(&m, id), 0.3f, 1e-6);
    mtn_mixer_stop_all(&m);

    /* an empty looping sound simply stops */
    mtn_sound e = make_sound(NULL, 0, 1000);
    int eid = mtn_mixer_add(&m, "empty", &e);
    mtn_mixer_ambient(&m, eid, 1.0f, 0.0f);
    CHECK(mtn_mixer_active_voices(&m) == 1);
    mtn_mixer_mix(&m, out, 4);
    CHECK(mtn_mixer_active_voices(&m) == 0);
    CHECK(all_finite_in(out, 8, 0.0f));
    mtn_mixer_free(&m);
}

static void test_mixer_short_loop_fast_step(void) {
    /* a 2 sample loop at 44.1 kHz played into an 8 kHz mixer steps over the whole loop each frame */
    mtn_mixer m;
    mtn_mixer_init(&m, 8000);
    const int16_t two[2] = {1000, -1000};
    mtn_sound s = make_sound(two, 2, 44100);
    int id = mtn_mixer_add(&m, "buzz", &s);
    mtn_mixer_ambient(&m, id, 1.0f, 0.0f);
    float out[128];
    for (int k = 0; k < 4; k++) {
        mtn_mixer_mix(&m, out, 64);
        CHECK(m.voices[0].active);
        CHECK(m.voices[0].pos >= 0.0 && m.voices[0].pos < 2.0 + 5.6);
        CHECK(all_finite_in(out, 128, 0.05f));
    }
    mtn_mixer_free(&m);
}

static void test_mixer_voices(void) {
    mtn_mixer m;
    mtn_mixer_init(&m, 1000);
    const int16_t one[4] = {1, 1, 1, 1};
    char name[16];
    for (int i = 0; i < MTN_MAX_VOICES + 1; i++) {
        snprintf(name, sizeof name, "snd%d", i);
        mtn_sound s = make_sound(one, 4, 1000);
        mtn_mixer_add(&m, name, &s);
    }
    /* fill all voices with one-shots, voice 7 is the quietest */
    for (int i = 0; i < MTN_MAX_VOICES; i++) {
        int v = mtn_mixer_play(&m, 0, i == 7 ? 0.05f : 0.5f + 0.01f * (float)i, 0, 1);
        CHECK(v == i);
    }
    CHECK(mtn_mixer_active_voices(&m) == MTN_MAX_VOICES);
    /* no room for an ambient loop */
    mtn_mixer_ambient(&m, 1, 1.0f, 1.0f);
    CHECK_NEAR(mtn_mixer_ambient_volume(&m, 1), 0.0f, 0);
    for (int i = 0; i < MTN_MAX_VOICES; i++) CHECK(!m.voices[i].ambient);
    /* a new one-shot steals the quietest voice */
    CHECK(mtn_mixer_play(&m, 2, 0.9f, 0, 1) == 7);
    CHECK(m.voices[7].sound == 2);
    CHECK_NEAR(m.voices[7].vol, 0.9f, 1e-6);
    /* next quietest is voice 0 (0.5) */
    CHECK(mtn_mixer_play(&m, 3, 0.9f, 0, 1) == 0);
    /* ambient loops are never stolen */
    mtn_mixer_stop_all(&m);
    for (int i = 0; i < MTN_MAX_VOICES; i++) mtn_mixer_ambient(&m, i, 1.0f, 1.0f);
    CHECK(mtn_mixer_active_voices(&m) == MTN_MAX_VOICES);
    CHECK(mtn_mixer_play(&m, MTN_MAX_VOICES, 1.0f, 0, 1) == -1);
    mtn_mixer_free(&m);
}

MTN_SUITE(audio) {
    TEST_RUN(test_wav_16bit_mono);
    TEST_RUN(test_wav_8bit_and_stereo);
    TEST_RUN(test_wav_chunks);
    TEST_RUN(test_wav_malformed);
    TEST_RUN(test_mixer_sounds);
    TEST_RUN(test_mixer_play_mix);
    TEST_RUN(test_mixer_ambient);
    TEST_RUN(test_mixer_short_loop_fast_step);
    TEST_RUN(test_mixer_voices);
}
