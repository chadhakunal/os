#ifndef EXECUTABLE_LOADER_H
#define EXECUTABLE_LOADER_H

#include "types.h"
#include "kernel/task/task.h"
#include "kernel/filesystem/vfs/vfs.h"

/* File type detection results */
#define FILE_TYPE_INVALID 0
#define FILE_TYPE_ELF     1
#define FILE_TYPE_SCRIPT  2

/* Shebang constants */
#define SHEBANG_MAX_LEN 128

/* Execve arguments */
#define MAX_ARG_COUNT 16
#define MAX_ARG_LEN 64
#define MAX_PATH_LEN 128

#define MAX_SCRIPT_DEPTH 4

// Structure for execve arguments - must fit in one page (4096 bytes)
// Size: 128 + 8 + (16*64) + (16*64) = 128 + 8 + 1024 + 1024 = 2184 bytes
struct execve_args_t {
  char pathname[MAX_PATH_LEN];
  int argc;
  int envc;
  char argv[MAX_ARG_COUNT][MAX_ARG_LEN];
  char envp[MAX_ARG_COUNT][MAX_ARG_LEN];
};

/* Main entry point for loading any executable (ELF or script) with args struct */
int load_executable(struct task_t *task, struct execve_args_t *args);

/* Detect file type by reading magic bytes from dentry */
int detect_file_type(struct dentry_t *dentry);

#endif
