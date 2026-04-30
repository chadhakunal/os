#ifndef TASK_H
#define TASK_H

typedef struct vma {
    uint64_t start_addr;
    uint64_t end_addr;
    uint64_t size;

    uint64_t permissions;
    struct vma* next;
} vma_t;



typedef struct mm_struct {
    vma_t* vma_head;
    page_table_t* root_page_table;
} mm_struct_t;

typedef struct task_struct {
    uint64_t task_id;

    mm_struct_t* mm;
} task_struct_t;


#endif