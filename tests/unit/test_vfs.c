#if !defined(_WIN32) && !defined(_POSIX_C_SOURCE)
#define _POSIX_C_SOURCE 200809L
#endif
#include "test.h"
#include "mtn/vfs.h"

#include <stdlib.h>

#ifdef _WIN32
#include <direct.h>
#include <process.h>
#define test_mkdir(p) _mkdir(p)
#define test_rmdir(p) _rmdir(p)
#define test_getpid() _getpid()
#else
#include <sys/stat.h>
#include <unistd.h>
#define test_mkdir(p) mkdir((p), 0755)
#define test_rmdir(p) rmdir(p)
#define test_getpid() getpid()
#endif

/* temporary tree: <tmp>/mtn_vfs_<pid>/{assets,mods}/... */
static char g_root[512];

static void tmp_path(char *out, size_t n, const char *rel) { snprintf(out, n, "%s%s", g_root, rel); }

static void put(const char *rel, const char *text) {
    char p[1024];
    tmp_path(p, sizeof p, rel);
    CHECK(mtn_file_write(p, text, strlen(text)));
}

static void rm(const char *rel) {
    char p[1024];
    tmp_path(p, sizeof p, rel);
    remove(p);
}

static void rmd(const char *rel) {
    char p[1024];
    tmp_path(p, sizeof p, rel);
    test_rmdir(p);
}

static void mkd(const char *rel) {
    char p[1024];
    tmp_path(p, sizeof p, rel);
    test_mkdir(p);
}

static void setup_tree(void) {
    const char *tmp = getenv("TMPDIR");
    if (!tmp || !*tmp) tmp = getenv("TEMP");
    if (!tmp || !*tmp) tmp = "/tmp";
    size_t l = strlen(tmp);
    const char *sep = (tmp[l - 1] == '/' || tmp[l - 1] == '\\') ? "" : "/";
    snprintf(g_root, sizeof g_root, "%s%smtn_vfs_%d/", tmp, sep, (int)test_getpid());
    test_mkdir(g_root);
    mkd("assets");
    mkd("assets/scripts");
    mkd("mods");
    mkd("mods/scripts");
    put("assets/scripts/main.lua", "print('base')\n");
    put("assets/scripts/only_base.lua", "base only");
    put("mods/scripts/main.lua", "print('mod')");
    put("mods/only_mod.txt", "mod only");
    put("assets/empty.txt", "");
}

static void teardown_tree(void) {
    rm("assets/scripts/main.lua");
    rm("assets/scripts/only_base.lua");
    rm("mods/scripts/main.lua");
    rm("mods/only_mod.txt");
    rm("assets/empty.txt");
    rm("written.bin");
    rmd("assets/scripts");
    rmd("mods/scripts");
    rmd("assets");
    rmd("mods");
    test_rmdir(g_root);
}

static void test_path_ok(void) {
    CHECK(!mtn_vfs_path_ok(NULL));
    CHECK(!mtn_vfs_path_ok(""));
    CHECK(!mtn_vfs_path_ok("/etc/passwd"));
    CHECK(!mtn_vfs_path_ok("\\windows\\system32"));
    CHECK(!mtn_vfs_path_ok("C:/game/assets/x"));
    CHECK(!mtn_vfs_path_ok("scripts/../../secret"));
    CHECK(!mtn_vfs_path_ok(".."));
    CHECK(mtn_vfs_path_ok("scripts/main.lua"));
    CHECK(mtn_vfs_path_ok("a.b/c.d"));
}

static void test_base(void) {
    mtn_vfs_init(NULL);
    CHECK_STR(mtn_vfs_base(), "");
    mtn_vfs_init("some/dir/");
    CHECK_STR(mtn_vfs_base(), "some/dir/");
    mtn_vfs_init(g_root);
    CHECK_STR(mtn_vfs_base(), g_root);
}

static void test_resolve(void) {
    char out[1024], want[1024];
    mtn_vfs_init(g_root);
    /* mods/ overrides assets/ */
    CHECK(mtn_vfs_resolve("scripts/main.lua", out, sizeof out));
    tmp_path(want, sizeof want, "mods/scripts/main.lua");
    CHECK_STR(out, want);
    /* falls back to assets/ */
    CHECK(mtn_vfs_resolve("scripts/only_base.lua", out, sizeof out));
    tmp_path(want, sizeof want, "assets/scripts/only_base.lua");
    CHECK_STR(out, want);
    /* a file that only exists as a mod */
    CHECK(mtn_vfs_resolve("only_mod.txt", out, sizeof out));
    tmp_path(want, sizeof want, "mods/only_mod.txt");
    CHECK_STR(out, want);
    /* missing: reports failure, leaves the assets/ candidate in out */
    CHECK(!mtn_vfs_resolve("scripts/missing.lua", out, sizeof out));
    tmp_path(want, sizeof want, "assets/scripts/missing.lua");
    CHECK_STR(out, want);
    /* unsafe paths never resolve */
    out[0] = 'x';
    out[1] = 0;
    CHECK(!mtn_vfs_resolve("../assets/scripts/main.lua", out, sizeof out));
    CHECK_STR(out, "x");
    CHECK(!mtn_vfs_resolve("/etc/hosts", out, sizeof out));
}

static void test_read(void) {
    mtn_vfs_init(g_root);
    size_t n = 99;
    char *s = mtn_vfs_read("scripts/main.lua", &n);
    CHECK(s != NULL);
    if (s) {
        CHECK_STR(s, "print('mod')");
        CHECK(n == strlen("print('mod')"));
    }
    free(s);
    s = mtn_vfs_read("scripts/only_base.lua", NULL);
    CHECK(s && strcmp(s, "base only") == 0);
    free(s);
    n = 99;
    s = mtn_vfs_read("empty.txt", &n);
    CHECK(s != NULL);
    CHECK(n == 0);
    CHECK(s && s[0] == 0);
    free(s);
    n = 99;
    CHECK(mtn_vfs_read("nothing/here.txt", &n) == NULL);
    CHECK(n == 99);
    CHECK(mtn_vfs_read("../escape.txt", NULL) == NULL);
    /* a different base no longer sees the files */
    mtn_vfs_init("/nonexistent-mountain-dir/");
    CHECK(mtn_vfs_read("scripts/main.lua", NULL) == NULL);
}

static void test_files(void) {
    char p[1024];
    tmp_path(p, sizeof p, "written.bin");
    CHECK(!mtn_file_exists(p));
    const unsigned char bin[6] = {0, 1, 2, 255, 0, 7};
    CHECK(mtn_file_write(p, bin, sizeof bin));
    CHECK(mtn_file_exists(p));
    size_t n = 0;
    char *d = mtn_file_read(p, &n);
    CHECK(d != NULL);
    CHECK(n == sizeof bin);
    CHECK(d && memcmp(d, bin, sizeof bin) == 0);
    CHECK(d && d[n] == 0); /* always NUL terminated */
    free(d);
    /* overwrite with shorter content */
    CHECK(mtn_file_write(p, "hi", 2));
    d = mtn_file_read(p, &n);
    CHECK(n == 2 && d && strcmp(d, "hi") == 0);
    free(d);
    /* failures */
    char bad[1024];
    tmp_path(bad, sizeof bad, "no_such_dir/file.txt");
    CHECK(!mtn_file_write(bad, "x", 1));
    CHECK(!mtn_file_exists(bad));
    n = 5;
    CHECK(mtn_file_read(bad, &n) == NULL);
    CHECK(n == 5);
}

#if defined(__linux__)
/* A pipe reopened through /proc cannot seek: ftell fails and the read is empty. */
static void test_unseekable(void) {
    int fds[2];
    if (pipe(fds) != 0) return;
    char p[64];
    snprintf(p, sizeof p, "/proc/self/fd/%d", fds[0]);
    CHECK(write(fds[1], "abc", 3) == 3);
    close(fds[1]);
    size_t n = 99;
    char *d = mtn_file_read(p, &n);
    if (d) {
        CHECK(n == 0);
        CHECK(d[0] == 0);
        free(d);
    }
    close(fds[0]);
}
#endif

MTN_SUITE(vfs) {
    char saved[512];
    snprintf(saved, sizeof saved, "%s", mtn_vfs_base());
    setup_tree();
    TEST_RUN(test_path_ok);
    TEST_RUN(test_base);
    TEST_RUN(test_resolve);
    TEST_RUN(test_read);
    TEST_RUN(test_files);
#if defined(__linux__)
    TEST_RUN(test_unseekable);
#endif
    teardown_tree();
    mtn_vfs_init(saved);
}
