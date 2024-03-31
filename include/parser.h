#ifndef _H_PARSER
#define _H_PARSER

#include "lex.h"
#include "vm.h"

typedef struct{
    module_t *m;
    Lexer_t *l;
    bool isglo;
}parser_t;

enum op_priority
{
    LPP_ASSIGN = 10,
    LPP_LOGIC_OR = 9,
    LPP_LOGIC_AND = 9,
    LPP_BIT_OR = 8,
    LPP_BIT_XOR = 8,
    LPP_BIT_AND = 8,
    LPP_EQNEQ = 7,
    LPP_COMP = 7,
    LPP_BIT_SHIFT = 6,
    LPP_ADD = 5,
    LPP_MUL = 4,
    LPP_IDENT = 3
    
};

void parser_start(module_t *m,Lexer_t* lxr);

#endif