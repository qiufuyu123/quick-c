#include "vm.h"
#include "hashmap.h"
#include "vec.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
void panic_oom(const char *msg){
    printf("VM: %s: OOM\n",msg);
    exit(1);
}

void module_pack_jit(module_t*v){
    v->jit_compiled_len = v->jit_codes.size;
    u64 len = BYTE_ALIGN(v->jit_codes.size, 4096);
    char *memory = mmap(NULL,             // address
                      len,             // size
                      PROT_READ | PROT_WRITE | PROT_EXEC,
                      MAP_PRIVATE | MAP_ANONYMOUS,
                      -1,               // fd (not used here)
                      0);               // offset (not used here)
    memcpy(memory, v->jit_codes.data, v->jit_codes.size);
    v->jit_compiled = memory;
    vec_release(&v->jit_codes);
}

void module_init(module_t *v, char *name){
    v->module_name = name;
    vec_init(&v->jit_codes, 1, 16);
    vec_init(&v->heap, 1, 64);
    if(hashmap_create(2, &v->sym_table)){
        panic_oom("Fail to alloc sym_table");
    }
    if(hashmap_create(2, &v->local_sym_table)){
        panic_oom("Fail to alloc local_sym_table");
    }
    if(hashmap_create(2, &v->prototypes)){
        panic_oom("Fail to alloc prototypes");
    }
    v->stack = 0;
    v->jit_compiled = NULL;
}

var_t* var_new_base(char type,u64 v,int ptr,bool isglo,proto_t* prot){
    var_t* r= malloc(sizeof(var_t));
    if(!r)
        panic_oom("Fail to alloc var");
    r->base_addr = v;
    r->type = type;
    r->ptr_depth = ptr;
    r->prot = prot;
    r->isglo = isglo;
    return r;

}

proto_t* proto_new(int base_len){
    proto_t* t = malloc(sizeof(proto_t));
    t->len = base_len;
    hashmap_create(1, &t->subs);
    return t;
}

int _debug_prot(void* const context, struct hashmap_element_s* const e) {
  char buf[100]={0};
  memcpy(buf, e->key, e->key_len);
  proto_sub_t* sub = e->data;
  printf("| name: %s ",buf);
  printf("- offset:%d",sub->offset);
  if(sub->type){
    printf(": type(%d):\n",sub->builtin);
    proto_debug(sub->type);
  }else {
    printf(": builtin:%d\n",sub->builtin);
  }
  return 0;
}


void proto_debug(proto_t *type){
    printf("Prototype:%d\n",type->subs.size);
    hashmap_iterate_pairs(&type->subs, _debug_prot, NULL);
    printf("Prototype End(%d bytes)\n",type->len);
}

proto_sub_t* subproto_new(char offset){
    proto_sub_t* t = malloc(sizeof(proto_sub_t));
    t->offset = offset;
    t->type = NULL;
    return t;
}

void emit(module_t *v,char op){
    vec_push(&v->jit_codes, &op);
}

void emit_data(module_t*v,char w,void* data){
    vec_push_n(&v->jit_codes, data, w);
}

void emit_load(module_t*v,char r,u64 m){
    emit(v, 0x48);
    emit(v, 0xb8+r); 
    emit_data(v, 8, &m);
}

static void emit_rm(module_t*v,char dst,char src,char mode,char opc){
    char op = mode,prefix=0b01001000;
    op |= ((0b00000111 & src) <<3);
    op |= (0b00000111 & dst);
    if(src & 0b00001000){
        prefix |= 0b00000100;
    }
    if(dst & 0b00001000){
        prefix |= 0b00000001;
    }
    emit(v, prefix);
    emit(v, opc);
    emit(v, op);    
}

void emit_mov_r2r(module_t*v,char dst,char src){ 
    emit_rm(v, dst, src, 0b11000000, 0x89);
}

void emit_mov_addr2r(module_t*v,char dst,char src){
    emit_rm(v, src, dst, 0, 0x8b);
}

void emit_mov_r2addr(module_t*v,char dst,char src){
    emit_rm(v, dst, src, 0, 0x89);
}

void emit_addr2r(module_t*v,char dst,char src){
    emit_rm(v, dst, src, 0b11000000, 0x01);
}

void emit_minusr2r(module_t*v,char dst,char src){
    emit_rm(v, dst, src, 0b11000000, 0x29);
}

void emit_mulrbx(module_t*v){
    emit(v, 0x48);
    emit(v, 0xf7);
    emit(v, 0xe3);
}

void emit_divrbx(module_t*v){
    emit_load(v, REG_DX, 0);
    emit(v, 0x48);
    emit(v, 0xf7);
    emit(v, 0xf3);
}

void emit_pushrax(module_t*v){
    emit(v, 0x50);
}
void emit_poprax(module_t*v){
    emit(v,0x58);
}

void emit_param_4(module_t *v,u64 a,u64 b,u64 c,u64 d){
    emit_load(v, REG_CX, a);emit_load(v, REG_DX, b);
    emit_load(v, REG_R8, c);emit_load(v, REG_R9, d);
}

void emit_call(module_t *v,u64 addr){
    emit_load(v, REG_AX, addr);
    emit(v, 0xff);emit(v,0xd0); // callq [rax]
}

int emit_call_enter(module_t* v,int p_cnt){
    int sz = p_cnt*8 + 8; // an extra 8 for ebp stack
    sz = BYTE_ALIGN(sz, 16);
    emit_load(v, REG_AX, sz);
    emit_minusr2r(v, REG_SP, REG_AX);
    return sz;
}

void emit_sub_esp(module_t* v,int sz){
    if(sz <= 0x80){
        emit(v, 0x83);
        emit(v, 0xec);
        emit(v, sz);
    }else {
        emit(v, 0x81);
        emit(v, 0xec);
        emit_data(v, 4, &sz);
    }
}

void emit_add_esp(module_t* v,int sz){
    if(sz < 0x80){
        emit(v, 0x83);
        emit(v, 0xc4);
        emit(v, sz);
    }else {
        emit(v, 0x81);
        emit(v, 0xc4);
        emit_data(v, 4, &sz);
    }
}

void emit_call_leave(module_t* v,int sz){
    emit_load(v, REG_AX, sz);
    emit_addr2r(v, REG_SP, REG_AX);
}

void* string_new(module_t*v,char *str){
    return vec_push_n(&v->heap, str, strlen(str)+1);
}