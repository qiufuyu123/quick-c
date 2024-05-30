#ifndef _H_PARSER
#define _H_PARSER

#include "define.h"
#include "hashmap.h"
#include "lex.h"
#include "vm.h"

typedef struct{
    module_t *m;
    Lexer_t *l;
    u64 loop_reloc;
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

typedef struct{
    int src_start;
    int src_num;
    char *dst;
    u64 code_base;
    bool need_obj;
    bool reloc_table;
    bool glo_sym_table;
    bool need_qlib;
}flgs_t;

#define VAR_EXIST_LOC (hashmap_get(&p->m->local_sym_table,&p->l->code[p->l->tk_now.start], p->l->tk_now.length))
void* var_exist_glo(parser_t *p);

int var_get_base_len(char type);
void expr_root(parser_t*p,var_t*inf);
bool expr_prim(parser_t*p,var_t*inf,bool leftval);
void prep_assign(parser_t *p,var_t *v);
void assignment(parser_t *p,var_t *v);
void trigger_parser_err(parser_t* p,const char *s,...);
bool expr_base(parser_t *p,var_t *inf,bool needptr);

module_t* module_compile(char *path,char *module_name, int name_len,bool is_module,module_t *previous);
int link_local(module_t *mod,u64 base_data, u64 base_code, u64 base_str);
int link_jit(module_t *mod);
#endif