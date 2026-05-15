#define DEBUG 0
#include "kernel/task/script_loader.h"
#include "kernel/task/elf_loader.h"
#include "kernel/task/executable_loader.h"
#include "kernel/filesystem/vfs/vfs.h"
#include "lib/printk/printk.h"
#include "lib/string.h"

int load_script(struct task_t *task, struct dentry_t *dentry, struct execve_args_t *args) {
    debugk("load_script: path=%s\n", args->pathname);

    // Read shebang line (first 128 bytes)
    char shebang_line[SHEBANG_MAX_LEN];
    int32_t ret;
    ret = vfs_vnode_read(dentry->vnode, shebang_line, SHEBANG_MAX_LEN - 1, 0);
    if (ret < 2) {
        debugk("load_script: failed to read shebang\n");
        return -1;
    }
    shebang_line[ret] = '\0';

    // Verify it starts with #!
    if (shebang_line[0] != '#' || shebang_line[1] != '!') {
        debugk("load_script: invalid shebang\n");
        return -1;
    }

    // Find the end of the first line
    char *newline = shebang_line;
    while (*newline != '\0' && *newline != '\n' && *newline != '\r') {
        newline++;
    }
    *newline = '\0';

    // Parse interpreter path (skip the #!)
    char *interpreter = shebang_line + 2;

    // Skip leading whitespace
    while (*interpreter == ' ' || *interpreter == '\t') {
        interpreter++;
    }

    if (*interpreter == '\0') {
        debugk("load_script: empty interpreter path\n");
        return -1;
    }

    // Find the end of the interpreter path (space or end of string)
    char *interpreter_end = interpreter;
    while (*interpreter_end != '\0' && *interpreter_end != ' ' && *interpreter_end != '\t') {
        interpreter_end++;
    }
    *interpreter_end = '\0';

    debugk("load_script: interpreter=%s, script=%s\n", interpreter, args->pathname);

    // Rewrite argv in the args struct: shift existing args right and insert script path
    // New layout:
    // args->argv[0] = interpreter path
    // args->argv[1] = script path
    // args->argv[2..n] = original args->argv[1..n-1]

    int original_argc = args->argc;

    // Check if we have room for one more argument (script path)
    if (original_argc + 1 >= MAX_ARG_COUNT) {
        debugk("load_script: too many arguments\n");
        return -1;
    }

    // Shift original args to make room (work backwards to avoid overwriting)
    for (int i = original_argc - 1; i >= 1; i--) {
        strncpy(args->argv[i + 1], args->argv[i], MAX_ARG_LEN);
    }

    // Set up new argv[0] = interpreter, argv[1] = script path
    strncpy(args->argv[0], interpreter, MAX_ARG_LEN);
    strncpy(args->argv[1], args->pathname, MAX_ARG_LEN);

    // Update argc and pathname to point to interpreter
    args->argc = original_argc + 1;
    strncpy(args->pathname, interpreter, MAX_PATH_LEN);

    // The caller (execve or load_executable) will handle loading the interpreter
    return 0;
}
