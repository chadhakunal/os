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

#define PERM_IS_DIR 1000000 /* The vnode is a directory */

#define IS_DIR(mode) ((mode & PERM_IS_DIR) != 0)

#endif
