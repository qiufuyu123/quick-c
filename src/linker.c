#include "debuglib.h"
#include "define.h"
#include "hashmap.h"
#include "parser.h"
#include "vec.h"
#include "vm.h"
#include <asm-generic/errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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
    return link_local(mod, base_data, base_code, base_str);
}

int link_local(module_t *mod,u64 base_data, u64 base_code, u64 base_str){
    FILE *f = NULL;
    qlib_exe_t file_header = {
        .magic="QLIB",
        .compile_info="quick-c v0.1," __DATE__,
        .type=1,
        .code_base = base_code,
        .data_base = base_data,
        .str_base = base_str,
        .code_sz = mod->jit_cur,
        .str_sz = mod->str_table.size,
        .reloc_table_offset = sizeof(qlib_exe_t),
        .reloc_num = mod->reloc_table.size,
        .data_sz = mod->data - DATA_MASK
    };
    if(glo_flag.need_obj){
        f = fopen(glo_flag.dst, "wc");
        fseek(f, sizeof(file_header), SEEK_SET);
    }

    for (int i = 0; i<mod->reloc_table.size; i++) {
        u32 offset = *(u32*)vec_at(&mod->reloc_table, i);
        bool is_extern = 0;
        if(offset & EXTERN_MASK){
            offset &= ~EXTERN_MASK;
            is_extern = 1;
        }
        
        u64 *addr = (u64*)( (u64)mod->jit_compiled + offset );
        if(glo_flag.need_obj){
            fwrite(&offset, sizeof(u32), 1, f);
        }
        if(is_extern){
            
            var_t* nv =(var_t*)(*addr);
            if(nv->type != TP_FUNC){
                if(glo_flag.reloc_table)
                    printf("Reloc-0x%x-0x%llx-content:%llx-EXTERN-DATA\n",offset,(u64)addr,nv->base_addr);
                u64 data_extern = nv->base_addr & ~DATA_MASK;
                *addr = base_data + data_extern;
            }else{
                if(glo_flag.reloc_table)
                    printf("Reloc-0x%x-0x%llx-content:%llx-EXTERN-CODE\n",offset,(u64)addr,nv->base_addr);
                *addr = ((function_frame_t*)nv->base_addr)->ptr;
                if(*addr == 0){
                    printf("Undefined Symbol when linking!\n");
                    return -1;
                }
            }
        }
        else if((*addr) & DATA_MASK){
            if(glo_flag.reloc_table)
                printf("Reloc-0x%x-0x%llx-DATA\n",offset,(u64)addr);
            *addr &= ~DATA_MASK;
            *addr = *addr + base_data;
        }else if((*addr) & STRTABLE_MASK){
            if(glo_flag.reloc_table)
                printf("Reloc-0x%x-0x%llx-STR\n",offset,(u64)addr);
            *addr &= ~STRTABLE_MASK;
            *addr = *addr + base_str;
        }else if(((*addr) & CODE_MASK) == 0){
            if(glo_flag.reloc_table)
                printf("Reloc-0x%x-0x%llx-CODE\n",offset,(u64)addr);
            *addr = *addr + base_code;
        }
        else {
            printf("Reloc-0x%x-0x%llx-content:%llx-UNKNOWN\n",offset,(u64)addr,*addr);
        }
    }
    if(glo_flag.need_obj){
        file_header.main_entry = module_get_func(mod,"_start_");
        file_header.code_offset = file_header.reloc_table_offset + file_header.reloc_num * sizeof(u32);
        fwrite(mod->jit_compiled, mod->jit_cur, 1, f);
        file_header.str_offset += (file_header.code_offset + mod->jit_cur);
        fwrite(mod->str_table.data, mod->str_table.size, 1, f);
        fseek(f, 0, SEEK_SET);
        fwrite(&file_header, sizeof(file_header), 1, f);
        fclose(f);
    }
    
    printf("Linker finished!\n");
    mod->alloc_data = base_data;
    return 1;
}