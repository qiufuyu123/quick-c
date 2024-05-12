#include "define.h"
#include "lex.h"
#include "vec.h"
#include "vm.h"
#include"parser.h"
#include <bits/types/FILE.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct{
    char *src;
    char *dst;
    bool need_obj;
    bool reloc_table;
    bool glo_sym_table;
}flgs_t;

static flgs_t glo_flag = {
    0,"a.out",0,1,1
};

void check(int argc,char**argv){
    for (int i = 1; i<argc; i++) {
        if(argv[i][0] != '-'){
            if(!glo_flag.need_obj){
                if(!glo_flag.src)
                    glo_flag.src = argv[i];
            }else {
                if(!glo_flag.dst){
                    glo_flag.dst = argv[i];
                }
            }
        }else {
            char s = argv[i][1];
            if(s == 'r')glo_flag.reloc_table = 1;
            else if(s == 's') glo_flag.glo_sym_table = 1;
            else if(s == 'o') glo_flag.need_obj = 1;
            else{
                printf("Not supported switch:%s\n",argv[i]);
                exit(-1);
            };
        }
    }
    if(!glo_flag.src){
        printf("WARN: No src inputed!\n");
        glo_flag.src = "test/string.qc";
    }
}

int main(int argc,char**argv){
    check(argc, argv);
    module_liblist_init();
    module_t *entry= module_compile(glo_flag.src, "main", 4, 0);
    if(!entry){
        printf("Fail to compile module:'main'\n");
        return -1;
    }
    int(*start)() = entry->jit_compiled;
    if(glo_flag.glo_sym_table){
        glo_sym_debug(&entry->sym_table);
    }
    if(glo_flag.reloc_table){
        printf("Total Reloc Item:%d\n",entry->reloc_table.size);
        for (int i = 0; i<entry->reloc_table.size; i++) {
            printf("0x%llx;\n",*(u64*)vec_at(&entry->reloc_table, i));
        }
    }
    FILE *f = fopen("core.bin", "wc");
    fwrite(entry->jit_compiled, entry->jit_cur, 1, f);
    fclose(f);
    printf("JIT returned:%d\n",start());
    if(glo_flag.need_obj && glo_flag.dst){
        module_pack(entry, glo_flag.dst);
    }
    return 0;
}