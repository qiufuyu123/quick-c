#include "define.h"
#include "lex.h"
#include "vm.h"
#include"parser.h"
#include <bits/types/FILE.h>
#include <stdio.h>
#include <stdlib.h>

int main(int argc,char**argv){
    Lexer_t lex;
    char *path = "./test/fib.qc";
    if(argc==2){
        path = argv[1];
        // printf("Please input codes from stdin!");
        // exit(1);
    }
    FILE *fd = fopen(path, "r");
    char buf[1024]={0};
    fread(buf, 1024, 1, fd);
    fclose(fd);
    printf("%s",buf);
    lexer_init(&lex, "debug.q.c", buf);
    module_t mod;
    lex.cursor=0;
    lexer_debug(buf);
    module_init(&mod, "debug");
    parser_start(&mod, &lex);
    return 0;
}