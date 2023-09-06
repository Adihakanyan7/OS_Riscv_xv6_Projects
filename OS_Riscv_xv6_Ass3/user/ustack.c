#include "ustack.h"
#include "user.h"
#include "kernel/riscv.h"
#include "kernel/types.h"

struct uheader *uh = 0;

void* ustack_malloc(uint len){
    if(len > 512)
        return (void*)-1;
    uint64 ad;
    if(uh == 0){
        ad = (uint64)sbrk(4096);
        if(ad == -1)
            return (void*)-1; 
        uh = (struct uheader*) ad;
        uh->len = len;
        uh->page_dealloc = 4096 - len - sizeof(struct uheader);
        uh->prev = 0;
    }else {
        if(len + sizeof(struct uheader)> uh->page_dealloc){
            struct uheader nh;
            nh.len = len;
            nh.page_dealloc = 4096-(len - uh->page_dealloc)- sizeof(struct uheader);
            nh.prev = uh;
            uint64 addres = (uint64)sbrk(4096);
            if(addres == -1)
                return (void*)-1;
            uh = (struct uheader*)((uint64)uh + sizeof(struct uheader) + uh->len );
            uh->page_dealloc = nh.page_dealloc;
            uh->prev = nh.prev;
            uh->len = len;
        }
        else{
            struct uheader nh;
            nh.len = len;
            nh.page_dealloc = uh->page_dealloc - len - sizeof(struct uheader);
            nh.prev = uh;
            uh = (struct uheader*)(uh->len + sizeof(struct uheader)+(uint64)uh);
            uh->page_dealloc = nh.page_dealloc;
            uh->len = len;
            uh->prev = nh.prev;
        }
    }
    return (void*)uh + sizeof(struct uheader);
}

int ustack_free(void){
    struct uheader* t;
    if(uh == 0){
        return -1;
    }
    uint len = uh->len;
    t = uh->prev;
    if(uh->prev == 0)
        sbrk(-4096);    
    else if(PGROUNDDOWN((uint64)uh) == (uint64)uh)
        sbrk(-4096);
    else if(PGROUNDUP((uint64)uh) < (uint64)uh + sizeof(struct uheader) + uh->len)
        sbrk(-PGSIZE);
    uh = t;
    return len;    
}