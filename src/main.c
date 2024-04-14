#include "define.h"
#include "lex.h"
#include "vm.h"
#include"parser.h"
#include <bits/types/FILE.h>
#include <stdio.h>
#include <stdlib.h>

int main(int argc,char**argv){
    Lexer_t lex;
    char *path = "./test/lib2.qc";
    if(argc==2){
        path = argv[1];
        // printf("Please input codes from stdin!");
        // exit(1);
    }
    module_liblist_init();
    module_t *entry= module_compile(path, "main", 4, 0);
    if(!entry){
        printf("Fail to compile module:'main'\n");
        return -1;
    }
    int(*start)() = entry->jit_compiled;
    FILE *f = fopen("core.bin", "wc");
    fwrite(entry->jit_compiled, entry->jit_cur, 1, f);
    fclose(f);
    printf("JIT returned:%d\n",start());
    
    return 0;
}