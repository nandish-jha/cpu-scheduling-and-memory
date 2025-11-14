#define SBRK_EAGER 1
#define SBRK_LAZY  2

int cowfault(pagetable_t pagetable, uint64 va);