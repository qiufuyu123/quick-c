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

#define VAR_EXIST_GLO (hashmap_get(&p->m->sym_table,&p->l->code[p->l->tk_now.start], p->l->tk_now.length))
#define VAR_EXIST_LOC (hashmap_get(&p->m->local_sym_table,&p->l->code[p->l->tk_now.start], p->l->tk_now.length))


int var_get_base_len(char type);
void expr_root(parser_t*p,var_t*inf);
void expr_prim(parser_t*p,var_t*inf,bool leftval);
void prep_assign(parser_t *p,var_t *v);
void assignment(parser_t *p,var_t *v);
void trigger_parser_err(parser_t* p,const char *s,...);

void parser_start(module_t *m,Lexer_t* lxr);

#endif