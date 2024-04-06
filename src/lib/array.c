#include "array.h"
#include "define.h"
#include <stdlib.h>

function_frame_t _array_ptr[3];

u64 qc_lib_array_new(u64 unit,u64 sz){
    return (u64)calloc(unit, sz);
}

u64 qc_lib_array_free(u64 addr){
    free((void*)addr);
    return 0;
}

function_frame_t _array_ptr[3] ={
    (function_frame_t){
        .ptr = (u64)qc_lib_array_new,
        .ret_type.builtin=TP_U64,
        .ret_type.ptr_depth=0},
        (function_frame_t){
        .ptr = (u64)qc_lib_array_free,
        .ret_type.builtin=TP_U64,
        .ret_type.ptr_depth=0},
        (function_frame_t){
            .ptr = (u64)0,
            .ret_type.builtin = TP_U64,
            .ret_type.ptr_depth=0    
        }};

void qc_lib_array(module_t *m){
    if(hashmap_get(&m->prototypes, "array", 6))
        return;
    proto_t* prot = proto_new(0);
    proto_sub_t *sub = subproto_new(0,TP_FUNC,0,0);
    proto_sub_t *sub2 = subproto_new(sizeof(function_frame_t),TP_FUNC,0,0);
    prot->len = sizeof(_array_ptr);
    hashmap_put(&prot->subs, "new", 3, sub);
    hashmap_put(&prot->subs, "free", 4, sub2);
    module_add_prototype(m, prot, CSTR2VMSTR("_array"));
    var_t *v = var_new_base(TP_CUSTOM, (u64)&_array_ptr, 0, 1, prot);
    module_add_var(m, v, CSTR2VMSTR("array"));
}

