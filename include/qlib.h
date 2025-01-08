#include "define.h"

#pragma pack(1)
typedef struct{
    u8 magic[4]; // Q L I B
    u8 type;
    u8 version;

    u64 code_base;
    u64 data_base;
    u64 str_base;

    u64 main_entry; 
    u32 reloc_table_offset;
    u32 reloc_num;
    u32 code_offset;
    u32 code_sz;
    u32 str_offset;
    u32 str_sz;
    u32 data_sz;
    u8 compile_info[64];
}qlib_exe_t;

typedef struct{
    
}qlib_jit_runtime_t;

#pragma pack()