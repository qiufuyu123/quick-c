#include "vm.h"
#include "define.h"
#include "hashmap.h"
#include "parser.h"
#include "vec.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define __USE_GNU
#include <sys/mman.h>
#include <sys/types.h>

extern flgs_t glo_flag;

void panic_oom(const char *msg){
    printf("VM: %s: OOM\n",msg);
    exit(1);
}


void module_pack_jit(module_t*v){

    v->jit_compiled_len = 4096*(glo_flag.jit_pg_no+1);

    char *memory = mmap(NULL,             // address
                      v->jit_compiled_len,             // size
                      PROT_READ | PROT_WRITE | PROT_EXEC,
                      MAP_PRIVATE | MAP_ANONYMOUS,
                      -1,               // fd (not used here)
                      0);               // offset (not used here)
    v->jit_compiled = memory+4096;
    v->jit_cur=0;
    v->got_start = memory;
}

void module_init(module_t *v, vm_string_t name){
    v->name = name;
    vec_init(&v->str_table, 1, 64);
    vec_init(&v->reloc_table, sizeof(u32), 2);
    vec_init(&v->got_table, 8, 64);
    u64 str_entry = 0;
    vec_push(&v->got_table, &str_entry);
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

void emit_eip_addr(module_t *v,char dst, int pc_offset){
    pc_offset -= 0x07;
    if(dst < REG_R8){
        emit(v, 0x48);
    }else {
        dst -= REG_R8;
        emit(v, 0x4c);
    }
    emit(v, 0x8b);
    emit(v, ((const char[]){0x05,0x0d,0x15,0x1d,0x35,0x3d})[dst]);
    emit_data(v, 4, &pc_offset);
}

void module_add_reloc(module_t *m, u32 addr){
    vec_push(&m->reloc_table, &addr);
}

u64 module_add_got(module_t *m, u64 addr){
    vec_push(&m->got_table, &addr);
    return m->got_table.size;
}

void module_set_got(module_t *m,u64 idx,u64 addr){
    *((u64*)vec_at(&m->got_table, idx-1))=addr;
}

u64 module_get_got(module_t *m,u64 idx){
    return *((u64*)vec_at(&m->got_table, idx-1));
}

void emit_access_got(module_t *m,char dst,u64 idx){
    emit_eip_addr(m,dst,-m->jit_cur-idx*8);
}

function_frame_t *function_new(u64 ptr){
    function_frame_t* r= malloc(sizeof(function_frame_t));
    if(!r)
        panic_oom("Fail to alloc function frame");
    r->got_index = ptr;
    hashmap_create(1, &r->arg_table);
    r->size=0;
    return r;
}

var_t* var_new_base(char type,u64 v,int ptr,bool isglo,proto_t* prot,bool isarr){
    var_t* r= malloc(sizeof(var_t));
    if(!r)
        panic_oom("Fail to alloc var");
    r->got_index = v;
    r->type = type;
    r->ptr_depth = ptr;
    r->prot = prot;
    r->isglo = isglo;
    r->is_const = 0;
    r->is_arr = isarr;
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
        function_frame_t *fram = (function_frame_t*)nv->got_index;
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
    module_t *m = context;
    printf("@%s: 0x%llx",buf,addr->got_index);
    if(addr->type == TP_FUNC){
        function_frame_t* fram = (function_frame_t*)addr->got_index;
        printf("--%llx",fram->got_index);
        printf("--%llx",module_get_got(m,fram->got_index));
    }else {
        printf("--%llx",module_get_got(m,addr->got_index));

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
        emit_load(p->p,REG_BX,fb->got_index);
        emit(p->p, 0x48);emit(p->p,0x89);emit(p->p,0x98);
        u32 dst = p->offset+sub->offset;
        emit_data(p->p, 4, &dst);
    }
    return 0;
}

void glo_sym_debug(module_t*p, hashmap_t *map){
    printf("sym global: \n");
    hashmap_iterate_pairs(map,_sym_iter_call,p);
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
    return ((function_frame_t*)var->got_index)->got_index;
}

proto_sub_t* subproto_new(int offset,char builtin,proto_t*prot,int ptrdepth,bool isarr){
    proto_sub_t* t = malloc(sizeof(proto_sub_t));
    t->offset = offset;
    t->type = prot;
    t->builtin=builtin;
    t->ptr_depth=ptrdepth;
    t->impl = 0;
    t->is_arr = isarr;
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
        //mremap(void *addr, size_t old_len, size_t new_len, int flags, ...)
        panic_oom("JIT OOM");
    }
    memcpy(v->jit_compiled+v->jit_cur, data, w);
    v->jit_cur+=w;
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