#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#define FS_MAGIC      0x53464253  // "SBFS"
#define INODE_COUNT   1024
#define FS_BLOCK_SIZE 512

#define INODE_FREE 0
#define INODE_FILE 1
#define INODE_DIR  2

typedef struct superblock {
  uint32_t magic;

  uint32_t block_size;
  uint32_t total_blocks;

  uint32_t inode_count;
  uint32_t inode_size;

  uint32_t inode_bitmap_start;
  uint32_t inode_bitmap_num_blocks;

  uint32_t block_bitmap_start;
  uint32_t block_bitmap_num_blocks;

  uint32_t inode_table_start;
  uint32_t inode_table_num_blocks;

  uint32_t data_start;
  uint32_t root_inode;

  uint8_t reserved[512 - 13 * 4];
} superblock_t;

typedef struct inode {
  uint16_t type;
  uint16_t nlinks;
  uint32_t size;              // file size in bytes

  uint32_t direct_blocks[12];
  uint32_t indirect_block;

  uint32_t reserved;
} inode_t;

_Static_assert(sizeof(superblock_t) == 512, "superblock must be 512 bytes");
_Static_assert(sizeof(inode_t) == 64, "inode must be 64 bytes");

static uint32_t div_round_up(uint32_t x, uint32_t y) {
  return (x + y - 1) / y;
}

int main(int argc, char **argv) {
  if (argc != 3) {
    fprintf(stderr, "usage: mkfs <image> <size_mb>\n");
    return 1;
  }

  long mb = atol(argv[2]);
  if (mb <= 0) {
    fprintf(stderr, "invalid size\n");
    return 1;
  }

  FILE *f = fopen(argv[1], "wb");
  if (!f) {
    perror(argv[1]);
    return 1;
  }

  uint64_t disk_size_bytes = (uint64_t)mb * 1024 * 1024;
  uint32_t total_blocks = disk_size_bytes / FS_BLOCK_SIZE;

  uint32_t inode_bitmap_blocks = div_round_up(INODE_COUNT, FS_BLOCK_SIZE * 8);
  uint32_t block_bitmap_blocks = div_round_up(total_blocks, FS_BLOCK_SIZE * 8);
  uint32_t inode_table_blocks = div_round_up(INODE_COUNT * sizeof(inode_t), FS_BLOCK_SIZE);

  superblock_t superblock;
  memset(&superblock, 0, sizeof(superblock));

  superblock.magic = FS_MAGIC;
  superblock.block_size = FS_BLOCK_SIZE;
  superblock.total_blocks = total_blocks;

  superblock.inode_count = INODE_COUNT;
  superblock.inode_size = sizeof(inode_t);

  superblock.inode_bitmap_start = 1;
  superblock.inode_bitmap_num_blocks = inode_bitmap_blocks;

  superblock.block_bitmap_start = superblock.inode_bitmap_start + superblock.inode_bitmap_num_blocks;
  superblock.block_bitmap_num_blocks = block_bitmap_blocks;

  superblock.inode_table_start = superblock.block_bitmap_start + superblock.block_bitmap_num_blocks;
  superblock.inode_table_num_blocks = inode_table_blocks;

  superblock.data_start = superblock.inode_table_start + superblock.inode_table_num_blocks;

  superblock.root_inode = 0;

  if (superblock.data_start >= total_blocks) {
    fprintf(stderr, "disk too small for filesystem metadata\n");
    fclose(f);
    return 1;
  }

  // Create sparse disk image of requested size.
  if (fseek(f, disk_size_bytes - 1, SEEK_SET) != 0) {
    perror("fseek");
    fclose(f);
    return 1;
  }

  if (fputc(0, f) == EOF) {
    perror("fputc");
    fclose(f);
    return 1;
  }

  // Write superblock at block 0.
  if (fseek(f, 0, SEEK_SET) != 0) {
    perror("fseek");
    fclose(f);
    return 1;
  }

  if (fwrite(&superblock, sizeof(superblock), 1, f) != 1) {
    perror("fwrite superblock");
    fclose(f);
    return 1;
  }

  uint8_t inode_bitmap[FS_BLOCK_SIZE];
  memset(inode_bitmap, 0, sizeof(inode_bitmap));
  inode_bitmap[0] |= 1 << 0;

  fseek(f, superblock.inode_bitmap_start * FS_BLOCK_SIZE, SEEK_SET);
  fwrite(inode_bitmap, FS_BLOCK_SIZE, 1, f);

  uint32_t block_bitmap_bytes = superblock.block_bitmap_num_blocks * FS_BLOCK_SIZE;
  uint8_t *block_bitmap = malloc(block_bitmap_bytes);
  if (!block_bitmap) {
    perror("malloc block_bitmap");
    fclose(f);
    return 1;
  }
  memset(block_bitmap, 0, block_bitmap_bytes);
  
  for(uint32_t b = 0; b < superblock.data_start; b++) {
    uint32_t byte = b / 8;
    uint32_t bit = b % 8;
    block_bitmap[byte] |= 1 << bit;
  }

  fseek(f, superblock.block_bitmap_start * FS_BLOCK_SIZE, SEEK_SET);
  fwrite(block_bitmap, block_bitmap_bytes, 1, f);

  uint64_t inode_table_offset = (uint64_t)superblock.inode_table_start * FS_BLOCK_SIZE;

  const char *test_content = "Hello from sbfs!\n";
  uint32_t test_content_len = strlen(test_content);

  /* allocate inode 1 for the test file */
  inode_bitmap[0] |= 1 << 1;
  fseek(f, superblock.inode_bitmap_start * FS_BLOCK_SIZE, SEEK_SET);
  fwrite(inode_bitmap, FS_BLOCK_SIZE, 1, f);

  /* allocate two data blocks: data_start+0 for file content, data_start+1 for root dir */
  uint32_t file_data_block = superblock.data_start;
  uint32_t root_dir_block  = superblock.data_start + 1;
  block_bitmap[file_data_block / 8] |= 1 << (file_data_block % 8);
  block_bitmap[root_dir_block  / 8] |= 1 << (root_dir_block  % 8);
  fseek(f, superblock.block_bitmap_start * FS_BLOCK_SIZE, SEEK_SET);
  fwrite(block_bitmap, block_bitmap_bytes, 1, f);

  /* write file content into file_data_block */
  uint8_t data_block[FS_BLOCK_SIZE];
  memset(data_block, 0, sizeof(data_block));
  memcpy(data_block, test_content, test_content_len);
  fseek(f, (uint64_t)file_data_block * FS_BLOCK_SIZE, SEEK_SET);
  fwrite(data_block, FS_BLOCK_SIZE, 1, f);

  /* write inode 1: the test file */
  inode_t file_inode;
  memset(&file_inode, 0, sizeof(file_inode));
  file_inode.type = INODE_FILE;
  file_inode.nlinks = 1;
  file_inode.size = test_content_len;
  file_inode.direct_blocks[0] = file_data_block;
  fseek(f, inode_table_offset + sizeof(inode_t), SEEK_SET);
  fwrite(&file_inode, sizeof(file_inode), 1, f);

  /* write root dir block: one dirent pointing at inode 1 */
  typedef struct { uint32_t inode_num; char name[28]; } dirent_t;
  uint8_t dir_block[FS_BLOCK_SIZE];
  memset(dir_block, 0, sizeof(dir_block));
  dirent_t *de = (dirent_t *)dir_block;
  de->inode_num = 1;
  strncpy(de->name, "hello.txt", sizeof(de->name) - 1);
  fseek(f, (uint64_t)root_dir_block * FS_BLOCK_SIZE, SEEK_SET);
  fwrite(dir_block, FS_BLOCK_SIZE, 1, f);

  /* write root inode with size and dir block pointer */
  inode_t root;
  memset(&root, 0, sizeof(root));
  root.type = INODE_DIR;
  root.nlinks = 1;
  root.size = sizeof(dirent_t);
  root.direct_blocks[0] = root_dir_block;
  fseek(f, inode_table_offset, SEEK_SET);
  fwrite(&root, sizeof(root), 1, f);

  fclose(f);
  free(block_bitmap);
  return 0;
}