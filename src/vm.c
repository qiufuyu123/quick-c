#include "vm.h"
#include "define.h"
#include "hashmap.h"
#include "parser.h"
#include "vec.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/types.h>

void panic_oom(const char *msg){
    printf("VM: %s: OOM\n",msg);
    exit(1);
}

void module_pack_jit(module_t*v){
    v->jit_compiled_len = 4096*8;
    u64 len = 4096*8;
    char *memory = mmap(NULL,             // address
                      len,             // size
                      PROT_READ | PROT_WRITE | PROT_EXEC,
                      MAP_PRIVATE | MAP_ANONYMOUS,
                      -1,               // fd (not used here)
                      0);               // offset (not used here)
    v->jit_compiled = memory;
    v->jit_cur=0;
}

void module_init(module_t *v, vm_string_t name){
    v->name = name;
    vec_init(&v->str_table, 1, 64);
    vec_init(&v->reloc_table, sizeof(u32), 2);
    if(hashmap_create(2, &v->sym_table)){
        panic_oom("Fail to alloc sym_table");
    }
    if(hashmap_create(2, &v->local_sym_table)){
        panic_oom("Fail to alloc local_sym_table");
    }
    if(hashmap_create(2, &v->prototypes)){
        panic_oom("Fail to alloc prototypes");
    }
    v->data = DATA_MASK;
    v->stack = 0;
    module_pack_jit(v);
}

void module_add_prototype(module_t *m,proto_t *t,vm_string_t name){
    hashmap_put(&m->prototypes, name.ptr, name.len, t);
}

void module_add_var(module_t* m,var_t *v,vm_string_t name){
    hashmap_put(&m->sym_table, name.ptr, name.len, v);
}

int module_add_string(module_t *m,vm_string_t str){
    // not sure for the format of string
    // vec_push_n(&m->str_table, &str.len, 4);
    char buf[128] = {0}; // TODO
    if(str.len > 127){
        return -1;
    }
    int ret = (u64)vec_top(&m->str_table)+1-(u64)m->str_table.data;
    memcpy(buf, str.ptr, str.len);
    char *c = buf;
    char dst = '\0';
    while (*c) {
        if(*c == '\\'){
            c++;
            if(*c == 0){
                return -1;
            }
            switch (*c) {
                case 'n':
                    dst = '\n';
                    break;
                case 'b':
                    dst = '\b';
                    break;
                case 't':
                    dst = '\t';
                    break;
                case '0':
                    dst = '\0';
                    break;
                case 'r':
                    dst = '\r';
                    break;
                default:
                    return -1;
            }
            vec_push(&m->str_table,&dst);
        }else {
            vec_push(&m->str_table,c);
        }

        c++;
    }
    dst = '\0';
    vec_push(&m->str_table, &dst); // terminate
    return ret;
}

void module_add_reloc(module_t *m, u32 addr){
    vec_push(&m->reloc_table, &addr);
}

function_frame_t *function_new(u64 ptr){
    function_frame_t* r= malloc(sizeof(function_frame_t));
    if(!r)
        panic_oom("Fail to alloc function frame");
    r->ptr = ptr;
    hashmap_create(1, &r->arg_table);
    r->size=0;
    return r;
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

int _iter_sym(void* const context, struct hashmap_element_s* const e) {
  char buf[100]={0};
  memcpy(buf, e->key, e->key_len);
  void* val = e->data;
  hashmap_t *ctx = (hashmap_t*)context;
  hashmap_put(ctx, buf, e->key_len, val);
  return 0;
}

void module_add_stack_sym(module_t *v,hashmap_t *next){
    hashmap_iterate_pairs(next, _iter_sym, &v->local_sym_table);
}

int _iter_destroy_subproto(void *const context,struct hashmap_element_s* const e){
    free(e->data);
    return -1;
}

int _iter_destroy_proto(void *const context,struct hashmap_element_s* const e){
    proto_t *prot = (var_t*)e->data;
    hashmap_iterate_pairs(&prot->subs, _iter_destroy_subproto, 0);
    hashmap_destroy(&prot->subs);
    free(prot);
    return -1;
}

int _iter_destroy(void *const context,struct hashmap_element_s* const e){
    var_t *nv = (var_t*)e->data;
    if(nv->type == TP_FUNC){
        function_frame_t *fram = (function_frame_t*)nv->base_addr;
        if(fram){
            // hashmap_iterate_pairs(&fram->arg_table, _iter_destroy, 0);
            hashmap_destroy(&fram->arg_table);
            free(fram);
        }
    }
    free(nv);
    return -1;
}

void module_clean_proto(module_t *v){
    hashmap_iterate_pairs(&v->prototypes, _iter_destroy_proto, 0);
}

void module_clean_stack_sym(module_t *v){
    hashmap_iterate_pairs(&v->local_sym_table, _iter_destroy, 0);
    // hashmap_destroy(&v->local_sym_table);
    // hashmap_create(2, &v->local_sym_table);
}

void module_clean_glo_sym(module_t *v){
    hashmap_iterate_pairs(&v->sym_table, _iter_destroy, 0);
    // hashmap_destroy(&v->local_sym_table);
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

u64 reserv_data(module_t *v,u32 sz){
    u64 addr = v->data;
    v->data += sz;
    return addr;
}

typedef struct{
    module_t *p;
    u32 offset;
}_prot_impl_t;

int _sym_iter_call(void* const context,struct hashmap_element_s* const e){
    char buf[100]={0};
    memcpy(buf, e->key, e->key_len);
    var_t* addr = (var_t*)e->data;
    printf("@%s: 0x%llx",buf,addr->base_addr);
    if(addr->type == TP_FUNC){
        function_frame_t* fram = (function_frame_t*)addr->base_addr;
        printf("--%llx",fram->ptr);
    }
    printf("\n");
    return 0;
}

int _prot_impl_call(void* const context, struct hashmap_element_s* const e) {
    char buf[100]={0};
    memcpy(buf, e->key, e->key_len);
    proto_sub_t* sub = e->data;
    _prot_impl_t *p = context;
    if(sub->builtin == TP_CUSTOM){
        u32 old = p->offset;
        p->offset+=sub->offset;
        hashmap_iterate_pairs(&sub->type->subs, _prot_impl_call, p);
        p->offset = old;
    }else if(sub->impl){
        function_frame_t *fb = (function_frame_t*)sub->impl;
        emit_load(p->p,REG_BX,fb->ptr);
        emit(p->p, 0x48);emit(p->p,0x89);emit(p->p,0x98);
        u32 dst = p->offset+sub->offset;
        emit_data(p->p, 4, &dst);
    }
    return 0;
}

void glo_sym_debug(hashmap_t *map){
    printf("sym global: \n");
    hashmap_iterate_pairs(map,_sym_iter_call,0);
    printf("sym table end.\n");
} 

void stack_debug(hashmap_t *map){
    printf("stack global: \n");
    hashmap_iterate_pairs(map,_sym_iter_call,0);
    printf("stack end.\n");
} 

void proto_impl(module_t *p, proto_t *type){
    _prot_impl_t parent;
    parent.p = p;
    parent.offset = 0;
    hashmap_iterate_pairs(&type->subs, _prot_impl_call, &parent);
}

u64 module_get_func(module_t*v, char *name){
    var_t *var = (var_t*)hashmap_get(&v->sym_table, name, strlen(name));
    if(!var)
        return 0;
    if(var->type != TP_FUNC)
        return 0;
    return ((function_frame_t*)var->base_addr)->ptr;
}

proto_sub_t* subproto_new(int offset,char builtin,proto_t*prot,int ptrdepth){
    proto_sub_t* t = malloc(sizeof(proto_sub_t));
    t->offset = offset;
    t->type = prot;
    t->builtin=builtin;
    t->ptr_depth=ptrdepth;
    t->impl = 0;
    return t;
}

void emit(module_t *v,char op){
    if(v->jit_cur+1 >= v->jit_compiled_len){
        panic_oom("JIT OOM");
    }
    *(v->jit_compiled+v->jit_cur)=op;
    v->jit_cur++;
}

u64* jit_top(module_t *v){
    return (u64*)(v->jit_compiled+v->jit_cur);
}

void emit_data(module_t*v,char w,void* data){
    if(v->jit_cur+w >= v->jit_compiled_len){
        panic_oom("JIT OOM");
    }
    memcpy(v->jit_compiled+v->jit_cur, data, w);
    v->jit_cur+=w;
}

void emit_load(module_t*v,char r,u64 m){
    emit(v, 0x48);
    emit(v, 0xb8+r); 
    emit_data(v, 8, &m);
}

static void emit_offset_rbx_call(module_t *v,int offset, char w, char is_add){
    emit(v, 0x48);
    emit(v, w?0x81:0x83);
    emit(v, is_add?0xc3:0xeb);
    emit_data(v, w?4:1, &offset);
}

void emit_sub_rbx(module_t *v,int offset){
    emit_offset_rbx_call(v, offset, offset>127, 0);
}

void emit_add_rbx(module_t *v,int offset){
    emit_offset_rbx_call(v, offset, offset>127, 1);
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


void emit_pushrbx(module_t *v){
    emit(v, 0x53); 
}
void emit_pushrax(module_t*v){
    emit(v, 0x50);
}
void emit_poprax(module_t*v){
    emit(v,0x58);
}
void emit_poprbx(module_t*v){
    emit(v, 0x5b);
}

void emit_saversp(module_t *v){
    emit(v, 0x54);
}

void emit_restorersp(module_t *v){
    emit(v,0x5c);
}

void emit_loadglo(module_t *v, u64 base_addr,bool isrbx,bool is_undef){
    u64*ptr =(u64*)emit_label_load(v,isrbx);
    *ptr = base_addr;
    u32 val = (u64)ptr - (u64)v->jit_compiled;
    module_add_reloc(v, is_undef?val|EXTERN_MASK:val);
}

void emit_param_4(module_t *v,u64 a,u64 b,u64 c,u64 d){
    emit_load(v, REG_CX, a);emit_load(v, REG_DX, b);
    emit_load(v, REG_R8, c);emit_load(v, REG_R9, d);
}

void emit_reg2rbp(module_t*v,char src,i32 offset){
    offset = -offset;
    switch (src) {
        case TP_U8:case TP_I8:
            emit(v, 0x88);
            break;
        case TP_U16:case TP_I16:
            emit(v, 0x66);emit(v, 0x89);
            break;
        case TP_U32:case TP_I32:
            emit(v, 0x89);
            break;
        case TP_U64:case TP_I64:
            emit(v, 0x48);emit(v, 0x89);
            break;
    }
    if(offset<128 && offset>=-127){
        emit(v, 0x45);
        emit(v,(u8)offset);
    }else {
        emit(v,0x85);
        emit_data(v, 4, &offset);
    }
    
}

void emit_rbpload(module_t *v,char w,u32 offset){
    offset = -offset;
    if(w < TP_I64){
        emit(v, 0x48);emit(v, 0x31);emit(v, 0xc0); // xor rax,rax
    }
     switch (w) {
        case TP_U8:case TP_I8:
            emit(v, 0x8a);
            break;
        case TP_U16:case TP_I16:
            emit(v, 0x66);emit(v, 0x8b);
            break;
        case TP_U32:case TP_I32:
            emit(v, 0x8b);
            break;
        case TP_U64:case TP_I64:
            emit(v, 0x48);emit(v, 0x8b);
            break;
    }
    if(offset<128){
        emit(v, 0x45);
        emit(v,(u8)offset);
    }else {
        emit(v,0x85);
        emit_data(v, 4, &offset);
    }
    
}

void emit_storelocaddr(module_t *v,u32 dst,u32 src){
    emit_mov_r2r(v, REG_AX, REG_BP);
    emit_load(v, REG_BX, src);
    emit_minusr2r(v, REG_AX, REG_BX);
    emit_reg2rbp(v, TP_U64, dst);
}

void emit_rsp2bx(module_t*v,u32 offset){
    emit(v, 0x48);emit(v, 0x8b);
    if(offset<128){
        emit(v, 0x5c);
        emit(v, 0x24);
        emit(v,(u8)offset);
    }else {
        emit(v,0x9c);
        emit(v, 0x24);
        emit_data(v, 4, &offset);
    }
}

u64* emit_offsetrsp(module_t*v,u32 offset,bool sub){
    emit(v, 0x48);
    if(offset<128){
        emit(v, 0x83);
    }else {
        emit(v, 0x81);
    }
    sub?emit(v, 0xec):emit(v,0xc4);
    u64* r = jit_top(v);
    if(offset<128){
        emit(v,offset);
    }else {
        emit_data(v, 4, &offset);
    }
    return r;
}

u64* emit_jmp_flg(module_t*v){
    emit_load(v, REG_AX, 0);
    u64 ret = (u64)jit_top(v)-8;
    emit(v, 0xff);emit(v,0xe0);
    return (u64*)ret;
}

u64 *jit_restore_off(module_t *v,int off){
    return (u64*)((u64)v->jit_compiled+off);
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

u64 emit_label_load(module_t* v,bool isrbx){
    emit(v, 0x48);emit(v,isrbx?0xbb:0xb8);
    u64 ret = (u64)v->jit_compiled+ v->jit_cur;
    u64 def = 0;
    emit_data(v, 8, &def);
    return ret;
}

void backup_caller_reg(module_t *v,int no){
    emit(v, no == 1?0x57:
            no == 2?0x56:
            no == 3?0x52:
            no == 4?0x51:0x41);
    if(no == 5){
        emit(v, 0x50); // r8
    }else if(no == 6){
        emit(v, 0x51); //r9
    }
}

void restore_caller_reg(module_t *v,int no){
    emit(v, no == 1?0x5f:
            no == 2?0x5e:
            no == 3?0x5a:
            no == 4?0x59:0x41);
    if(no == 5){
        emit(v, 0x58); // r8
    }else if(no == 6){
        emit(v, 0x59); //r9
    }
}

void module_release(module_t *entry){
    module_clean_glo_sym(entry);
    module_clean_proto(entry);
    hashmap_destroy(&entry->sym_table);
    hashmap_destroy(&entry->local_sym_table);
    hashmap_destroy(&entry->prototypes);
    vec_release(&entry->reloc_table);
    vec_release(&entry->str_table);
    if(entry->alloc_data){
        free((void*)entry->alloc_data);
    }
    free(entry);
}