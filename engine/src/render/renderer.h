/* Mountain engine - OpenGL renderer. MIT License. */
#ifndef MTN_RENDERER_H
#define MTN_RENDERER_H

#include "mtn/game.h"
#include <stdint.h>

typedef struct mtn_renderer mtn_renderer;

typedef struct {
    int draw_calls;
    int triangles;
    int flora_chunks;
    int portals;
    int lw, lh;
} mtn_render_stats;

mtn_renderer *mtn_render_create(void);
void mtn_render_destroy(mtn_renderer *r);
/* Loads shaders and textures from the asset folder. Returns 0 and fills err on failure. */
int mtn_render_load(mtn_renderer *r, char *err, size_t n);
/* Renders a full frame (scene, post process, interface) into the default framebuffer. */
void mtn_render_frame(mtn_renderer *r, mtn_game *g, int win_w, int win_h);
/* Renders one stereo view for WebXR into fbo/viewport with the given matrices. */
void mtn_render_view(mtn_renderer *r, mtn_game *g, mat4 view, mat4 proj, vec3 eye, unsigned fbo, int x, int y, int w,
                     int h, int with_ui);
/* Reads the last low resolution frame (post processed, no interface). Caller frees. */
uint8_t *mtn_render_capture(mtn_renderer *r, mtn_game *g, int *w, int *h);
/* Reads the window framebuffer (for game test screenshots). Caller frees. */
uint8_t *mtn_render_read_window(int w, int h);
void mtn_render_forget_photo(mtn_renderer *r, const char *name);
mtn_render_stats mtn_render_get_stats(const mtn_renderer *r);

#endif
