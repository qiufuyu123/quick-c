#ifndef _H_DEBUGLIB
#define _H_DEBUGLIG


#include "define.h"

enum debug_lib{
    DBG_PRINT_INT,
     // require 2 arge e.g. print(u64 addr,u8 len)
     // which len is 1,2,4 or 8
    DBG_PRINT_STR,
    DBG_NUM
};

typedef struct __attribute__((packed, aligned(1))) {
    u64 addr;
    char *name;
}native_func_t;

extern native_func_t debuglibs[DBG_NUM];

#endif