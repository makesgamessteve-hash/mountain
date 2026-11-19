/* Mountain engine - photos: filters, upscaling, PNG encoding, gallery list. MIT License. */
#include "mtn/photo.h"

#include "stb_image.h"
#include "stb_image_write.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char *FILTERS[MTN_FILTER_COUNT] = {"none", "sepia", "bw", "vivid", "dusk"};

const char *mtn_filter_name(int f) { return (f >= 0 && f < MTN_FILTER_COUNT) ? FILTERS[f] : "none"; }

int mtn_filter_from_name(const char *name) {
    for (int i = 0; i < MTN_FILTER_COUNT; i++)
        if (strcmp(FILTERS[i], name) == 0) return i;
    return -1;
}

static uint8_t clamp8(float v) { return (uint8_t)(v < 0.0f ? 0.0f : (v > 255.0f ? 255.0f : v + 0.5f)); }

void mtn_photo_filter_apply(uint8_t *p, int w, int h, int filter) {
    if (filter <= MTN_FILTER_NONE || filter >= MTN_FILTER_COUNT) return;
    size_t n = (size_t)w * (size_t)h;
    for (size_t i = 0; i < n; i++, p += 4) {
        float r = p[0], g = p[1], b = p[2];
        float l = 0.299f * r + 0.587f * g + 0.114f * b;
        switch (filter) {
        case MTN_FILTER_SEPIA:
            p[0] = clamp8(l * 1.07f + 20.0f);
            p[1] = clamp8(l * 0.88f + 8.0f);
            p[2] = clamp8(l * 0.66f);
            break;
        case MTN_FILTER_BW: {
            float c = (l - 128.0f) * 1.15f + 128.0f;
            p[0] = p[1] = p[2] = clamp8(c);
            break;
        }
        case MTN_FILTER_VIVID:
            p[0] = clamp8(l + (r - l) * 1.45f);
            p[1] = clamp8(l + (g - l) * 1.45f);
            p[2] = clamp8(l + (b - l) * 1.45f);
            break;
        default: /* dusk: warm highlights, violet shadows */
            p[0] = clamp8(r * 1.08f + 14.0f);
            p[1] = clamp8(g * 0.92f + 2.0f);
            p[2] = clamp8(b * 0.86f + 30.0f * (1.0f - l / 255.0f));
            break;
        }
    }
}

void mtn_photo_flip_y(uint8_t *rgba, int w, int h) {
    size_t row = (size_t)w * 4;
    uint8_t *tmp = malloc(row);
    for (int y = 0; y < h / 2; y++) {
        uint8_t *a = rgba + (size_t)y * row, *b = rgba + (size_t)(h - 1 - y) * row;
        memcpy(tmp, a, row);
        memcpy(a, b, row);
        memcpy(b, tmp, row);
    }
    free(tmp);
}

int mtn_photo_scale_for(int h, int min_h) {
    if (h <= 0) return 1;
    int f = (min_h + h - 1) / h;
    return f < 1 ? 1 : f;
}

uint8_t *mtn_photo_upscale(const uint8_t *src, int w, int h, int k) {
    if (k < 1) k = 1;
    int W = w * k, H = h * k;
    uint8_t *dst = malloc((size_t)W * (size_t)H * 4);
    for (int y = 0; y < H; y++) {
        const uint32_t *s = (const uint32_t *)(const void *)(src + (size_t)(y / k) * (size_t)w * 4);
        uint32_t *d = (uint32_t *)(void *)(dst + (size_t)y * (size_t)W * 4);
        for (int x = 0; x < W; x++) d[x] = s[x / k];
    }
    return dst;
}

uint8_t *mtn_photo_downscale(const uint8_t *src, int w, int h, int dw, int dh) {
    uint8_t *dst = malloc((size_t)dw * (size_t)dh * 4);
    for (int y = 0; y < dh; y++)
        for (int x = 0; x < dw; x++) {
            int x0 = x * w / dw, x1 = (x + 1) * w / dw, y0 = y * h / dh, y1 = (y + 1) * h / dh;
            if (x1 <= x0) x1 = x0 + 1;
            if (y1 <= y0) y1 = y0 + 1;
            unsigned acc[4] = {0, 0, 0, 0}, n = 0;
            for (int yy = y0; yy < y1; yy++)
                for (int xx = x0; xx < x1; xx++) {
                    const uint8_t *p = src + ((size_t)yy * (size_t)w + (size_t)xx) * 4;
                    for (int c = 0; c < 4; c++) acc[c] += p[c];
                    n++;
                }
            for (int c = 0; c < 4; c++) dst[((size_t)y * (size_t)dw + (size_t)x) * 4 + (size_t)c] = (uint8_t)(acc[c] / n);
        }
    return dst;
}

typedef struct { uint8_t *data; size_t size, cap; } membuf;

static void write_cb(void *ctx, void *data, int size) {
    membuf *m = ctx;
    if (m->size + (size_t)size > m->cap) {
        m->cap = (m->size + (size_t)size) * 2;
        m->data = realloc(m->data, m->cap);
    }
    memcpy(m->data + m->size, data, (size_t)size);
    m->size += (size_t)size;
}

uint8_t *mtn_png_encode(const uint8_t *rgba, int w, int h, size_t *out_size) {
    membuf m = {NULL, 0, 0};
    if (w <= 0 || h <= 0 || !stbi_write_png_to_func(write_cb, &m, w, h, 4, rgba, w * 4)) {
        free(m.data);
        *out_size = 0;
        return NULL;
    }
    *out_size = m.size;
    return m.data;
}

uint8_t *mtn_png_decode(const uint8_t *data, size_t size, int *w, int *h) {
    int comp;
    return stbi_load_from_memory(data, (int)size, w, h, &comp, 4);
}

void mtn_image_free(void *p) { stbi_image_free(p); }

void mtn_photo_name(char *out, size_t n, uint32_t seed, int year, int month, int day, int hour, int minute, int second,
                    int counter) {
    if (counter > 0)
        snprintf(out, n, "mountain-%u-%04d%02d%02d-%02d%02d%02d-%d.png", seed, year, month, day, hour, minute, second, counter);
    else
        snprintf(out, n, "mountain-%u-%04d%02d%02d-%02d%02d%02d.png", seed, year, month, day, hour, minute, second);
}

void mtn_gallery_clear(mtn_gallery *g) { g->count = 0; }

int mtn_gallery_find(const mtn_gallery *g, const char *name) {
    for (int i = 0; i < g->count; i++)
        if (strcmp(g->names[i], name) == 0) return i;
    return -1;
}

int mtn_gallery_add(mtn_gallery *g, const char *name) {
    if (mtn_gallery_find(g, name) >= 0) return 0;
    if (g->count >= MTN_MAX_PHOTOS) return -1;
    /* newest first */
    memmove(g->names[1], g->names[0], sizeof(g->names[0]) * (size_t)g->count);
    snprintf(g->names[0], sizeof g->names[0], "%s", name);
    g->count++;
    return 1;
}

int mtn_gallery_remove(mtn_gallery *g, int index) {
    if (index < 0 || index >= g->count) return 0;
    memmove(g->names[index], g->names[index + 1], sizeof(g->names[0]) * (size_t)(g->count - index - 1));
    g->count--;
    return 1;
}

/* sort by the timestamp part (after "mountain-<seed>-") so different worlds interleave by date */
static const char *date_part(const char *s) {
    const char *a = strchr(s, '-');
    const char *b = a ? strchr(a + 1, '-') : NULL;
    return b ? b + 1 : s;
}

static int cmp_desc(const void *a, const void *b) {
    int c = strcmp(date_part((const char *)b), date_part((const char *)a));
    return c ? c : strcmp((const char *)b, (const char *)a);
}

void mtn_gallery_sort(mtn_gallery *g) { qsort(g->names, (size_t)g->count, sizeof g->names[0], cmp_desc); }
