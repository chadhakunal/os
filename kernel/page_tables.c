/*
PTE:
|63........48|47................12|11........2|1|0|
|  reserved  |  physical address  |  flags    |T|V|


Virtual Address:
|63....48|47.....39|38....30|29....21|20...12|11...0|

*/

#include "types.h"
#include "platform.h"
#include "lib/string.h"
#include "kernel/drivers/uart.h"

#define NUM_PTE 512

#define PHYSICAL_MEMORY_START   0x00000000
#define PHYSICAL_MEMORY_END     0x80000000
#define PAGE_SIZE               0x00001000

#define PTE_VALID       (1ULL << 0)

/*
Level	    bits[1:0] = 0b01	                bits[1:0] = 0b11
L0/L1/L2	Block descriptor (large mapping)	Table descriptor (points to next level)
L3	        Reserved/fault	                    Page descriptor (4KB page)
*/
#define PTE_TABLE       (1ULL << 1)

#define PTE_ATTRIDX(x)  ((uint64_t)(x) << 2)

#define PTE_AP_KERNEL       (0ULL << 6) // EL1 read/write, EL0 no access
#define PTE_AP_USER         (1ULL << 6) // EL1 read/write, EL0 read/write
#define PTE_AP_KERNEL_RO    (2ULL << 6) // EL1 read-only, EL0 no access
#define PTE_AP_RO           (3ULL << 6) // EL1 read-only, EL0 read-only

#define PTE_SH_NONE         (0ULL << 8) // Memory is not shared across cores
#define PTE_SH_OUTER        (2ULL << 8) // Memory is shared across outer cache levels.
#define PTE_SH_INNER        (3ULL << 8) // Memory is shared across all cores in the same cluster and their caches stay coherent.

#define PTE_AF (1ULL << 10)
#define PTE_NG (1ULL << 11)

#define PTE_ADDR(x) ((uint64_t)(x) & 0x0000FFFFFFFFF000ULL)

#define PT0_OFFSET(x) (((uint64_t)(x) >> 39) & 0x1FF)
#define PT1_OFFSET(x) (((uint64_t)(x) >> 30) & 0x1FF)
#define PT2_OFFSET(x) (((uint64_t)(x) >> 21) & 0x1FF)
#define PT3_OFFSET(x) (((uint64_t)(x) >> 12) & 0x1FF)

typedef uint64_t pte_t;

typedef struct page_table {
    pte_t page_table_entries[NUM_PTE];
} page_table;

static int tmp_counter = 1;

page_table* allocate_page_table() {
    // Will be replaced later
    page_table* pt = (page_table*)((uint64_t)&_end + 0x1000 * tmp_counter);
    memset(pt, 0, PAGE_SIZE);
    tmp_counter++;
    return pt;
}

void* create_initial_page_table() {
    page_table* pt0;
    page_table* pt1;
    page_table* pt2;
    page_table* pt3;

    pt0 = allocate_page_table();

    for(uint64_t pa = PHYSICAL_MEMORY_START; pa < PHYSICAL_MEMORY_END; pa = pa + PAGE_SIZE) {
        uint64_t pt0_idx = PT0_OFFSET(pa);
        uint64_t pt1_idx = PT1_OFFSET(pa);
        uint64_t pt2_idx = PT2_OFFSET(pa);
        uint64_t pt3_idx = PT3_OFFSET(pa);

        if(pt0->page_table_entries[pt0_idx] == 0) {
            pt1 = allocate_page_table();
            pt0->page_table_entries[pt0_idx] = PTE_ADDR(pt1) | PTE_VALID | PTE_TABLE;
        } else {
            pt1 = (page_table*)(PTE_ADDR(pt0->page_table_entries[pt0_idx]));
        }

        if(pt1->page_table_entries[pt1_idx] == 0) {
            pt2 = allocate_page_table();
            pt1->page_table_entries[pt1_idx] = PTE_ADDR(pt2) | PTE_VALID | PTE_TABLE;
        } else {
            pt2 = (page_table*)(PTE_ADDR(pt1->page_table_entries[pt1_idx]));
        }

        if(pt2->page_table_entries[pt2_idx] == 0) {
            pt3 = allocate_page_table();
            pt2->page_table_entries[pt2_idx] = PTE_ADDR(pt3) | PTE_VALID | PTE_TABLE;
        } else {
            pt3 = (page_table*)(PTE_ADDR(pt2->page_table_entries[pt2_idx]));
        }

        pt3->page_table_entries[pt3_idx] = PTE_ADDR(pa) | PTE_VALID | PTE_TABLE | PTE_ATTRIDX(1) | PTE_AP_KERNEL | PTE_SH_INNER | PTE_AF;
    }

    uart_print("Initial PT created! \n");
    uart_print_hex((uint64_t)pt0);

    return pt0;
}
