#ifndef SCRIPT_LOADER_H
#define SCRIPT_LOADER_H

#include "types.h"
#include "kernel/task/task.h"
#include "kernel/filesystem/vfs/vfs.h"
#include "kernel/task/executable_loader.h"

/* Load and execute a script with shebang */
int load_script(struct task_t *task, struct dentry_t *dentry, struct execve_args_t *args);

#endif
