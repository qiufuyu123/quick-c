#include "debuglib.h"
#include "define.h"
#include "hashmap.h"
#include "lex.h"
#include "vec.h"
#include "vm.h"
#include"parser.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

flgs_t glo_flag = {
    .src_start = 0,
    .src_num   = 0,
    .dst       = 0,
    .code_base = 0,
    .glo_sym_table = 0,
    .reloc_table   = 0,
    .need_obj      = 0,
    .need_qlib     = 1,
    .norun         = 0
};

void check(int argc,char**argv){
    for (int i = 1; i<argc; i++) {
        if(argv[i][0] != '-'){
            if(!glo_flag.need_obj){
                if(glo_flag.src_start == 0){
                    glo_flag.src_start = i;
                    glo_flag.src_num = 0;
                }
                glo_flag.src_num++;
            }else {
                printf("Unknown input:%s\n",argv[i]);
            }
        }else {
            char s = argv[i][1];
            if(s == 'r')glo_flag.reloc_table = 1;
            else if(s == 's') glo_flag.glo_sym_table = 1;
            else if(s == 'o'){
                glo_flag.need_obj = 1;
                    i++;
                    glo_flag.dst = argv[i];
            }
            else if(s == 'i' || s == 'I'){
                i++;
                glo_flag.glo_include = argv[i];
            }
            else{
                if(!strcmp(argv[i],"-code")){
                    if(argc <= i + 1){
                        printf("Expect base_address after -code");
                        exit(-1);
                    }
                    char *num = argv[i+1];
                    glo_flag.code_base = atoll(num);
                }else if(!strcmp(argv[i], "-nostd")){
                    glo_flag.need_qlib = 0;
                }else if(!strcmp(argv[i], "-norun")){
                    glo_flag.norun = 1;
                }
                else {
                    printf("Not supported switch:%s\n",argv[i]);
                    exit(-1);
                }
                
            };
        }
    }
    if(glo_flag.src_start == 0){
        printf("ERRO: No src inputed!\n");
        
    }
}



int main(int argc,char**argv){
    check(argc, argv);
    module_t *entry = 0;
    for (int i = 0; i<glo_flag.src_num; i++) {
        printf("Start Compile%s\n",argv[glo_flag.src_start + i]);
        entry= module_compile(argv[glo_flag.src_start+i], "main", 4, 0,entry);
        if(!entry){
            printf("Fail to compile module:'main'\n");
            return -1;
        }
        printf("Compile %s OK\n",argv[glo_flag.src_start + i]);
    }
    entry->alloc_data = 0;
    if(glo_flag.glo_sym_table){
        glo_sym_debug(&entry->sym_table);
    }
    if(glo_flag.dst)
        printf("%s\n",glo_flag.dst);
    if(link_jit(entry) != 1){
        exit(-1);
    }
    int(*start)() = entry->jit_compiled;
    
    // if(glo_flag.reloc_table){
    //     printf("Total Reloc Item:%d\n",entry->reloc_table.size);
    //     for (int i = 0; i<entry->reloc_table.size; i++) {
    //         printf("0x%llx;\n",*(u64*)vec_at(&entry->reloc_table, i));
    //     }
    // }
    FILE *f = fopen("core.bin", "wc");
    fwrite(entry->jit_compiled, entry->jit_cur, 1, f);
    fclose(f);
    if(!glo_flag.norun)
    {
        printf("RUN");
        start();
    }
    if(glo_flag.need_qlib && !glo_flag.norun){
        u64 qlib_entry = module_get_func(entry, "_start_");
        if(!qlib_entry){
            printf("[ERRO]: Cannot find main-entry(qlibc) _start_\n");
            exit(-1);
        }
        printf("called with:%llx\n",debuglibs);
        void (*qlib_main)(native_func_t*list,u8 sz) = (void*)qlib_entry;
        qlib_main(&debuglibs[0],2);
        printf("JIT returned: %dbytes / %d % \n",entry->jit_cur,entry->jit_cur*100/entry->jit_compiled_len);

    }
    
    if(glo_flag.need_obj && glo_flag.dst){
       // module_pack(entry, glo_flag.dst);
    }
    module_release(entry);
    macro_free();
    return 0;
}