#ifndef _H_DEBUGLIB
#define _H_DEBUGLIG


#include "define.h"

enum debug_lib{
    DBG_PRINT_INT,
     // require 2 arge e.g. print(u64 addr,u8 len)
     // which len is 1,2,4 or 8
    DBG_PRINT_STR
};

extern u64 debuglibs[2];

#endif