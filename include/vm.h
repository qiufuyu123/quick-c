#ifndef _H_VM
#define _H_VM

#include "define.h"
#include "vec.h"
#include "hashmap.h"
#include <sys/types.h>
enum{
    TP_I8,TP_U8,TP_I16,TP_U16,TP_I32,TP_U32,TP_I64,TP_U64,TP_INTEGER,
    TP_FLOAT,TP_FUNC,TP_CUSTOM
};

enum{
    REG_AX,REG_CX,REG_DX,REG_BX,REG_SP,REG_BP,REG_SI,REG_DI,REG_R8,
    REG_R9,REG_R10,REG_R11,REG_R12,REG_R13,REG_R14,REG_R15
};

typedef struct{
    int len;
    hashmap_t subs;
}proto_t;

typedef struct{
    int offset;
    int ptr_depth;
    char builtin;
    proto_t *type;
}proto_sub_t;

typedef struct{
    char type;
    u64 base_addr;
    proto_t *prot; // prototype
    int ptr_depth; // pointer 
    bool isglo;
}var_t;

typedef struct{
    char *module_name;  // source are split into module

    char *jit_compiled;
    
    int jit_compiled_len; // real used length

    int jit_cur;

    hashmap_t prototypes;

    hashmap_t sym_table;

    hashmap_t local_sym_table;

    vec_t str_table;

    vec_t heap;

    int stack;

}module_t;

typedef struct{
    u32 len;
    char *ptr;
}vm_string_t;

#define CSTR2VMSTR(s) (vm_string_t){.len=strlen(s),.ptr=s} 
#define TKSTR2VMSTR(s,len) (vm_string_t){.len = len,.ptr = s}

typedef struct{
    u64 ptr;
    hashmap_t arg_table;
    proto_sub_t ret_type;
    int size;
}function_frame_t;

#define BYTE_ALIGN(x,a) ( ((x) + ((a) - 1) ) & ( ~((a) - 1) ) )


void module_init(module_t *v, char *name);

void module_pack_jit(module_t*v);

void module_add_prototype(module_t *m,proto_t *t,vm_string_t name);

void module_add_var(module_t* m,var_t *v,vm_string_t name);

void* module_add_string(module_t *m,vm_string_t str);

var_t* var_new_base(char type,u64 v,int ptr,bool isglo,proto_t *prot);

proto_t* proto_new(int base_len);

function_frame_t *function_new(u64 ptr);

void proto_debug(proto_t *type);

proto_sub_t* subproto_new(int offset,char builtin,proto_t*prot,int ptrdepth);

void emit(module_t *v,char op);

u64* jit_top(module_t *v);

u64 *jit_restore_off(module_t *v,int off);

void module_clean_stack_sym(module_t *v);

void module_add_stack_sym(module_t *v,hashmap_t *next);

void emit_data(module_t*v,char w,void* data);

void emit_load(module_t*v,char r,u64 m);

void emit_mov_r2r(module_t*v,char dst,char src);

void emit_mov_addr2r(module_t*v,char dst,char src);

void emit_mov_r2addr(module_t*v,char dst,char src);

void emit_addr2r(module_t*v,char dst,char src);

void emit_minusr2r(module_t*v,char dst,char src);

void emit_mulrbx(module_t*v);

void emit_divrbx(module_t*v);

void emit_pushrax(module_t*v);

void emit_saversp(module_t *v);

void emit_restorersp(module_t *v);

void emit_poprax(module_t*v);

void emit_param_4(module_t *v,u64 a,u64 b,u64 c,u64 d);

void emit_reg2rbp(module_t*v,char src,i32 offset);

void emit_rbpload(module_t *v,char w,u32 offset);

void emit_rsp2bx(module_t*v,u32 offset);

u64* emit_offsetrsp(module_t*v,u32 offset,bool sub);

int emit_call_enter(module_t* v,int p_cnt);

void emit_call(module_t *v,u64 addr);

u64* emit_jmp_flg(module_t*v);

void emit_call_leave(module_t* v,int sz);

void* string_new(module_t*v,char *str);
#endif