#include "test.h"
#include "mtn/photo.h"

#include <stdlib.h>

static void px(uint8_t *p, int r, int g, int b, int a) {
    p[0] = (uint8_t)r;
    p[1] = (uint8_t)g;
    p[2] = (uint8_t)b;
    p[3] = (uint8_t)a;
}

static int px_is(const uint8_t *p, int r, int g, int b, int a) { return p[0] == r && p[1] == g && p[2] == b && p[3] == a; }

static void test_filter_names(void) {
    CHECK_STR(mtn_filter_name(MTN_FILTER_NONE), "none");
    CHECK_STR(mtn_filter_name(MTN_FILTER_SEPIA), "sepia");
    CHECK_STR(mtn_filter_name(MTN_FILTER_BW), "bw");
    CHECK_STR(mtn_filter_name(MTN_FILTER_VIVID), "vivid");
    CHECK_STR(mtn_filter_name(MTN_FILTER_DUSK), "dusk");
    CHECK_STR(mtn_filter_name(-1), "none");
    CHECK_STR(mtn_filter_name(MTN_FILTER_COUNT), "none");
    for (int i = 0; i < MTN_FILTER_COUNT; i++) CHECK(mtn_filter_from_name(mtn_filter_name(i)) == i);
    CHECK(mtn_filter_from_name("polaroid") == -1);
    CHECK(mtn_filter_from_name("") == -1);
}

static void test_filters(void) {
    uint8_t img[8];
    /* none and out-of-range filters leave the pixels alone */
    px(img, 100, 150, 200, 77);
    px(img + 4, 1, 2, 3, 4);
    mtn_photo_filter_apply(img, 2, 1, MTN_FILTER_NONE);
    mtn_photo_filter_apply(img, 2, 1, MTN_FILTER_COUNT);
    mtn_photo_filter_apply(img, 2, 1, -3);
    CHECK(px_is(img, 100, 150, 200, 77) && px_is(img + 4, 1, 2, 3, 4));

    px(img, 100, 150, 200, 77);
    mtn_photo_filter_apply(img, 1, 1, MTN_FILTER_SEPIA);
    CHECK(px_is(img, 171, 132, 93, 77));
    CHECK(img[0] > img[1] && img[1] > img[2]); /* warm tint */

    px(img, 100, 150, 200, 77);
    mtn_photo_filter_apply(img, 1, 1, MTN_FILTER_BW);
    CHECK(px_is(img, 143, 143, 143, 77));

    px(img, 100, 150, 200, 77);
    mtn_photo_filter_apply(img, 1, 1, MTN_FILTER_VIVID);
    CHECK(px_is(img, 82, 154, 227, 77));

    px(img, 100, 150, 200, 77);
    mtn_photo_filter_apply(img, 1, 1, MTN_FILTER_DUSK);
    CHECK(px_is(img, 122, 140, 185, 77));

    /* saturation clamps at both ends, every pixel is processed */
    px(img, 255, 0, 0, 255);
    px(img + 4, 0, 0, 255, 9);
    mtn_photo_filter_apply(img, 2, 1, MTN_FILTER_VIVID);
    CHECK(px_is(img, 255, 0, 0, 255));
    CHECK(img[4] == 0 && img[6] == 255 && img[7] == 9);
    px(img, 255, 255, 255, 1);
    mtn_photo_filter_apply(img, 1, 1, MTN_FILTER_BW);
    CHECK(px_is(img, 255, 255, 255, 1));
    px(img, 0, 0, 0, 1);
    mtn_photo_filter_apply(img, 1, 1, MTN_FILTER_BW);
    CHECK(px_is(img, 0, 0, 0, 1));
}

static void test_flip(void) {
    uint8_t img[2 * 3 * 4];
    for (int y = 0; y < 3; y++)
        for (int i = 0; i < 8; i++) img[y * 8 + i] = (uint8_t)(10 * (y + 1) + i);
    mtn_photo_flip_y(img, 2, 3);
    for (int i = 0; i < 8; i++) {
        CHECK(img[i] == 30 + i);
        CHECK(img[8 + i] == 20 + i);
        CHECK(img[16 + i] == 10 + i);
    }
    mtn_photo_flip_y(img, 2, 3);
    CHECK(img[0] == 10 && img[16] == 30);
    uint8_t one[4] = {1, 2, 3, 4};
    mtn_photo_flip_y(one, 1, 1);
    CHECK(px_is(one, 1, 2, 3, 4));
}

static void test_scale(void) {
    CHECK(mtn_photo_scale_for(270, 1080) == 4);
    CHECK(mtn_photo_scale_for(300, 1080) == 4); /* rounds up */
    CHECK(mtn_photo_scale_for(1080, 1080) == 1);
    CHECK(mtn_photo_scale_for(2000, 1080) == 1);
    CHECK(mtn_photo_scale_for(0, 1080) == 1);
    CHECK(mtn_photo_scale_for(-5, 1080) == 1);
    CHECK(mtn_photo_scale_for(100, 0) == 1);
    CHECK(mtn_photo_scale_for(100, -500) == 1);

    uint8_t src[2 * 1 * 4];
    px(src, 1, 2, 3, 4);
    px(src + 4, 5, 6, 7, 8);
    uint8_t *up = mtn_photo_upscale(src, 2, 1, 2);
    for (int y = 0; y < 2; y++) {
        const uint8_t *row = up + y * 16;
        CHECK(px_is(row, 1, 2, 3, 4) && px_is(row + 4, 1, 2, 3, 4));
        CHECK(px_is(row + 8, 5, 6, 7, 8) && px_is(row + 12, 5, 6, 7, 8));
    }
    free(up);
    up = mtn_photo_upscale(src, 2, 1, 0); /* factor < 1 copies */
    CHECK(memcmp(up, src, sizeof src) == 0);
    free(up);

    /* 4x2 -> 2x1 box filter */
    uint8_t big[4 * 2 * 4];
    for (int y = 0; y < 2; y++)
        for (int x = 0; x < 4; x++) px(big + (y * 4 + x) * 4, x < 2 ? 10 * (y + 1) : 200, x, 0, 255);
    uint8_t *dn = mtn_photo_downscale(big, 4, 2, 2, 1);
    CHECK(px_is(dn, 15, 0, 0, 255)); /* (10+10+20+20)/4, (0+1+0+1)/4 */
    CHECK(px_is(dn + 4, 200, 2, 0, 255)); /* (2+3+2+3)/4 */
    free(dn);
    /* "downscaling" to a larger size samples the nearest source pixel */
    dn = mtn_photo_downscale(src, 2, 1, 4, 2);
    CHECK(px_is(dn, 1, 2, 3, 4) && px_is(dn + 4, 1, 2, 3, 4));
    CHECK(px_is(dn + 8, 5, 6, 7, 8) && px_is(dn + 28, 5, 6, 7, 8));
    free(dn);
    /* upscale then downscale is lossless */
    uint8_t *u3 = mtn_photo_upscale(big, 4, 2, 3);
    uint8_t *d3 = mtn_photo_downscale(u3, 12, 6, 4, 2);
    CHECK(memcmp(d3, big, sizeof big) == 0);
    free(u3);
    free(d3);
}

static void test_png(void) {
    const int w = 5, h = 3;
    uint8_t img[5 * 3 * 4];
    for (int i = 0; i < w * h; i++) px(img + i * 4, i * 17, 255 - i * 9, (i * 31) & 255, i == 0 ? 0 : 255 - i);
    size_t n = 0;
    uint8_t *png = mtn_png_encode(img, w, h, &n);
    CHECK(png != NULL);
    CHECK(n > 8);
    CHECK(png && memcmp(png, "\x89PNG\r\n\x1a\n", 8) == 0);
    int dw = 0, dh = 0;
    uint8_t *back = mtn_png_decode(png, n, &dw, &dh);
    CHECK(back != NULL);
    CHECK(dw == w && dh == h);
    CHECK(back && memcmp(back, img, sizeof img) == 0);
    mtn_image_free(back);
    /* corrupt data does not decode */
    uint8_t junk[32];
    memset(junk, 0x5A, sizeof junk);
    CHECK(mtn_png_decode(junk, sizeof junk, &dw, &dh) == NULL);
    CHECK(mtn_png_decode(png, 10, &dw, &dh) == NULL); /* truncated */
    free(png);
    /* invalid sizes */
    n = 77;
    CHECK(mtn_png_encode(img, 0, 3, &n) == NULL);
    CHECK(n == 0);
    n = 77;
    CHECK(mtn_png_encode(img, 3, -1, &n) == NULL);
    CHECK(n == 0);
    /* a larger, incompressible image round trips too */
    const int bw = 64, bh = 64;
    uint8_t *noise = malloc((size_t)bw * bh * 4);
    uint32_t s = 12345u;
    for (int i = 0; i < bw * bh * 4; i++) {
        s = s * 1664525u + 1013904223u;
        noise[i] = (uint8_t)(s >> 24);
    }
    png = mtn_png_encode(noise, bw, bh, &n);
    CHECK(png && n > 4096);
    back = mtn_png_decode(png, n, &dw, &dh);
    CHECK(back && dw == bw && dh == bh && memcmp(back, noise, (size_t)bw * bh * 4) == 0);
    mtn_image_free(back);
    free(png);
    free(noise);
}

static void test_names(void) {
    char out[96];
    mtn_photo_name(out, sizeof out, 42, 2026, 3, 7, 9, 5, 1, 0);
    CHECK_STR(out, "mountain-42-20260307-090501.png");
    mtn_photo_name(out, sizeof out, 4000000000u, 1999, 12, 31, 23, 59, 58, 3);
    CHECK_STR(out, "mountain-4000000000-19991231-235958-3.png");
    mtn_photo_name(out, sizeof out, 1, 2026, 1, 1, 0, 0, 0, -2);
    CHECK_STR(out, "mountain-1-20260101-000000.png");
}

static void test_gallery(void) {
    static mtn_gallery g;
    mtn_gallery_clear(&g);
    CHECK(g.count == 0);
    CHECK(mtn_gallery_find(&g, "x.png") == -1);
    CHECK(mtn_gallery_add(&g, "mountain-1-20260101-000000.png") == 1);
    CHECK(mtn_gallery_add(&g, "mountain-1-20260102-000000.png") == 1);
    CHECK(mtn_gallery_add(&g, "mountain-1-20260101-000000.png") == 0); /* duplicate */
    CHECK(g.count == 2);
    CHECK_STR(g.names[0], "mountain-1-20260102-000000.png"); /* newest first */
    CHECK(mtn_gallery_find(&g, "mountain-1-20260101-000000.png") == 1);
    CHECK(!mtn_gallery_remove(&g, 2));
    CHECK(!mtn_gallery_remove(&g, -1));
    CHECK(mtn_gallery_remove(&g, 0));
    CHECK(g.count == 1);
    CHECK_STR(g.names[0], "mountain-1-20260101-000000.png");
    CHECK(mtn_gallery_remove(&g, 0));
    CHECK(g.count == 0);

    /* capacity */
    char name[96];
    for (int i = 0; i < MTN_MAX_PHOTOS; i++) {
        snprintf(name, sizeof name, "p%03d.png", i);
        CHECK(mtn_gallery_add(&g, name) == 1);
    }
    CHECK(g.count == MTN_MAX_PHOTOS);
    CHECK(mtn_gallery_add(&g, "one-too-many.png") == -1);
    CHECK(mtn_gallery_add(&g, "p000.png") == 0);
    CHECK_STR(g.names[0], "p255.png");
    CHECK_STR(g.names[MTN_MAX_PHOTOS - 1], "p000.png");
    /* removing from the middle keeps the order */
    CHECK(mtn_gallery_remove(&g, 1));
    CHECK_STR(g.names[1], "p253.png");
    CHECK(mtn_gallery_remove(&g, g.count - 1));
    CHECK_STR(g.names[g.count - 1], "p001.png");

    /* sorting by date across worlds, ties by full name, odd names first (byte order) */
    mtn_gallery_clear(&g);
    const char *in[6] = {"mountain-7-20250101-120000.png", "mountain-12-20260101-120000.png",
                         "mountain-3-20251231-235959.png", "weird", "mountain-9-20260101-120000.png", "a-b"};
    for (int i = 0; i < 6; i++) mtn_gallery_add(&g, in[i]);
    mtn_gallery_sort(&g);
    CHECK_STR(g.names[0], "weird");
    CHECK_STR(g.names[1], "a-b");
    CHECK_STR(g.names[2], "mountain-9-20260101-120000.png");
    CHECK_STR(g.names[3], "mountain-12-20260101-120000.png");
    CHECK_STR(g.names[4], "mountain-3-20251231-235959.png");
    CHECK_STR(g.names[5], "mountain-7-20250101-120000.png");
    CHECK(g.count == 6);
}

MTN_SUITE(photo) {
    TEST_RUN(test_filter_names);
    TEST_RUN(test_filters);
    TEST_RUN(test_flip);
    TEST_RUN(test_scale);
    TEST_RUN(test_png);
    TEST_RUN(test_names);
    TEST_RUN(test_gallery);
}
