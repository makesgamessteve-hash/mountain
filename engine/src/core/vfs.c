/* Mountain engine - asset file system: assets/ and mods/ folders beside the executable. MIT License. */
#include "mtn/vfs.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char g_base[512];

void mtn_vfs_init(const char *base) { snprintf(g_base, sizeof g_base, "%s", base ? base : ""); }

const char *mtn_vfs_base(void) { return g_base; }

int mtn_vfs_path_ok(const char *rel) {
    if (!rel || !*rel) return 0;
    if (rel[0] == '/' || rel[0] == '\\' || strchr(rel, ':')) return 0;
    if (strstr(rel, "..")) return 0;
    return 1;
}

int mtn_file_exists(const char *path) {
    FILE *f = fopen(path, "rb");
    if (!f) return 0;
    fclose(f);
    return 1;
}

int mtn_vfs_resolve(const char *rel, char *out, size_t n) {
    if (!mtn_vfs_path_ok(rel)) return 0;
    snprintf(out, n, "%smods/%s", g_base, rel);
    if (mtn_file_exists(out)) return 1;
    snprintf(out, n, "%sassets/%s", g_base, rel);
    return mtn_file_exists(out);
}

char *mtn_file_read(const char *path, size_t *size) {
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (len < 0) len = 0;
    char *buf = malloc((size_t)len + 1);
    size_t got = fread(buf, 1, (size_t)len, f);
    fclose(f);
    buf[got] = 0;
    if (size) *size = got;
    return buf;
}

int mtn_file_write(const char *path, const void *data, size_t size) {
    FILE *f = fopen(path, "wb");
    if (!f) return 0;
    size_t put = fwrite(data, 1, size, f);
    fclose(f);
    return put == size;
}

char *mtn_vfs_read(const char *rel, size_t *size) {
    char path[1024];
    if (!mtn_vfs_resolve(rel, path, sizeof path)) return NULL;
    return mtn_file_read(path, size);
}
