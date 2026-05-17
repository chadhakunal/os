#define DEBUG 0
#include "kernel/task/executable_loader.h"
#include "kernel/task/elf_loader.h"
#include "kernel/task/script_loader.h"
#include "kernel/filesystem/vfs/vfs.h"
#include "lib/printk/printk.h"
#include "lib/string.h"

int detect_file_type(struct dentry_t *dentry) {
    debugk("detect_file_type: dentry=%p\n", dentry);

    // Read first 4 bytes to check magic numbers
    uint8_t header[4];
    int32_t ret = vfs_vnode_read(dentry->vnode, header, 4, 0);
    if (ret < 4) {
        debugk("detect_file_type: failed to read header\n");
        return FILE_TYPE_INVALID;
    }

    // Check for ELF magic: 0x7F 'E' 'L' 'F'
    if (header[0] == 0x7F && header[1] == 'E' &&
        header[2] == 'L' && header[3] == 'F') {
        debugk("detect_file_type: ELF detected\n");
        return FILE_TYPE_ELF;
    }

    // Check for shebang: '#!'
    if (header[0] == '#' && header[1] == '!') {
        debugk("detect_file_type: Script detected\n");
        return FILE_TYPE_SCRIPT;
    }

    debugk("detect_file_type: Unknown file type\n");
    return FILE_TYPE_INVALID;
}

int load_executable_dentry(struct task_t *task, struct execve_args_t *args,
                           struct dentry_t *initial_dentry) {
    debugk("load_executable: path=%s\n", args->pathname);

    int depth = 0;

    while (depth < MAX_SCRIPT_DEPTH) {
        struct dentry_t *dentry;
        int32_t ret;

        if (initial_dentry != NULL && depth == 0) {
            dentry = initial_dentry;
        } else {
            ret = vfs_resolve_path(args->pathname, &dentry);
            if (ret != 0) {
                debugk("load_executable: failed to resolve path %s\n", args->pathname);
                return -1;
            }
        }

        int file_type = detect_file_type(dentry);

        if (file_type == FILE_TYPE_ELF) {
            debugk("load_executable: Loading ELF %s\n", args->pathname);
            return load_elf_dentry(task, dentry);
        } else if (file_type == FILE_TYPE_SCRIPT) {
            debugk("load_executable: Processing script %s (depth=%d)\n", args->pathname, depth);
            ret = load_script(task, dentry, args);
            if (ret != 0) {
                debugk("load_executable: load_script failed\n");
                return -1;
            }
            // args has been modified by load_script, loop again with new pathname
            depth++;
        } else {
            debugk("load_executable: Invalid file type\n");
            return -1;
        }
    }

    debugk("load_executable: Max script depth exceeded\n");
    return -1;
}

int load_executable(struct task_t *task, struct execve_args_t *args) {
    return load_executable_dentry(task, args, NULL);
}


