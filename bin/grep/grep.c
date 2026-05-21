#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <regex.h>

static int flag_n = 0;
static int flag_v = 0;
static int flag_l = 0;
static int flag_c = 0;
static int flag_r = 0;
static int flag_h = 0;
static int flag_H = 0;
static int cflags = REG_EXTENDED;

static regex_t re;
static int multi_file = 0;
static int found_any = 0;

static int grep_fd(int fd, const char *name) {
    char buf[4096];
    char line[4096];
    int linelen = 0;
    long lineno = 0;
    long match_count = 0;
    int matched_file = 0;
    ssize_t n;

    while ((n = read(fd, buf, sizeof(buf))) > 0) {
        for (ssize_t i = 0; i < n; i++) {
            char c = buf[i];
            if (linelen < (int)sizeof(line) - 1)
                line[linelen++] = c;
            if (c == '\n' || (i == n - 1 && linelen > 0)) {
                line[linelen] = '\0';
                lineno++;

                /* strip trailing newline before matching so $ anchors work */
                int matchlen = linelen;
                if (matchlen > 0 && line[matchlen - 1] == '\n')
                    line[--matchlen] = '\0';

                int m = (regexec(&re, line, 0, NULL, 0) == REG_OK);

                /* restore newline for printing */
                if (matchlen < linelen) line[matchlen] = '\n';
                if (flag_v) m = !m;

                if (m) {
                    found_any = 1;
                    match_count++;
                    if (flag_l) {
                        if (!matched_file) {
                            printf("%s\n", name);
                            matched_file = 1;
                        }
                        linelen = 0;
                        continue;
                    }
                    if (!flag_c) {
                        int show_name = (multi_file && !flag_h) || flag_H;
                        if (show_name) printf("%s:", name);
                        if (flag_n)   printf("%ld:", lineno);
                        fputs(line, stdout);
                        if (line[linelen - 1] != '\n') putchar('\n');
                    }
                }
                linelen = 0;
            }
        }
    }

    if (flag_c) {
        int show_name = (multi_file && !flag_h) || flag_H;
        if (show_name) printf("%s:", name);
        printf("%ld\n", match_count);
        if (match_count > 0) found_any = 1;
    }

    return 0;
}

static void grep_path(const char *path);

static void grep_dir(const char *path) {
    int fd = open(path, O_RDONLY);
    if (fd < 0) {
        fprintf(stderr, "grep: %s: cannot open\n", path);
        return;
    }

    struct dirent buf[32];
    int n;
    while ((n = getdents(fd, buf, 32)) > 0) {
        for (int i = 0; i < n; i++) {
            if (buf[i].d_name[0] == '\0') continue;
            if (buf[i].d_name[0] == '.' &&
                (buf[i].d_name[1] == '\0' ||
                 (buf[i].d_name[1] == '.' && buf[i].d_name[2] == '\0')))
                continue;

            char child[512];
            int plen = strlen(path);
            int nlen = strlen(buf[i].d_name);
            if (plen + nlen + 2 > (int)sizeof(child)) continue;
            memcpy(child, path, plen);
            if (child[plen - 1] != '/') child[plen++] = '/';
            memcpy(child + plen, buf[i].d_name, nlen + 1);
            grep_path(child);
        }
    }
    close(fd);
}

static void grep_path(const char *path) {
    struct stat st;
    if (stat(path, &st) < 0) {
        fprintf(stderr, "grep: %s: cannot stat\n", path);
        return;
    }

    if (S_ISDIR(st.st_mode)) {
        if (flag_r)
            grep_dir(path);
        else
            fprintf(stderr, "grep: %s: is a directory\n", path);
        return;
    }

    int fd = open(path, O_RDONLY);
    if (fd < 0) {
        fprintf(stderr, "grep: %s: cannot open\n", path);
        return;
    }
    grep_fd(fd, path);
    close(fd);
}

int main(int argc, char **argv) {
    int i;
    for (i = 1; i < argc && argv[i][0] == '-' && argv[i][1] != '\0'; i++) {
        if (argv[i][1] == '-' && argv[i][2] == '\0') { i++; break; }
        for (int j = 1; argv[i][j]; j++) {
            switch (argv[i][j]) {
            case 'n': flag_n = 1; break;
            case 'v': flag_v = 1; break;
            case 'i': cflags |= REG_ICASE; break;
            case 'l': flag_l = 1; break;
            case 'c': flag_c = 1; break;
            case 'r': flag_r = 1; break;
            case 'h': flag_h = 1; break;
            case 'H': flag_H = 1; break;
            case 'E': break; /* extended is already the default */
            case 'F': break; /* fixed strings not implemented */
            default:
                fprintf(stderr, "grep: unknown flag -%c\n", argv[i][j]);
                return 2;
            }
        }
    }

    if (i >= argc) {
        fprintf(stderr, "usage: grep [-nivlcrhHE] pattern [file...]\n");
        return 2;
    }

    const char *pattern = argv[i++];
    int err = regcomp(&re, pattern, cflags);
    if (err) {
        char errbuf[128];
        regerror(err, &re, errbuf, sizeof(errbuf));
        fprintf(stderr, "grep: bad pattern: %s\n", errbuf);
        return 2;
    }

    int nfiles = argc - i;
    multi_file = (nfiles > 1) || flag_r;

    if (nfiles == 0) {
        grep_fd(0, "(stdin)");
    } else {
        for (; i < argc; i++)
            grep_path(argv[i]);
    }

    regfree(&re);
    return found_any ? 0 : 1;
}
