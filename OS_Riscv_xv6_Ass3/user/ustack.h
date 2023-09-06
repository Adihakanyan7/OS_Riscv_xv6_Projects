#include "kernel/types.h"

typedef
struct uheader {
    uint len;
    uint page_dealloc;
    struct uheader* prev;
}uheader;

void* ustack_malloc(uint len);
int ustack_free(void);