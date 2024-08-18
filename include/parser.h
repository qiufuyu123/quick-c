#ifndef _H_PARSER
#define _H_PARSER

#include "define.h"
#include "hashmap.h"
#include "lex.h"
#include "vm.h"

typedef struct{
    module_t *m;
    Lexer_t *l;
    u8 caller_regs_used;
    u64 loop_reloc;
    bool isglo;
    bool isend;
}parser_t;

enum op_priority
{

    OPP_Assign = 1, // operator =, keep Assign as highest priority operator
    OPP_OrAssign, OPP_XorAssign, OPP_AndAssign, OPP_ShlAssign, OPP_ShrAssign, // |=, ^=, &=, <<=, >>=
    OPP_AddAssign, OPP_SubAssign, OPP_MulAssign, OPP_DivAssign, OPP_ModAssign, // +=, -=, *=, /=, %=
    OPP_Cond, // operator: ?
    OPP_Lor, OPP_Lan, OPP_Or, OPP_Xor, OPP_And, // operator: ||, &&, |, ^, &
    OPP_Eq, OPP_Ne, OPP_Lt, OPP_Gt, OPP_Le, OPP_Ge, // operator: ==, !=, <, >, <=, >=
    OPP_Shl, OPP_Shr, OPP_Add, OPP_Sub, OPP_Mul, OPP_Div, OPP_Mod, // operator: <<, >>, +, -, *, /, %
    OPP_LeftOnly, OPP_Inc, OPP_Dec, OPP_Dot, OPP_Arrow, OPP_Fcall,OPP_Bracket, // operator: ++, --, ., ->, [
};

typedef struct{
    int src_start;
    int src_num;
    char *dst;
    char *glo_include;
    u64 code_base;
    bool need_obj;
    bool reloc_table;
    bool glo_sym_table;
    bool need_qlib;
    bool norun;
}flgs_t;

#define VAR_EXIST_LOC (hashmap_get(&p->m->local_sym_table,&p->l->code[p->l->tk_now.start], p->l->tk_now.length))
void* var_exist_glo(parser_t *p);


int def_stmt(parser_t *p,int *ptr_depth,char *builtin,proto_t** proto,token_t *name,bool need_var_name);
int var_get_base_len(char type);
bool expr(parser_t *p, var_t *inf,int ctx_priority);
void prep_assign(parser_t *p,var_t *v);
void assignment(parser_t *p,var_t *v);
void trigger_parser_err(parser_t* p,const char *s,...);
//bool expr_base(parser_t *p,var_t *inf,bool needptr);

module_t* module_compile(char *path,char *module_name, int name_len,bool is_module,module_t *previous);
int link_local(module_t *mod,u64 base_data, u64 base_code, u64 base_str);
int link_jit(module_t *mod);
#endif