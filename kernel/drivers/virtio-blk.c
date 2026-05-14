#define DEBUG 0
#include "kernel/drivers/virtio-blk.h"
#include "kernel/memory/page_allocator.h"
#include "kernel/panic.h"
#include "lib/printk/printk.h"

struct virtq_desc_t* base_virtq_desc = NULL;
struct virtq_avail_t* base_virtq_avail = NULL;
struct virtq_used_t* base_virtq_used = NULL;

static struct virtio_pci_common_cfg *common = NULL;
static uint64_t notify_addr = 0;
static struct virtio_blk_req *virtio_req_buf = NULL;
static uint8_t *virtio_status_buf = NULL;

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

    return 0;
}

static int virtio_blk_submit(uint32_t type, uint64_t sector, void *buf, uint32_t num_sectors) {
    if (base_virtq_desc == NULL || virtio_req_buf == NULL || notify_addr == 0) {
        panic("virtio_blk_submit: driver not initialized");
    }

    struct virtio_blk_req *req = virtio_req_buf;
    req->type     = type;
    req->reserved = 0;
    req->sector   = sector;

    uint8_t *status = virtio_status_buf;
    *status = 0xFF;

    /*
     * Descriptor table indices are not the same as avail->ring slots.
     * We use a fixed 3-descriptor chain (0→1→2) for each request; only the
     * avail ring index advances to queue the chain head for the device.
     */
    const uint16_t d0 = 0;
    const uint16_t d1 = 1;
    const uint16_t d2 = 2;

    /* descriptor 0: request header (device reads) */
    base_virtq_desc[d0].addr  = (uint64_t)VIRT_TO_PHYS(virtio_req_buf);
    base_virtq_desc[d0].len   = sizeof(struct virtio_blk_req);
    base_virtq_desc[d0].flags = VIRTQ_DESC_F_NEXT;
    base_virtq_desc[d0].next  = d1;

    /* descriptor 1: data buffer */
    base_virtq_desc[d1].addr  = (uint64_t)VIRT_TO_PHYS(buf);
    base_virtq_desc[d1].len   = num_sectors * SECTOR_BLOCK_SIZE;
    base_virtq_desc[d1].flags = VIRTQ_DESC_F_NEXT | (type == VIRTIO_BLK_T_IN ? VIRTQ_DESC_F_WRITE : 0);
    base_virtq_desc[d1].next  = d2;

    /* descriptor 2: status byte (device writes) */
    base_virtq_desc[d2].addr  = (uint64_t)VIRT_TO_PHYS(virtio_status_buf);
    base_virtq_desc[d2].len   = 1;
    base_virtq_desc[d2].flags = VIRTQ_DESC_F_WRITE;
    base_virtq_desc[d2].next  = 0;

    uint16_t last_used = base_virtq_used->idx;

    uint16_t avail_slot = (uint16_t)(base_virtq_avail->idx % QUEUE_SIZE);
    base_virtq_avail->ring[avail_slot] = d0;

    __sync_synchronize();
    base_virtq_avail->idx++;
    __sync_synchronize();

    debugk("Notifying device at 0x%llx\n", notify_addr);
    debugk("last_used=%u, avail->idx=%u\n", last_used, base_virtq_avail->idx);

    *(volatile uint32_t *)notify_addr = 0;

    uint64_t spin = 0;
    while (base_virtq_used->idx == last_used) {
        __sync_synchronize();
        if (++spin > 10000000ULL) {
            debugk("TIMEOUT: used->idx still %u after %llu spins\n", base_virtq_used->idx, spin);
            debugk("device_status = 0x%x\n", common->device_status);
            panic("virtio_blk: device did not respond");
        }
    }

    debugk("Device responded! used->idx=%u, status=0x%x\n", base_virtq_used->idx, *status);

    return (*status != VIRTIO_BLK_S_OK) ? -1 : 0;
}

int virtio_blk_read(uint64_t sector, void *buf, uint32_t num_sectors) {
    return virtio_blk_submit(VIRTIO_BLK_T_IN, sector, buf, num_sectors);
}

int virtio_blk_write(uint64_t sector, const void *buf, uint32_t num_sectors) {
    return virtio_blk_submit(VIRTIO_BLK_T_OUT, sector, (void *)buf, num_sectors);
}