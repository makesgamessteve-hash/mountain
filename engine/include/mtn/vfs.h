/* Mountain engine - asset file system: assets/ and mods/ folders beside the executable. MIT License. */
#ifndef MTN_VFS_H
#define MTN_VFS_H

#include <stddef.h>

/* base: directory that contains the assets/ (and optional mods/) folder, with trailing slash or empty */
void mtn_vfs_init(const char *base);
const char *mtn_vfs_base(void);
/* Resolves "scripts/main.lua" to mods/scripts/main.lua if it exists, else assets/scripts/main.lua. */
int mtn_vfs_resolve(const char *rel, char *out, size_t n);
/* Reads a whole file (NUL terminated, size excludes the terminator). Caller frees. */
char *mtn_vfs_read(const char *rel, size_t *size);
char *mtn_file_read(const char *path, size_t *size);
int mtn_file_write(const char *path, const void *data, size_t size);
int mtn_file_exists(const char *path);
/* rejects absolute paths and ".." so scripts cannot escape the asset folders */
int mtn_vfs_path_ok(const char *rel);

#endif
