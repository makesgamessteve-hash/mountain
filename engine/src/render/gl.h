/* Mountain engine - OpenGL 3.3 core / OpenGL ES 3.0 (WebGL2) function access. MIT License. */
#ifndef MTN_GL_H
#define MTN_GL_H

#ifdef __EMSCRIPTEN__
#include <GLES3/gl3.h>
#define MTN_GLES 1
typedef void (*mtn_glproc)(void);
static inline int mtn_gl_load(mtn_glproc (*getproc)(const char *)) { (void)getproc; return 1; }
#else
#include <SDL3/SDL_opengl.h>
#define MTN_GLES 0

#ifndef APIENTRY
#define APIENTRY
#endif

/* X-macro list of every GL entry point the renderer uses beyond GL 1.1 */
#define MTN_GL_FUNCS(X)                                                                                    \
    X(void, glActiveTexture, (GLenum t))                                                                    \
    X(void, glAttachShader, (GLuint p, GLuint s))                                                           \
    X(void, glBindBuffer, (GLenum t, GLuint b))                                                             \
    X(void, glBindFramebuffer, (GLenum t, GLuint f))                                                        \
    X(void, glBindRenderbuffer, (GLenum t, GLuint r))                                                       \
    X(void, glBindVertexArray, (GLuint a))                                                                  \
    X(void, glBindAttribLocation, (GLuint p, GLuint i, const GLchar *n))                                    \
    X(void, glBlitFramebuffer, (GLint a, GLint b, GLint c, GLint d, GLint e, GLint f, GLint g, GLint h,       \
                                GLbitfield m, GLenum fl))                                                    \
    X(void, glBufferData, (GLenum t, GLsizeiptr s, const void *d, GLenum u))                                \
    X(void, glBufferSubData, (GLenum t, GLintptr o, GLsizeiptr s, const void *d))                           \
    X(GLenum, glCheckFramebufferStatus, (GLenum t))                                                         \
    X(void, glCompileShader, (GLuint s))                                                                    \
    X(GLuint, glCreateProgram, (void))                                                                      \
    X(GLuint, glCreateShader, (GLenum t))                                                                   \
    X(void, glDeleteBuffers, (GLsizei n, const GLuint *b))                                                  \
    X(void, glDeleteFramebuffers, (GLsizei n, const GLuint *f))                                             \
    X(void, glDeleteProgram, (GLuint p))                                                                    \
    X(void, glDeleteRenderbuffers, (GLsizei n, const GLuint *r))                                            \
    X(void, glDeleteShader, (GLuint s))                                                                     \
    X(void, glDeleteVertexArrays, (GLsizei n, const GLuint *a))                                             \
    X(void, glEnableVertexAttribArray, (GLuint i))                                                          \
    X(void, glFramebufferRenderbuffer, (GLenum t, GLenum a, GLenum rt, GLuint r))                           \
    X(void, glFramebufferTexture2D, (GLenum t, GLenum a, GLenum tt, GLuint tex, GLint l))                   \
    X(void, glGenBuffers, (GLsizei n, GLuint *b))                                                           \
    X(void, glGenFramebuffers, (GLsizei n, GLuint *f))                                                      \
    X(void, glGenRenderbuffers, (GLsizei n, GLuint *r))                                                     \
    X(void, glGenVertexArrays, (GLsizei n, GLuint *a))                                                      \
    X(void, glGetProgramInfoLog, (GLuint p, GLsizei m, GLsizei *l, GLchar *s))                              \
    X(void, glGetProgramiv, (GLuint p, GLenum n, GLint *v))                                                 \
    X(void, glGetShaderInfoLog, (GLuint s, GLsizei m, GLsizei *l, GLchar *i))                               \
    X(void, glGetShaderiv, (GLuint s, GLenum n, GLint *v))                                                  \
    X(GLint, glGetUniformLocation, (GLuint p, const GLchar *n))                                             \
    X(void, glLinkProgram, (GLuint p))                                                                      \
    X(void, glRenderbufferStorage, (GLenum t, GLenum f, GLsizei w, GLsizei h))                              \
    X(void, glShaderSource, (GLuint s, GLsizei c, const GLchar *const *str, const GLint *l))                \
    X(void, glUniform1f, (GLint l, GLfloat v))                                                              \
    X(void, glUniform1i, (GLint l, GLint v))                                                                \
    X(void, glUniform2f, (GLint l, GLfloat a, GLfloat b))                                                   \
    X(void, glUniform3f, (GLint l, GLfloat a, GLfloat b, GLfloat c))                                        \
    X(void, glUniform4f, (GLint l, GLfloat a, GLfloat b, GLfloat c, GLfloat d))                             \
    X(void, glUniform1fv, (GLint l, GLsizei c, const GLfloat *v))                                           \
    X(void, glUniform3fv, (GLint l, GLsizei c, const GLfloat *v))                                           \
    X(void, glUniform4fv, (GLint l, GLsizei c, const GLfloat *v))                                           \
    X(void, glUniformMatrix4fv, (GLint l, GLsizei c, GLboolean t, const GLfloat *v))                        \
    X(void, glUseProgram, (GLuint p))                                                                       \
    X(void, glVertexAttribPointer, (GLuint i, GLint s, GLenum t, GLboolean n, GLsizei st, const void *p))

#define MTN_GL_DECLARE(ret, name, args) typedef ret(APIENTRY *mtn_pfn_##name) args; extern mtn_pfn_##name mtn_##name;
MTN_GL_FUNCS(MTN_GL_DECLARE)
#undef MTN_GL_DECLARE

#ifndef MTN_GL_NO_REDIRECT
#define glActiveTexture mtn_glActiveTexture
#define glAttachShader mtn_glAttachShader
#define glBindBuffer mtn_glBindBuffer
#define glBindFramebuffer mtn_glBindFramebuffer
#define glBindRenderbuffer mtn_glBindRenderbuffer
#define glBindVertexArray mtn_glBindVertexArray
#define glBindAttribLocation mtn_glBindAttribLocation
#define glBlitFramebuffer mtn_glBlitFramebuffer
#define glBufferData mtn_glBufferData
#define glBufferSubData mtn_glBufferSubData
#define glCheckFramebufferStatus mtn_glCheckFramebufferStatus
#define glCompileShader mtn_glCompileShader
#define glCreateProgram mtn_glCreateProgram
#define glCreateShader mtn_glCreateShader
#define glDeleteBuffers mtn_glDeleteBuffers
#define glDeleteFramebuffers mtn_glDeleteFramebuffers
#define glDeleteProgram mtn_glDeleteProgram
#define glDeleteRenderbuffers mtn_glDeleteRenderbuffers
#define glDeleteShader mtn_glDeleteShader
#define glDeleteVertexArrays mtn_glDeleteVertexArrays
#define glEnableVertexAttribArray mtn_glEnableVertexAttribArray
#define glFramebufferRenderbuffer mtn_glFramebufferRenderbuffer
#define glFramebufferTexture2D mtn_glFramebufferTexture2D
#define glGenBuffers mtn_glGenBuffers
#define glGenFramebuffers mtn_glGenFramebuffers
#define glGenRenderbuffers mtn_glGenRenderbuffers
#define glGenVertexArrays mtn_glGenVertexArrays
#define glGetProgramInfoLog mtn_glGetProgramInfoLog
#define glGetProgramiv mtn_glGetProgramiv
#define glGetShaderInfoLog mtn_glGetShaderInfoLog
#define glGetShaderiv mtn_glGetShaderiv
#define glGetUniformLocation mtn_glGetUniformLocation
#define glLinkProgram mtn_glLinkProgram
#define glRenderbufferStorage mtn_glRenderbufferStorage
#define glShaderSource mtn_glShaderSource
#define glUniform1f mtn_glUniform1f
#define glUniform1i mtn_glUniform1i
#define glUniform2f mtn_glUniform2f
#define glUniform3f mtn_glUniform3f
#define glUniform4f mtn_glUniform4f
#define glUniform1fv mtn_glUniform1fv
#define glUniform3fv mtn_glUniform3fv
#define glUniform4fv mtn_glUniform4fv
#define glUniformMatrix4fv mtn_glUniformMatrix4fv
#define glUseProgram mtn_glUseProgram
#define glVertexAttribPointer mtn_glVertexAttribPointer
#endif

typedef void (*mtn_glproc)(void);
int mtn_gl_load(mtn_glproc (*getproc)(const char *));
#endif

#endif
