#include "debuglib.h"
#include "define.h"
#include "vm.h"

#include <stdio.h>
#include <string.h>

void debug_int(u64 addr,u8 len){
    printf("[debug lib]: ");
    switch (len) {
        case TP_I8: //i8
            printf("%d",*(i8*)&addr);
            break;
        case TP_U8: //u8
            printf("%d",*(u8*)&addr);
            break;
        case TP_I16: //i16
            printf("%d",*(i16*)&addr);
            break;
        case TP_U16: //u16
            printf("%d",*(u16*)&addr);
            break;
        case TP_I32: //i8
            printf("%d",*(i32*)&addr);
            break;
        case TP_U32: //u8
            printf("%d",*(u32*)&addr);
            break;
        case TP_I64: //i16
            printf("%lld",*(i64*)&addr);
            break;
        case TP_U64: //u16
            printf("%llx",*(u64*)&addr);
            break;
    }
    printf("\n");
}

void debug_printstr(u64 val){
    printf("[debug libs]%s(%d)\n",(char*)val,strlen((char*)val));
}

native_func_t debuglibs[DBG_NUM] = {
    { (u64)&debug_int,"printnum"},
    {(u64)&debug_printstr,"printstr"},
    {(u64)printf,"printf"},
    {(u64)calloc,"calloc"},
    {(u64)free,"free"}
};