/* Mountain engine - OpenGL function loader for desktop builds. MIT License. */
#define MTN_GL_NO_REDIRECT
#include "render/gl.h"

#ifndef __EMSCRIPTEN__
#define MTN_GL_DEFINE(ret, name, args) mtn_pfn_##name mtn_##name;
MTN_GL_FUNCS(MTN_GL_DEFINE)
#undef MTN_GL_DEFINE

int mtn_gl_load(mtn_glproc (*getproc)(const char *)) {
    int ok = 1;
#define MTN_GL_LOAD(ret, name, args)                              \
    mtn_##name = (mtn_pfn_##name)getproc(#name); \
    if (!mtn_##name) ok = 0;
    MTN_GL_FUNCS(MTN_GL_LOAD)
#undef MTN_GL_LOAD
    return ok;
}
#endif
