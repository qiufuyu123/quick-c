#ifndef _H_VM
#define _H_VM

#include "define.h"
#include "vec.h"
#include "hashmap.h"
#include <sys/types.h>
enum{
    TP_I8,TP_U8,TP_I16,TP_U16,TP_I32,TP_U32,TP_I64,TP_U64,TP_INTEGER,
    TP_FLOAT,TP_CUSTOM
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

    vec_t jit_codes;

    char *jit_compiled;
    
    int jit_compiled_len; // real used length

    hashmap_t prototypes;

    hashmap_t sym_table;

    hashmap_t local_sym_table;

    vec_t heap;

    int stack;

}module_t;

#define BYTE_ALIGN(x,a) ( ((x) + ((a) - 1) ) & ( ~((a) - 1) ) )


void module_init(module_t *v, char *name);

void module_pack_jit(module_t*v);

var_t* var_new_base(char type,u64 v,int ptr,bool isglo,proto_t *prot);

proto_t* proto_new(int base_len);

void proto_debug(proto_t *type);

proto_sub_t* subproto_new(char offset);

void emit(module_t *v,char op);

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

void emit_poprax(module_t*v);

void emit_param_4(module_t *v,u64 a,u64 b,u64 c,u64 d);

int emit_call_enter(module_t* v,int p_cnt);

void emit_call(module_t *v,u64 addr);

void emit_call_leave(module_t* v,int sz);

void* string_new(module_t*v,char *str);
#endif