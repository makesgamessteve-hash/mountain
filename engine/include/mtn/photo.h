/* Mountain engine - photos: filters, upscaling, PNG encoding, gallery list. MIT License. */
#ifndef MTN_PHOTO_H
#define MTN_PHOTO_H

#include <stddef.h>
#include <stdint.h>

typedef enum { MTN_FILTER_NONE = 0, MTN_FILTER_SEPIA, MTN_FILTER_BW, MTN_FILTER_VIVID, MTN_FILTER_DUSK, MTN_FILTER_COUNT } mtn_photo_filter;

#define MTN_MAX_PHOTOS 256

typedef struct {
    char names[MTN_MAX_PHOTOS][96];
    int count;
} mtn_gallery;

const char *mtn_filter_name(int f);
int mtn_filter_from_name(const char *name);
void mtn_photo_filter_apply(uint8_t *rgba, int w, int h, int filter);
void mtn_photo_flip_y(uint8_t *rgba, int w, int h);
int mtn_photo_scale_for(int h, int min_h);
uint8_t *mtn_photo_upscale(const uint8_t *src, int w, int h, int factor);
uint8_t *mtn_photo_downscale(const uint8_t *src, int w, int h, int dw, int dh);
/* Encodes RGBA to PNG in memory (malloc'd, caller frees). */
uint8_t *mtn_png_encode(const uint8_t *rgba, int w, int h, size_t *out_size);
/* Decodes a PNG (RGBA8). Caller frees with mtn_image_free. */
uint8_t *mtn_png_decode(const uint8_t *data, size_t size, int *w, int *h);
void mtn_image_free(void *p);
/* "mountain-<seed>-<YYYYMMDD-HHMMSS>[-n].png" */
void mtn_photo_name(char *out, size_t n, uint32_t seed, int year, int month, int day, int hour, int minute,
                    int second, int counter);

void mtn_gallery_clear(mtn_gallery *g);
int mtn_gallery_add(mtn_gallery *g, const char *name);
int mtn_gallery_remove(mtn_gallery *g, int index);
int mtn_gallery_find(const mtn_gallery *g, const char *name);
void mtn_gallery_sort(mtn_gallery *g); /* newest (lexicographically largest) first */

#endif
