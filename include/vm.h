#ifndef _H_VM
#define _H_VM


#define  QCARCH_X86
#include "define.h"
#include "vec.h"
#include "hashmap.h"
#include <sys/types.h>
enum{
    TP_I8,TP_U8,TP_I16,TP_U16,TP_I32,TP_U32,TP_I64,TP_U64,TP_INTEGER,
    TP_FLOAT,TP_FUNC,TP_CUSTOM,TP_RELOC,TP_UNK
};

enum{
    UOP_NOT,UOP_NEG,UOP_INC,UOP_DEC,
    BOP_CMP,BOP_OR,BOP_AND,BOP_XOR
};

enum{
    REG_AX,REG_CX,REG_DX,REG_BX,REG_SP,REG_BP,REG_SI,REG_DI,REG_R8,
    REG_R9,REG_R10,REG_R11,REG_R12,REG_R13,REG_R14,REG_R15,REG_FULL
};

typedef struct{
    char reg_used[16];
    char next_free
}reg_alloc_table_t;

#define STRTABLE_MASK (u64)(0x8000000000000000)
#define DATA_MASK     (u64)(0x4000000000000000)
#define REL_CODE_MASK (u64)(0x2000000000000000)
#define CODE_MASK     (u64)(0xF000000000000000)
#define EXTERN_MASK   (u32)0x80000000
typedef struct{
    u32 len;
    char *ptr;
}vm_string_t;

typedef struct{
    int len;
    hashmap_t subs;
}proto_t;

typedef struct{
    int offset;
    int ptr_depth;
    char builtin;
    bool is_arr;
    u64 impl;
    proto_t *type;
}proto_sub_t;

typedef struct{
    char type;
    u64 got_index;
    proto_t *prot; // prototype
    int ptr_depth; // pointer 
    bool isglo;
    bool is_arr;
    bool is_const;
    char reg_used;
}var_t;


typedef struct{
    vm_string_t name;

    char *jit_compiled;
    char *got_start;
    int jit_compiled_len; // real used length

    int jit_cur;

    hashmap_t prototypes;

    hashmap_t sym_table;

    hashmap_t local_sym_table;

    vec_t export_table;
    vec_t str_table;
    vec_t got_table;

    u64 data;
    u64 stack;
    u64 pushpop_stack;
    u64 alloc_data;
    bool is_native;
}module_t;

#define CSTR2VMSTR(s) (vm_string_t){.len=strlen(s),.ptr=s} 
#define TKSTR2VMSTR(s,l) (vm_string_t){.len = l,.ptr = s}

typedef struct{
    u64 got_index;
    hashmap_t arg_table;
    proto_sub_t ret_type;
    int size;
}function_frame_t;



typedef struct{
    u32 code_offset;
    u8 type;

}qlib_reloc_elem_t;

#define BYTE_ALIGN(x,a) ( ((x) + ((a) - 1) ) & ( ~((a) - 1) ) )

extern hashmap_t glo_libs;


void module_init(module_t *v, vm_string_t name);

void module_pack_jit(module_t*v);

void module_add_prototype(module_t *m,proto_t *t,vm_string_t name);

void module_add_var(module_t* m,var_t *v,vm_string_t name);

int module_add_string(module_t *m,vm_string_t str);

u64 module_add_got(module_t *m, u64 addr);

void module_set_got(module_t *m,u64 idx,u64 addr);

u64 module_get_got(module_t *m,u64 idx);

void emit_access_got(module_t *m,char dst,u64 idx);

var_t* var_new_base(char type,u64 v,int ptr,bool isglo,proto_t *prot, bool is_arr);

proto_t* proto_new(int base_len);

function_frame_t *function_new(u64 ptr);

void proto_debug(proto_t *type);

u64 reserv_data(module_t *v,u32 size);

proto_sub_t* subproto_new(int offset,char builtin,proto_t*prot,int ptrdepth,bool isarr);

void emit(module_t *v,char op);

u64* jit_top(module_t *v);

void module_clean_stack_sym(module_t *v);

void module_clean_proto(module_t *v);

void module_clean_glo_sym(module_t *v);

void module_add_stack_sym(module_t *v,hashmap_t *next);

u64 module_get_func(module_t *v, char *name);

void emit_data(module_t*v,char w,void* data);

void emit_load(module_t*v,char r,u64 m);

void emit_sub_regimm(module_t *v,char r, u32 offset);

void emit_add_regimm(module_t *v,char r, u32 offset);

void emit_call_reg(module_t *v,char r);

void emit_push_reg(module_t *v,char r);
void emit_pop_reg(module_t *v,char r);

void emit_eip_addr(module_t *v,char dst, int pc_offset);

void emit_mov_r2r(module_t*v,char dst,char src);

void emit_mov_addr2r(module_t*v,char dst,char src,char wide_type);

void emit_mov_r2addr(module_t*v,char dst,char src,char type);

void emit_addr2r(module_t*v,char dst,char src);

void emit_minusr2r(module_t*v,char dst,char src);

void emit_mulr(module_t*v,char r);

void emit_divr(module_t*v,char r);

u64 emit_offset_stack(module_t *v);

void emit_restore_stack(module_t *v,u64 offset);

void emit_reg2rbp(module_t*v,char src,i32 offset);

u64* emit_offsetrsp(module_t*v,u32 offset,bool sub);

void emit_call(module_t *v,u64 addr);

u64* emit_jmp_flg(module_t*v);

i32* emit_reljmp_flg(module_t *v);

void proto_impl(module_t *p, proto_t *type);

void glo_sym_debug(module_t *p, hashmap_t *map);

void stack_debug(hashmap_t *map);

void module_release(module_t *entry);

void backup_caller_reg(module_t *v,int no);

void restore_caller_reg(module_t *v,int no);

void emit_unary(module_t *v,char r, char type);

void emit_binary(module_t *v,char dst,char src, char type,char opwide);

void emit_gsbase(module_t *v, char r, char is_read);

void emit_settry(module_t *v);
#endif