#include "define.h"
#include "hashmap.h"
#include "parser.h"
#include "vec.h"
#include "vm.h"
#include <asm-generic/errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include "qlib.h"

extern flgs_t glo_flag;

int link_jit(module_t *mod){
    // if(glo_flag.need_qlib){
    //     for (int i = 0; i<DBG_NUM; i++) {
    //         var_t *v = (var_t*)hashmap_get(&mod->sym_table, debuglibs[i].name, strlen(debuglibs[i].name));
    //         if(v){
    //             function_frame_t *func = (function_frame_t*)v->base_addr;
    //             func->ptr = (u64)debuglibs[i].addr;
    //             printf("[debuglib:]%s-->%llx\n",debuglibs[i].name,func->ptr);
    //         }
    //     }
    // }
    u64 data_seg_used = mod->data - DATA_MASK;
    u64 base_data = (u64)calloc(1, data_seg_used);
    if(base_data == 0){
        printf("[LINKER]: Fail to allocate %d bytes for data seg.\n",data_seg_used);
        return -1;
    }
    u64 base_code = (u64)mod->jit_compiled;
    u64 base_str = (u64)mod->str_table.data;
    return link_local(mod, base_data, base_code, base_str,(u64)mod->got_start);
}

int got_local(module_t *mod,u64 database,u64 codebase,u64 strbase){
    u64 *got_base = (u64*)(codebase-8);

    for (int i =0; i<mod->got_table.size; i++) {
        u64 *elem_ptr = vec_at(&mod->got_table, i);
        u64 elem = *elem_ptr;
        if(!glo_flag.norun){
        if(i == 0){
            // spec for str table
            *elem_ptr = strbase;
        }else{
            if(*elem_ptr == 0){
                printf("Undefined extern value in GOT\n");
                return 0;;
            }else {
                if(elem & DATA_MASK){
                    elem &= ~DATA_MASK;
                    *elem_ptr = elem + database;
                }else {
                    *elem_ptr = elem + codebase;
                }
            }
        }
        }
        *got_base = *elem_ptr;
        got_base--;
    }
    return 1;
}

int link_local(module_t *mod,u64 base_data, u64 base_code, u64 base_str, u64 base_got){
    FILE *f = NULL;
    qlib_exe_t file_header = {
        .magic="QLIB",
        .compile_info="quick-c v0.1," __DATE__,
        .type=1,
        .code_base = base_code,
        .data_base = base_data,
        .str_base = base_str,
        .code_sz = mod->jit_cur+mod->got_table.size*8,
        .str_sz = mod->str_table.size,
        .data_sz = mod->data - DATA_MASK
    };
    if(glo_flag.need_obj){
        f = fopen(glo_flag.dst, "wc");
        fseek(f, sizeof(file_header), SEEK_SET);
    }
    // No need for relocation 
    file_header.main_offset = module_get_got(mod, module_get_func(mod, "_start_"));
    got_local(mod, base_data, base_code,base_str);
    if(glo_flag.need_obj){
        // file_header.main_entry = module_get_func(mod,"_start_");
        // file_header.code_offset = sizeof(qlib_exe_t);// file_header.reloc_table_offset + file_header.reloc_num * sizeof(u32);
        // file_header.setup_entry = file_header.code_offset + mod->got_table.size * 8;
        file_header.got_offset = sizeof(qlib_exe_t);
        
        file_header.got_size = mod->got_table.size * 8;
        file_header.code_offset = file_header.got_offset + file_header.got_size;
        fwrite((char*)mod->jit_compiled-file_header.got_size, mod->jit_cur+file_header.got_size, 1, f);
        file_header.str_offset = (file_header.code_offset + mod->jit_cur);
        fwrite(mod->str_table.data, mod->str_table.size, 1, f);
        fseek(f, 0, SEEK_SET);
        fwrite(&file_header, sizeof(file_header), 1, f);
        fclose(f);
    }
    printf("Linker Finish: code size:%d data size:%d str size:%d  main_offset%llx stroffset:%llx\n",mod->jit_cur,mod->data-DATA_MASK,mod->str_table.size,file_header.main_offset,file_header.str_offset);
    // printf("Linker finished! code_rel:%d,data_rel:%d\n",rel_code_cnt,rel_data_cnt);
    mod->alloc_data = base_data;
    return 1;
}