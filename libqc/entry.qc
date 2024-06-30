
/*
    libqc main
*/

#include "io.qh"
#include "mem.qh"

extern u8 main();

typedef struct {
    u64 addr;
    u8 *cstr_name;
}_libqc_func;


u8 _start_(_libqc_func* list,u8 cnt){
    printnum = list[0].addr;
    printstr = list[1].addr;
    printf = list[2].addr;
    calloc = list[3].addr;
    free = list[4].addr;
    
    return main();
}

