#ifndef TARFS_PARSER_H
#define TARFS_PARSER_H

#include "kernel/filesystem/tarfs/tarfs.h"

#define TAR_TYPE_FILE     '0'
#define TAR_TYPE_HARDLINK '1'
#define TAR_TYPE_SYMLINK  '2'
#define TAR_TYPE_CHARDEV  '3'
#define TAR_TYPE_BLOCKDEV '4'
#define TAR_TYPE_DIR      '5'
#define TAR_TYPE_FIFO     '6'

struct tar_header {
  char name[100];
  char mode[8];
  char uid[8];
  char gid[8];
  char size[12];
  char mtime[12];
  char checksum[8];
  char typeflag;
  char linkname[100];
  char magic[6];
  char version[2];
  char uname[32];
  char gname[32];
  char devmajor[8];
  char devminor[8];
  char prefix[155];
  char padding[12];
};

struct tarfs_vnode *parse_tar(void *data, uint64_t size);

static inline bool tar_is_dir(struct tar_header *header) {
  return header->typeflag == TAR_TYPE_DIR;
}

#endif
