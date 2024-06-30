#include "mem.qh"
u64 malloc(u64 size){
    return calloc(1, size);
}