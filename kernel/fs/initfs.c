#include "types.h"
#include "lib/printk/printk.h"
#include "lib/string.h"

#define TAR_HEADER_SIZE 512

extern char _tarfs_start[];
extern char _tarfs_end[];
extern char _tarfs_size[];

typedef struct tar_header {
    char name[100];
    char mode[8];
    char uid[8];
    char gid[8];
    char size[12];
    char mtime[12];
    char chksum[8];
    char typeflag;
} tar_header_t;


uint64_t parse_size(char* size_str) {
    uint64_t size = 0;
    for(int i = 0; i < 11 && size_str[i]; i++) {
        size = size * 8 + (size_str[i] - '0');
    }
    return size;
}

void init_fs() {
    printk("Tar FS Start: %x\n", _tarfs_start);
    printk("Tar FS End: %x\n", _tarfs_end);
    printk("Tar FS Size: %x\n", _tarfs_size);

    char* p = (void*)_tarfs_start;

    while(p <= _tarfs_end) {
        tar_header_t* hdr = (tar_header_t*)p;
        
        if(hdr->name[0] == '\0') break;

        uint64_t size = parse_size(hdr->size);
        uint64_t aligned_size = (size + 511) & ~511;

        char *data = p + TAR_HEADER_SIZE;

        printk("FILE NAME: %s\n", hdr->name);

        if(strncmp(hdr->name, "./bin/init") == 0) {
            printk("Init File Magic: %x\n", data[0]);
            printk("%c\n", data[1]);
            printk("%c\n", data[2]);
            printk("%c\n", data[3]);
        }

        p += TAR_HEADER_SIZE + aligned_size;
    }
}
