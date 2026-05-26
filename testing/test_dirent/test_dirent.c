/*
 * Tests for opendir/readdir/rewinddir/seekdir/telldir/closedir.
 * Exercises both normal directories (tarfs) and procfs which doesn't
 * support lseek on the underlying vnode.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <errno.h>

static int passed = 0;
static int failed = 0;

static void result(const char *name, int ok) {
    if (ok) { printf("  [PASS] %s\n", name); passed++; }
    else     { printf("  [FAIL] %s\n", name); failed++; }
}

/* ------------------------------------------------------------------ */
/* Helpers                                                             */
/* ------------------------------------------------------------------ */

static int count_entries(DIR *d) {
    int n = 0;
    struct dirent *e;
    while ((e = readdir(d)) != NULL)
        n++;
    return n;
}

static int find_entry(DIR *d, const char *name) {
    struct dirent *e;
    while ((e = readdir(d)) != NULL)
        if (strcmp(e->d_name, name) == 0)
            return 1;
    return 0;
}

/* ------------------------------------------------------------------ */
/* 1. Basic opendir / readdir / closedir on a known directory          */
/* ------------------------------------------------------------------ */
static void test_basic_readdir(void) {
    DIR *d = opendir("/bin");
    result("opendir /bin succeeds", d != NULL);
    if (!d) return;

    int n = count_entries(d);
    result("readdir /bin returns entries", n > 0);
    closedir(d);
}

/* ------------------------------------------------------------------ */
/* 2. rewinddir restarts iteration from the beginning                  */
/* ------------------------------------------------------------------ */
static void test_rewinddir(void) {
    DIR *d = opendir("/bin");
    if (!d) { result("rewinddir: opendir", 0); return; }

    int first_pass = count_entries(d);
    result("rewinddir: first pass has entries", first_pass > 0);

    rewinddir(d);

    int second_pass = count_entries(d);
    result("rewinddir: second pass same count", second_pass == first_pass);

    closedir(d);
}

/* ------------------------------------------------------------------ */
/* 3. rewinddir on /proc (no lseek support on underlying fs)           */
/* ------------------------------------------------------------------ */
static void test_rewinddir_proc(void) {
    DIR *d = opendir("/proc");
    if (!d) { result("rewinddir /proc: opendir", 0); return; }

    int first_pass = count_entries(d);
    result("rewinddir /proc: first pass has entries", first_pass > 0);

    rewinddir(d);

    int second_pass = count_entries(d);
    result("rewinddir /proc: second pass same count", second_pass == first_pass);

    closedir(d);
}

/* ------------------------------------------------------------------ */
/* 4. telldir / seekdir round-trip                                     */
/* ------------------------------------------------------------------ */
static void test_seekdir(void) {
    DIR *d = opendir("/bin");
    if (!d) { result("seekdir: opendir", 0); return; }

    /* Read one entry, record position, read another, seek back, verify. */
    struct dirent *e1 = readdir(d);
    result("seekdir: first entry exists", e1 != NULL);
    if (!e1) { closedir(d); return; }
    char name1[256];
    strncpy(name1, e1->d_name, sizeof(name1) - 1);
    name1[sizeof(name1)-1] = '\0';

    long pos_after_first = telldir(d);

    struct dirent *e2 = readdir(d);
    result("seekdir: second entry exists", e2 != NULL);
    if (!e2) { closedir(d); return; }
    char name2[256];
    strncpy(name2, e2->d_name, sizeof(name2) - 1);
    name2[sizeof(name2)-1] = '\0';

    /* Seek back to just after the first entry and read again. */
    seekdir(d, pos_after_first);
    struct dirent *e2b = readdir(d);
    result("seekdir: entry after seek matches original", e2b != NULL && strcmp(e2b->d_name, name2) == 0);

    closedir(d);
}

/* ------------------------------------------------------------------ */
/* 5. telldir at start == 0, advances after each readdir               */
/* ------------------------------------------------------------------ */
static void test_telldir(void) {
    DIR *d = opendir("/bin");
    if (!d) { result("telldir: opendir", 0); return; }

    long pos0 = telldir(d);
    result("telldir at start is 0", pos0 == 0);

    readdir(d);
    long pos1 = telldir(d);
    result("telldir advances after readdir", pos1 > pos0);

    readdir(d);
    long pos2 = telldir(d);
    result("telldir advances again", pos2 > pos1);

    closedir(d);
}

/* ------------------------------------------------------------------ */
/* 6. rewinddir then find a specific entry                             */
/* ------------------------------------------------------------------ */
static void test_rewinddir_find(void) {
    DIR *d = opendir("/bin");
    if (!d) { result("rewinddir find: opendir", 0); return; }

    /* Drain to find any one real entry name. */
    char target[256] = {0};
    struct dirent *e;
    while ((e = readdir(d)) != NULL) {
        if (strcmp(e->d_name, ".") != 0 && strcmp(e->d_name, "..") != 0) {
            strncpy(target, e->d_name, sizeof(target) - 1);
            break;
        }
    }
    result("rewinddir find: found a target entry", target[0] != '\0');

    rewinddir(d);
    int found = find_entry(d, target);
    result("rewinddir find: entry found after rewind", found);

    closedir(d);
}

/* ------------------------------------------------------------------ */
/* 7. readdir exhaustion returns NULL without setting errno            */
/* ------------------------------------------------------------------ */
static void test_readdir_eof(void) {
    DIR *d = opendir("/proc/1");
    if (!d) { result("readdir eof: opendir /proc/1", 0); return; }

    errno = 0;
    while (readdir(d) != NULL)
        ;

    result("readdir eof: returns NULL at end", errno == 0);
    closedir(d);
}

int main(void) {
    printf("\n=== dirent Tests ===\n\n");

    printf("Basic readdir:\n");
    test_basic_readdir();

    printf("\nrewinddir:\n");
    test_rewinddir();
    test_rewinddir_proc();
    test_rewinddir_find();

    printf("\ntelldir / seekdir:\n");
    test_telldir();
    test_seekdir();

    printf("\nreaddir EOF:\n");
    test_readdir_eof();

    printf("\n=== Summary: %d passed, %d failed ===\n\n", passed, failed);
    return failed > 0 ? 1 : 0;
}
