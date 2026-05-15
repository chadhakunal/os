#ifndef KERNEL_DRIVERS_VIRTIO_BLK
#define KERNEL_DRIVERS_VIRTIO_BLK

#include "types.h"
#include "platform.h"
#include "lib/string.h"
#include "lib/printk/printk.h"
#include "lib/list.h"
#include "arch/riscv64/virtual_memory_init.h"
#include "lib/pool_allocator.h"

#define SECTOR_BLOCK_SIZE 512

#define VIRTIO_STATUS_RESET        0
#define VIRTIO_STATUS_ACKNOWLEDGE  1
#define VIRTIO_STATUS_DRIVER       2
#define VIRTIO_STATUS_DRIVER_OK    4
#define VIRTIO_STATUS_FEATURES_OK  8
#define VIRTIO_STATUS_NEEDS_RESET  64
#define VIRTIO_STATUS_FAILED       128

#define VIRTIO_F_VERSION_1_FEATURE 32

#define VIRTQ_DESC_F_NEXT  1
#define VIRTQ_DESC_F_WRITE 2

#define VIRTIO_BLK_T_IN   0   // read from disk
#define VIRTIO_BLK_T_OUT  1   // write to disk

#define VIRTIO_BLK_S_OK   0

#define QUEUE_SIZE 16

extern struct virtq_desc_t* base_virtq_desc;
extern struct virtq_avail_t* base_virtq_avail;
extern volatile struct virtq_used_t* base_virtq_used;

struct virtio_blk_req {
    uint32_t type;
    uint32_t reserved;
    uint64_t sector;
};

struct virtio_pci_common_cfg {
    volatile uint32_t device_feature_select; // offset 0x00
    volatile uint32_t device_feature;        // offset 0x04

    volatile uint32_t driver_feature_select; // offset 0x08
    volatile uint32_t driver_feature;        // offset 0x0c

    volatile uint16_t msix_config;           // offset 0x10
    volatile uint16_t num_queues;            // offset 0x12

    volatile uint8_t  device_status;         // offset 0x14
    volatile uint8_t  config_generation;     // offset 0x15

    volatile uint16_t queue_select;          // offset 0x16
    volatile uint16_t queue_size;            // offset 0x18
    volatile uint16_t queue_msix_vector;     // offset 0x1a
    volatile uint16_t queue_enable;          // offset 0x1c
    volatile uint16_t queue_notify_off;      // offset 0x1e

    volatile uint64_t queue_desc;            // offset 0x20
    volatile uint64_t queue_driver;          // offset 0x28
    volatile uint64_t queue_device;          // offset 0x30
};

struct virtio_blk_device_config {
    volatile uint64_t capacity;   // number of 512-byte sectors
};

struct virtq_desc_t {
    uint64_t addr;
    uint32_t len;
    uint16_t flags;
    uint16_t next;
};

struct virtq_avail_t {
    uint16_t flags;
    uint16_t idx;
    uint16_t ring[QUEUE_SIZE];
};

struct virtq_used_elem {
    uint32_t id;
    uint32_t len;
};

struct virtq_used_t {
    uint16_t flags;
    uint16_t idx;
    struct virtq_used_elem ring[QUEUE_SIZE];
};

/* Per-request descriptor queued by callers. */
struct disk_request_t {
  uint32_t         type;         /* VIRTIO_BLK_T_IN or VIRTIO_BLK_T_OUT */
  uint64_t         sector;
  void            *buf;
  uint32_t         num_sectors;
  struct task_t   *waiter;       /* task to unblock on completion */
  int              result;       /* 0 = ok, -1 = error (set by poll) */
  struct list_node list;
};

DEFINE_POOL(disk_request_t, struct disk_request_t)

int virtio_blk_init();
int virtio_blk_read(uint64_t sector, void *buf, uint32_t num_sectors);
int virtio_blk_write(uint64_t sector, const void *buf, uint32_t num_sectors);

/* Called from the timer handler each tick to check if a pending I/O completed,
 * unblock the waiter, and start the next queued request. */
void virtio_blk_poll(void);

/* Remove any queued requests belonging to `task` (called on task exit). */
void virtio_blk_cancel_task(struct task_t *task);

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

#define FS_MAGIC      0x53464253  // "SBFS"

#endif
