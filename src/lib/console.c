#include "console.h"
#include "debuglib.h"
#include "define.h"
#include "hashmap.h"
#include "vm.h"
#include <stdio.h>
#include <stdlib.h>


function_frame_t _print_ptr[2];

u64 qc_lib_print(u64 val,char len){
    void (*f)(u64,u8) = (void*)debuglibs[DBG_PRINT_INT];
    f(val,len);
    return 0;
}

u64 qc_lib_whoami(){
    printf("[CONSOLE] Lib Console V0.1\n");
    return (u64)&_print_ptr[1].ptr;
}

function_frame_t _print_ptr[2] ={
    (function_frame_t){
        .ptr = (u64)qc_lib_print,
        .ret_type.builtin=TP_U64,
        .ret_type.ptr_depth=1},
        (function_frame_t){
        .ptr = (u64)qc_lib_whoami,
        .ret_type.builtin=TP_U64,
        .ret_type.ptr_depth=1}
                        };

void qc_lib_console(module_t *m){
    if(hashmap_get(&m->prototypes, "console", 7))
        return;
    proto_t* prot = proto_new(0);
    proto_sub_t *sub = subproto_new(0,TP_FUNC,0,1);
    proto_sub_t *sub2 = subproto_new(sizeof(function_frame_t),TP_FUNC,0,1);
    prot->len = sizeof(_print_ptr);
    hashmap_put(&prot->subs, "print", 5, sub);
    hashmap_put(&prot->subs, "whoami", 6, sub2);

    module_add_prototype(m, prot, CSTR2VMSTR("_console"));
    var_t *v = var_new_base(TP_CUSTOM, (u64)&_print_ptr, 0, 1, prot);
    module_add_var(m, v, CSTR2VMSTR("console"));
}

