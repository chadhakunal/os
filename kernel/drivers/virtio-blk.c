#define DEBUG 0
#include "kernel/drivers/virtio-blk.h"
#include "kernel/memory/page_allocator.h"
#include "kernel/task/task.h"
#include "kernel/task/schedule.h"
#include "lib/list.h"
#include "kernel/panic.h"
#include "lib/printk/printk.h"

struct virtq_desc_t* base_virtq_desc = NULL;
struct virtq_avail_t* base_virtq_avail = NULL;
struct virtq_used_t* base_virtq_used = NULL;

static struct virtio_pci_common_cfg *common = NULL;
static uint64_t notify_addr = 0;
static struct virtio_blk_req *virtio_req_buf = NULL;
static uint8_t *virtio_status_buf = NULL;

/* Request currently submitted to the virtio queue, or NULL. */
static struct disk_request_t *disk_current = NULL;
/* used->idx value we expect when the current request is done. */
static uint16_t virtio_expected_used_idx = 0;
/* Queue of pending requests not yet submitted. */
static struct list_node disk_request_queue;

int virtio_blk_init() {
    struct virtio_blk_device_config device_cfg  = *(struct virtio_blk_device_config *)platform.virtio_disk.device_cfg_mmio_reg;
    uint64_t sectors = device_cfg.capacity;
    uint64_t bytes = sectors * SECTOR_BLOCK_SIZE;

    printk("virtio-blk sectors: %llu\n", sectors);
    printk("virtio-blk size bytes: %llu\n", bytes);
    printk("virtio-blk size MiB: %llu\n", bytes / (1024 * 1024));

    common = (struct virtio_pci_common_cfg *)platform.virtio_disk.common_cfg_mmio_reg;
    
    common->device_status = VIRTIO_STATUS_RESET;
    
    // TODO: Fix Spin timeout
    while (common->device_status != 0) {
        // wait for reset complete
    }

    uint8_t status = 0;

    status |= VIRTIO_STATUS_ACKNOWLEDGE;
    common->device_status = status;

    status |= VIRTIO_STATUS_DRIVER;
    common->device_status = status;

    common->device_feature_select = 0;
    uint32_t features_lo = common->device_feature;

    common->device_feature_select = 1;
    uint32_t features_hi = common->device_feature;

    uint64_t features = ((uint64_t)features_hi << 32) | features_lo;
    printk("virtio device features = 0x%llx\n", features);

    uint64_t driver_features = (1ULL << VIRTIO_F_VERSION_1_FEATURE);
    common->driver_feature_select = 0;
    common->driver_feature = (uint32_t)driver_features;

    common->driver_feature_select = 1;
    common->driver_feature = (uint32_t)(driver_features >> 32);

    status |= VIRTIO_STATUS_FEATURES_OK;
    common->device_status = status;
    if (!(common->device_status & VIRTIO_STATUS_FEATURES_OK)) {
        // feature negotiation failed
        return -1;
    }

    common->queue_select = 0;
    uint16_t max_qsize = common->queue_size;
    printk("queue0 max size = %u\n", max_qsize);

    if (max_qsize < QUEUE_SIZE) {
        printk("queue too small\n");
        return -1;
    }

    common->queue_size = QUEUE_SIZE;

    void *desc_phys  = get_page(true);
    void *avail_phys = get_page(true);
    void *used_phys  = get_page(true);

    base_virtq_desc  = PHYS_TO_VIRT(desc_phys);
    base_virtq_avail = PHYS_TO_VIRT(avail_phys);
    base_virtq_used  = PHYS_TO_VIRT(used_phys);

    memset(base_virtq_desc,  0, DEFAULT_PAGE_SIZE);
    memset(base_virtq_avail, 0, DEFAULT_PAGE_SIZE);
    memset(base_virtq_used,  0, DEFAULT_PAGE_SIZE);

    common->queue_desc   = (uint64_t)desc_phys;
    common->queue_driver = (uint64_t)avail_phys;
    common->queue_device = (uint64_t)used_phys;

    common->queue_enable = 1;

    status |= VIRTIO_STATUS_DRIVER_OK;
    common->device_status = status;

    status = common->device_status;

    if (status & VIRTIO_STATUS_FAILED) {
        printk("virtio: device failed\n");
        return -1;
    }

    if (status & VIRTIO_STATUS_NEEDS_RESET) {
        printk("virtio: device needs reset\n");
        return -1;
    }

    printk("VIRTIO BLK Device Initialized!\n");
    
    notify_addr = platform.virtio_disk.notify_cfg_mmio_reg + common->queue_notify_off * platform.virtio_disk.notify_off_multiplier;

    /* Allocate a permanent page for the request header and status byte. */
    void *req_phys = get_page(true);
    virtio_req_buf    = PHYS_TO_VIRT(req_phys);
    virtio_status_buf = (uint8_t *)virtio_req_buf + sizeof(struct virtio_blk_req);

    list_init(&disk_request_queue);

    return 0;
}

/* Submit one request directly to the virtio hardware queue (no blocking). */
static void virtio_blk_hw_submit(struct disk_request_t *req) {
    struct virtio_blk_req *hdr = virtio_req_buf;
    hdr->type     = req->type;
    hdr->reserved = 0;
    hdr->sector   = req->sector;

    uint8_t *status = virtio_status_buf;
    *status = 0xFF;

    const uint16_t d0 = 0, d1 = 1, d2 = 2;

    base_virtq_desc[d0].addr  = (uint64_t)VIRT_TO_PHYS(virtio_req_buf);
    base_virtq_desc[d0].len   = sizeof(struct virtio_blk_req);
    base_virtq_desc[d0].flags = VIRTQ_DESC_F_NEXT;
    base_virtq_desc[d0].next  = d1;

    base_virtq_desc[d1].addr  = (uint64_t)VIRT_TO_PHYS(req->buf);
    base_virtq_desc[d1].len   = req->num_sectors * SECTOR_BLOCK_SIZE;
    base_virtq_desc[d1].flags = VIRTQ_DESC_F_NEXT |
                                 (req->type == VIRTIO_BLK_T_IN ? VIRTQ_DESC_F_WRITE : 0);
    base_virtq_desc[d1].next  = d2;

    base_virtq_desc[d2].addr  = (uint64_t)VIRT_TO_PHYS(virtio_status_buf);
    base_virtq_desc[d2].len   = 1;
    base_virtq_desc[d2].flags = VIRTQ_DESC_F_WRITE;
    base_virtq_desc[d2].next  = 0;

    virtio_expected_used_idx  = (uint16_t)(base_virtq_used->idx + 1);
    disk_current              = req;

    uint16_t avail_slot = (uint16_t)(base_virtq_avail->idx % QUEUE_SIZE);
    base_virtq_avail->ring[avail_slot] = d0;
    __sync_synchronize();
    base_virtq_avail->idx++;
    __sync_synchronize();
    *(volatile uint32_t *)notify_addr = 0;
}

static int virtio_blk_submit(uint32_t type, uint64_t sector, void *buf, uint32_t num_sectors) {
    if (base_virtq_desc == NULL || virtio_req_buf == NULL || notify_addr == 0) {
        panic("virtio_blk_submit: driver not initialized");
    }

    if (!scheduler_ready) {
        /* Early boot: no scheduler yet — submit directly and spin-wait. */
        struct disk_request_t boot_req = {
            .type = type, .sector = sector,
            .buf = buf, .num_sectors = num_sectors,
        };
        uint16_t last_used = base_virtq_used->idx;
        virtio_blk_hw_submit(&boot_req);
        uint64_t spin = 0;
        while (base_virtq_used->idx == last_used) {
            __sync_synchronize();
            if (++spin > 10000000ULL)
                panic("virtio_blk: device did not respond");
        }
        disk_current = NULL;
        return (*virtio_status_buf != VIRTIO_BLK_S_OK) ? -1 : 0;
    }

    /* Scheduler is live — create a request, enqueue it, and block. */
    struct disk_request_t *req = disk_request_t_alloc();
    req->type        = type;
    req->sector      = sector;
    req->buf         = buf;
    req->num_sectors = num_sectors;
    req->waiter      = current_task;
    req->result      = 0;

    /* If the disk is free, submit immediately; otherwise just enqueue. */
    if (disk_current == NULL)
        virtio_blk_hw_submit(req);
    else
        list_append(&disk_request_queue, &req->list);

    /* Block until poll() wakes us. */
    printk("[virtio] PID %llu blocking on I/O (sector=%llu)\n", current_task->pid, sector);
    current_task->state = TASK_BLOCKED;
    current_task->wait_reason = WAIT_IO;
    schedule();
    printk("[virtio] PID %llu woke from I/O\n", current_task->pid);

    int result = req->result;
    disk_request_t_free(req);
    return result;
}

void virtio_blk_cancel_task(struct task_t *task) {
    struct list_node *pos = disk_request_queue.next;
    while (pos != &disk_request_queue) {
        struct disk_request_t *req =
            container_of(pos, struct disk_request_t, list);
        pos = pos->next;
        if (req->waiter == task) {
            list_remove(&req->list);
            disk_request_t_free(req);
        }
    }
}

void virtio_blk_poll(void) {
    if (disk_current == NULL)
        return;

    __sync_synchronize();
    if (base_virtq_used->idx != virtio_expected_used_idx)
        return;

    /* Current request done — record result and wake the waiter. */
    struct disk_request_t *done = disk_current;
    disk_current = NULL;
    done->result = (*virtio_status_buf != VIRTIO_BLK_S_OK) ? -1 : 0;
    unblock_task(done->waiter);

    /* Start the next queued request if any. */
    if (!list_is_empty(&disk_request_queue)) {
        struct disk_request_t *next = container_of(
            disk_request_queue.next, struct disk_request_t, list);
        list_remove(&next->list);
        virtio_blk_hw_submit(next);
    }
}

int virtio_blk_read(uint64_t sector, void *buf, uint32_t num_sectors) {
    return virtio_blk_submit(VIRTIO_BLK_T_IN, sector, buf, num_sectors);
}

int virtio_blk_write(uint64_t sector, const void *buf, uint32_t num_sectors) {
    return virtio_blk_submit(VIRTIO_BLK_T_OUT, sector, (void *)buf, num_sectors);
}