#ifndef MODE_H
#define MODE_H

#include "types.h"

typedef uint32_t mode_t;

/* Owner permissions */
#define PERM_RUSR  0000400  /* Read permission, owner */
#define PERM_WUSR  0000200  /* Write permission, owner */
#define PERM_XUSR  0000100  /* Execute permission, owner */
#define PERM_RWXU  0000700  /* Read, write, execute - owner */

/* Group permissions */
#define PERM_RGRP  0000040  /* Read permission, group */
#define PERM_WGRP  0000020  /* Write permission, group */
#define PERM_XGRP  0000010  /* Execute permission, group */
#define PERM_RWXG  0000070  /* Read, write, execute - group */

/* Other permissions */
#define PERM_ROTH  0000004  /* Read permission, other */
#define PERM_WOTH  0000002  /* Write permission, other */
#define PERM_XOTH  0000001  /* Execute permission, other */
#define PERM_RWXO  0000007  /* Read, write, execute - other */

/* File type bits (upper bits of mode) */
#define S_IFMT   0170000  /* File type mask */
#define S_IFREG  0100000  /* Regular file */
#define S_IFDIR  0040000  /* Directory */
#define S_IFCHR  0020000  /* Character device */
#define S_IFBLK  0060000  /* Block device */
#define S_IFIFO  0010000  /* FIFO/pipe */
#define S_IFLNK  0120000  /* Symbolic link */

/* File type test macros */
#define IS_REG(mode)  (((mode) & S_IFMT) == S_IFREG)   /* Regular file */
#define IS_DIR(mode)  (((mode) & S_IFMT) == S_IFDIR)   /* Directory */
#define IS_CHR(mode)  (((mode) & S_IFMT) == S_IFCHR)   /* Character device */
#define IS_BLK(mode)  (((mode) & S_IFMT) == S_IFBLK)   /* Block device */
#define IS_FIFO(mode) (((mode) & S_IFMT) == S_IFIFO)   /* FIFO/pipe */
#define IS_LNK(mode)  (((mode) & S_IFMT) == S_IFLNK)   /* Symbolic link */

#endif
