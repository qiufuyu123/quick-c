#include "define.h"
#include "hashmap.h"
#include "lex.h"
#include "parser.h"
#include "vec.h"
#include "vm.h"
#include <stdio.h>

extern u64 debuglibs[2];

#define VALID_REG(i) (((i<6)||(i>9))&&(i!=REG_DX)&&(i!=REG_CX)&&(i != REG_SP)&&(i != REG_BP)&&(i != REG_R13)&&(i != REG_R12))

void load_const(parser_t *p, var_t *left){
    if(left->is_const == 0){
        return;
    }
    char r = acquire_reg(p);
    emit_load(p->m, r, left->base_addr);
    left->reg_used = r;
    left->is_const = 0;
}

void save_raxrbx(parser_t *p,var_t* left, var_t *right){
    if(left->reg_used != REG_AX && p->reg_table.reg_used[REG_AX]){
        emit_push_reg(p->m,REG_AX);
    }
    if(right->reg_used != REG_BX && p->reg_table.reg_used[REG_BX]){
        emit_push_reg(p->m,REG_BX);
    }
}

void release_raxrbx(parser_t *p,var_t *left, var_t *right){
    if(right->reg_used != REG_BX && p->reg_table.reg_used[REG_BX]){
        emit_pop_reg(p->m,REG_BX);
    }
    if(left->reg_used != REG_AX && p->reg_table.reg_used[REG_AX]){
        emit_pop_reg(p->m,REG_AX);
    }
}

char acquire_reg(parser_t *p){
    char r = p->reg_table.next_free;
    if(r != REG_FULL)
    {
        p->reg_table.next_free = REG_FULL;
        p->reg_table.reg_used[r] = 1;
        return r;
    }
    for (int i = 0; i<16; i++) {
        if(VALID_REG(i)&&(p->reg_table.reg_used[i] == 0)){
            p->reg_table.reg_used[i] = 1;
            //printf("alloc %d\n",i);
            return i;
        }
    }
    trigger_parser_err(p, "No register available!");
    return REG_FULL;
}

void release_reg(parser_t *p,char r){
    p->reg_table.reg_used[r] = 0;
    p->reg_table.next_free = r;
}

void save_usedreg(parser_t *p){
    for (int i = 0; i<16; i++) {
        if(VALID_REG(i)&&(p->reg_table.reg_used[i])){
            emit_push_reg(p->m, i);
        }
    }
}

void release_usedreg(parser_t *p){
    for (int i = REG_FULL-1; i>=0; i--) {
        if(VALID_REG(i)&&(p->reg_table.reg_used[i])){
            emit_pop_reg(p->m, i);
        }
    }
}

void* var_exist_glo(parser_t *p){
    var_t *ret = hashmap_get(&p->m->sym_table,&p->l->code[p->l->tk_now.start], p->l->tk_now.length);
    return ret;
}

void prepare_calling(parser_t*p, char call_r){
    var_t inf;
    int no = 1,old_no = p->caller_regs_used;
    for (int i = 1; i<=p->caller_regs_used; i++) {
        backup_caller_reg(p->m, i);
    }
    while (!lexer_skip(p->l, ')')) {
        lexer_next(p->l);
        expr(p, &inf,OPP_Assign,0);
        if(inf.type == TP_CUSTOM && inf.ptr_depth == 0){
            trigger_parser_err(p, "Cannot pass a structure as argument!");
        }
        if(no >= 7)
            trigger_parser_err(p, "Too many arguments!");
        if(no > p->caller_regs_used){
            p->caller_regs_used = no;
        }
        char target = no == 1?REG_DI:
                                no ==2?REG_SI:
                                no ==3?REG_DX:
                                no ==4?REG_CX:
                                no ==5?REG_R8:
                                REG_R9;
        if(inf.is_const)
            emit_load(p->m, target, inf.base_addr);
        else {
            emit_mov_r2r(p->m,target, inf.reg_used);
            release_reg(p, inf.reg_used);
        }
        no++;
        if(lexer_skip(p->l, ')')){
            break;
        }
        lexer_expect(p->l, ',');
    }
    lexer_next(p->l);

    p->reg_table.reg_used[call_r]=0;
    save_usedreg(p);
    u64 offset = emit_offset_stack(p->m);
    emit_call_reg(p->m, call_r);
    emit_mov_r2r(p->m, call_r, REG_AX);
    emit_restore_stack(p->m, offset);
    release_usedreg(p);

    p->reg_table.reg_used[call_r]=1;
    p->reg_table.next_free = REG_FULL;
    p->caller_regs_used = old_no;
    for (int i = p->caller_regs_used; i>=1; i--) {
        restore_caller_reg(p->m,i);
    }

    
}

void func_call(parser_t *p,var_t* inf){
    if(inf->type == TP_FUNC){
        
        function_frame_t *func = (function_frame_t*)inf->base_addr;
        // set up arguments
        prepare_calling(p,inf->reg_used);
        // emit_mov_r2r(p->m, REG_AX, REG_BX);
        //                
        
        inf->ptr_depth = func->ret_type.ptr_depth;
        inf->type = func->ret_type.builtin;
        inf->prot = func->ret_type.type;
    }else if(inf->ptr_depth){

        emit_mov_addr2r(p->m, inf->reg_used, inf->reg_used,TP_U64);
        // ax--> jump dst
        prepare_calling(p,inf->reg_used);
        inf->ptr_depth--;
    }else {
        printf("type:%d, ptr_depth:%d\n",inf->type,inf->ptr_depth);
        trigger_parser_err(p, "Cannot Call!");
    }
}

void prep_assign(parser_t *p,var_t *v){
    if(v->type == TP_CUSTOM && v->ptr_depth == 0)
        trigger_parser_err(p, "Struct is not supported");
    if(v->isglo){
        emit_loadglo(p->m, v->base_addr == 0?(u64)&v->base_addr:v->base_addr,v->reg_used,v->base_addr == 0);
    }
    else{
        emit_mov_r2r(p->m, v->reg_used, REG_BP);
        emit_sub_regimm(p->m,v->reg_used, v->base_addr);
    };
}

void assign_var(parser_t *p,var_t*dst,char src_reg){
    if(dst->ptr_depth){
        emit_mov_r2addr(p->m, dst->reg_used, src_reg, TP_U64);
        return;
    }
    if(dst->type == TP_CUSTOM){
        trigger_parser_err(p, "Unmatched type while assigning");
    }
    emit_mov_r2addr(p->m, dst->reg_used, src_reg, dst->type);
}

void assignment(parser_t *p,var_t *v){
    if(v->type >= TP_INTEGER && v->type < TP_CUSTOM){
        trigger_parser_err(p, "Cannot assign to a constant");
    }
    var_t left;
    expr(p, &left,OPP_Assign,1);
    if(left.type != TP_CUSTOM || left.ptr_depth){
        // basic types --> already loaded into rax
        assign_var(p, v,left.reg_used);
        release_reg(p, left.reg_used);
    }else {
        trigger_parser_err(p, "struct assign not support!");
    }
}

void array_visit(parser_t*p,var_t *inf,bool leftval){

    
    u64 offset = inf->ptr_depth?8:inf->type==TP_CUSTOM?inf->prot->len:var_get_base_len(inf->type);
    var_t left;
    lexer_next(p->l);
    expr(p, &left,OPP_Assign,0);
    lexer_expect(p->l, ']');
    if(left.is_const){
        if(left.base_addr != 0){
            left.reg_used = acquire_reg(p);
            emit_load(p->m, left.reg_used, left.base_addr * offset);
            emit_addr2r(p->m, inf->reg_used, left.reg_used);
            release_reg(p, left.reg_used);
        }
    }else{
        emit_push_reg(p->m, REG_DI);
        emit_push_reg(p->m, REG_AX);
        emit_push_reg(p->m, REG_BX);
        emit_mov_r2r(p->m, REG_AX, left.reg_used);
        //release_reg(p,left.reg_used);
        emit_load(p->m, REG_BX,offset );
        emit_push_reg(p->m, REG_DX);
        emit_mulrbx(p->m);
        emit_pop_reg(p->m, REG_DX);
        emit_mov_r2r(p->m, REG_DI, REG_AX);
        //emit_mov_r2r(p->m, REG_BX, REG_AX);
        emit_pop_reg(p->m, REG_BX);
        emit_pop_reg(p->m, REG_AX);
        emit_addr2r(p->m, inf->reg_used, REG_DI);
        emit_pop_reg(p->m, REG_DI);
        
        release_reg(p, left.reg_used);
        //inf->ptr_depth--;
    }
}


int struct_offset(parser_t*p ,proto_t *type,token_t name,proto_sub_t *sub_){
    //proto_debug(type);
    proto_sub_t *sub = (proto_sub_t*)hashmap_get(&type->subs, &p->l->code[name.start],name.length);
    if(sub){
        if(sub_)
            *sub_ = *sub;
        return sub->offset;
    }
    return -1;
}

void expr_ident(parser_t* p, var_t *inf){
    var_t *t = var_exist_glo(p);
    if(t == NULL){
        t = VAR_EXIST_LOC;
    }
    if(t == NULL){
        trigger_parser_err(p, "Cannot find symbol");
    }
    var_t parent = *t;
    *inf = parent;
    char r = acquire_reg(p);
    if(parent.isglo){
        if(parent.type == TP_FUNC){
            function_frame_t *fram = (function_frame_t*)parent.base_addr;
            if(fram->ptr == 0){
                // extern sym
                //printf("Load extern sym!\n");
                emit_loadglo(p->m, (u64)(t), r,1);
            }
            else if((u64)fram->ptr >= (u64)p->m->jit_compiled && (u64)fram->ptr < (u64)p->m->jit_compiled + p->m->jit_compiled_len){
                emit_loadglo(p->m, (u64)fram->ptr - (u64)p->m->jit_compiled,r,0);
            }
        }
        else emit_loadglo(p->m, parent.base_addr == 0?((u64)t):parent.base_addr,r,parent.base_addr == 0);
    }else{
        emit_mov_r2r(p->m, r, REG_BP);
        emit_sub_regimm(p->m, r, parent.base_addr);
        // emit_sub_rbx(p->m, parent.base_addr);
    }
    parent.reg_used = r;
    *inf = parent;
}


int token2opp(Tk type){
    static int opp_map[26][2] ={
        {'+',OPP_Add},
        {'-',OPP_Sub},
        {'*',OPP_Mul},
        {'/',OPP_Div},
        {'%',OPP_Mod},
        {'=',OPP_Assign},
        {TK_ADD_ASSIGN,OPP_Assign},
        {TK_SUB_ASSIGN,OPP_Assign},
        {'&',OPP_And},
        {'|',OPP_Or},
        {'^',OPP_Xor},
        {TK_OR,OPP_Or},
        {TK_AND,OPP_And},
        {TK_EQ,OPP_Eq},
        {TK_GE,OPP_Ge},
        {TK_LE,OPP_Le},
        {'>',OPP_Gt},
        {'<',OPP_Lt},
        {TK_NEQ,OPP_Ne},
        {'.',OPP_Dot},
        {'[',OPP_Bracket},
        {TK_ADD2,OPP_Inc},
        {TK_MINUS2,OPP_Dec},
        {TK_LSHL,OPP_Shl},
        {TK_LSHR,OPP_Shr},
        {'(',OPP_Fcall}
    };
    for (int i = 0; i<26; i++) {
        if(opp_map[i][0] == type){
            return opp_map[i][1];
        }
    }
    return 0;
}

bool is_numeric(parser_t *p,var_t left,var_t right,char is_op){
    if((is_op)&&(left.ptr_depth || right.ptr_depth)){
        if(var_get_base_len(left.type) != 1 || var_get_base_len(right.type)!=1){
            trigger_parser_err(p,"Operation between pointers are not allowed!(try convert it to u64)\n");
        }
    }
    return (left.ptr_depth || left.type < TP_INTEGER) && (right.ptr_depth || right.type < TP_INTEGER);
}

char load_var(parser_t *p,var_t *left){
    char newr = acquire_reg(p);
    if(left->type >= TP_I64 && left->type <= TP_U64){
        emit_mov_addr2r(p->m, newr, left->reg_used, TP_U64);
    }else {
        emit_binary(p->m, newr, newr, BOP_XOR,TP_U64);
        emit_mov_addr2r(p->m, newr, left->reg_used, left->ptr_depth?TP_U64:left->type);
    }
    return newr;
}

bool is_type(parser_t *p){
    token_t name = p->l->tk_now;
    if(name.type >= TK_I8 && name.type <= TK_U64)
        return 1;
    if(name.type != TK_IDENT){
        return 0;
    }
    
    if(hashmap_get(&p->m->prototypes, &p->l->code[name.start], name.length))
        return 1;
    return 0;
}

bool is_signed(char left, char right){
    return (left == TP_I8 || left == TP_I16 || left == TP_I32 || left == TP_I64);
}

extern flgs_t glo_flag;
bool expr(parser_t *p, var_t *inf,int ctx_priority,char no_const){
    if(glo_flag.opt_pass == 0){
        no_const = 1;
    }
    token_t token = p->l->tk_now;
    Tk type = token.type;
    var_t left = {0,0,0,0,0,0,0,0};
    bool need_load = 0;
    bool is_left_val = 0;
    bool is_left_only = (ctx_priority == OPP_LeftOnly);
    if(type == TK_INT){
        left.is_const = 1;
        left.base_addr = token.integer;
        
    }else if(type == '"'){
        int i = p->l->cursor;
        int len = 0;
        void* start =&p->l->code[i];
        while(p->l->code[i] != '\"'){
            i++;len++;
        }
        p->l->cursor=i+1;
        int v = (u64)module_add_string(p->m, TKSTR2VMSTR(start, len));
        // emit_load(p->m, REG_AX, v);
        if(v == -1){
            trigger_parser_err(p, "Fail to parse string constant!");
        }
        char r = acquire_reg(p);
        u64* addr = (u64*)emit_label_load(p->m, r);
        *addr = v | STRTABLE_MASK;
        module_add_reloc(p->m, (u64)addr - (u64)p->m->jit_compiled);
        left.reg_used = r;
        //printf("NEW str:%llx\n",v | STRTABLE_MASK);
        left.ptr_depth = 1;
        left.type = TP_U8;
    }else if(type == '+' ){
        lexer_next(p->l);
        expr(p, &left, OPP_Inc,0);
    }
    else if(type == '~'){
        lexer_next(p->l);
        expr(p, &left, OPP_Inc,0);
        // emit(p->m, 0x48);
        // emit(p->m, 0xf7);
        // emit(p->m, 0xd0); // not rax;
        if(!left.is_const){
            emit_unary(p->m, left.reg_used, UOP_NOT);
        }else {
            left.base_addr = ~left.base_addr;
        }
        
    }else if(type == '-'){
        lexer_next(p->l);
        expr(p, &left, OPP_Inc,0);
        if(left.type < TP_INTEGER){
            if(!left.is_const)
                emit_unary(p->m, left.reg_used, UOP_NEG);
            else
                left.base_addr = -left.base_addr;
        }else {
            trigger_parser_err(p, "Cannot neg!");
        }
    }else if(type == '&'){
        lexer_next(p->l);
        if(expr(p, &left, OPP_LeftOnly,1) == 0){
            trigger_parser_err(p, "& operator must follow a left value!");
        }
        
        left.ptr_depth++;
        // emit_mov_r2r(p->m, REG_AX, REG_BX);

    }else if(type == '*'){
        lexer_next(p->l);
        if(expr(p, &left, OPP_Inc,1)==0){
            // emit_mov_r2r(p->m, REG_BX, REG_AX);
        }
        if(left.ptr_depth == 0){
            trigger_parser_err(p, "* need a pointer!");
        }
        
        //load_var(p, left); // - notice sequence of 2 lines
        left.ptr_depth --; // -
        need_load = 1;
    }else if(type == TK_IDENT){
        is_left_val = 1;
        expr_ident(p, &left);
        if(lexer_skip(p->l, '(')){
            lexer_next(p->l);
            func_call(p, &left);
            is_left_val = 0;
        }else if(lexer_skip(p->l, '[')){
            if(left.is_arr == 0){
                if(left.ptr_depth){
                    emit_mov_addr2r(p->m, left.reg_used, left.reg_used,TP_U64);
                    left.ptr_depth--;
                }else{
                    trigger_parser_err(p, "array visit must be a pointer or array");
                }
            }
            left.is_arr = 0;
            lexer_next(p->l);
            array_visit(p, &left, 0);
            need_load = 1;
        }else {
            if(left.is_arr==0)
                need_load = 1;
        }
    }
    else if(type == TK_OFFSETOF){
        printf("offsetof!\n");
        lexer_expect(p->l, '(');
        lexer_next(p->l);
        if(!is_type(p)){
            trigger_parser_err(p, "Require a type here!");
        }
        if(p->l->tk_now.type != TK_IDENT){
            trigger_parser_err(p, "Require a custom type!");
        }
        proto_t *type = hashmap_get(&p->m->prototypes, &p->l->code[p->l->tk_now.start], p->l->tk_now.length);
        lexer_expect(p->l, ',');
        lexer_next(p->l);
        int offset= struct_offset(p, type, p->l->tk_now, 0);
        lexer_expect(p->l, ')');
        left.is_const = 1;
        left.base_addr = offset;
    }
    else if(type == TK_SIZEOF){
        //lexer_next(p->l);
        lexer_expect(p->l, '(');
        lexer_next(p->l);
        if(is_type(p)){
            
            int ptr_dep = 0;
            char builtin = TP_UNK;
            proto_t *prot = NULL;
            def_stmt(p, &ptr_dep, &builtin, &prot, 0, 0);
            lexer_now(p->l, ')');
            left.type = TP_U64;
            left.ptr_depth = 0;
            u64 size = 0;
            if(ptr_dep){
                size = 8;
            }else {
                if(builtin != TP_CUSTOM){
                    size = var_get_base_len(builtin);
                }else {
                    size = prot->len;
                }
            }
            left.is_const = 1;
            left.base_addr = size;
        }else {
            trigger_parser_err(p, "Expect a type!");
        }
    }
    else if(type == '('){
        lexer_next(p->l);
        if(is_type(p)){
            left.type = TP_UNK;
            int ptr_dep = 0;
            char builtin = TP_UNK;
            proto_t *prot = NULL;
            def_stmt(p, &ptr_dep, &builtin, &prot, 0, 0);
            lexer_next(p->l);
            expr(p, &left, OPP_Inc,0);
            left.ptr_depth = ptr_dep;
            left.prot = prot;
            left.type = builtin;
        }
        else{
            is_left_val = expr(p, &left, OPP_Assign,0);
            lexer_expect(p->l, ')');
        }
    }


    token = lexer_peek(p->l);
    type = token.type;
    while (type != ';' && token2opp(type) >= ctx_priority) {
        var_t right = {0,0,0,0,0};
        lexer_next(p->l);
        if(type == '='){
            if(need_load) need_load =0;
            lexer_next(p->l);
            assignment(p,&left);

        }
        else if(type == TK_ADD2 || type == TK_MINUS2){
            // add/minus, but return the original value
            if(!need_load){
                trigger_parser_err(p, "Inc/Dec must be applied to a left value!");
            }
            char newr = load_var(p, &left);
            emit_push_reg(p->m,newr);
            if(type == TK_ADD2){
                emit_unary(p->m, newr, UOP_INC);
            }else {
                emit_unary(p->m, newr, UOP_DEC);
            }
            assign_var(p, &left,newr);
            release_reg(p, left.reg_used);
            left.reg_used = newr;
            emit_pop_reg(p->m, newr);
            need_load = 0;
            is_left_val = 0;
        }else if(type == TK_ADD_ASSIGN || type == TK_SUB_ASSIGN){
            printf("+=/-=!\n");
            // add/minus, but return the original value
            if(!need_load){
                trigger_parser_err(p, "Inc/Dec must be applied to a left value!");
            }
            char newr = load_var(p, &left);
            emit_push_reg(p->m,newr);
            lexer_next(p->l);
            expr(p, &right, OPP_Assign,1);
            if(type == TK_ADD_ASSIGN){
                emit_addr2r(p->m, newr, right.reg_used);
            }else {
                emit_minusr2r(p->m, newr, right.reg_used);
            }
            release_reg(p, right.reg_used);
            assign_var(p, &left,newr);
            release_reg(p, left.reg_used);
            left.reg_used = newr;
            emit_pop_reg(p->m, newr);
            need_load = 0;
            is_left_val = 0;
        }
        else if(type == '('){
            func_call(p, &left);
            need_load = 0;
        }
        else if(type == '['){
            if(left.is_const){
                trigger_parser_err(p, "Cannot visit a constant value");
            }
            
            if(left.is_arr == 0){
                if(left.ptr_depth){
                    emit_mov_addr2r(p->m,left.reg_used,left.reg_used,TP_U64);
                    left.ptr_depth --;
                }else{
                    trigger_parser_err(p, "array visit needs a pointer!");
                }
            }
            printf("visit an array with ptr depth:%d\n",left.ptr_depth);
            array_visit(p, &left, 0);
            if(left.type != TP_CUSTOM)
                need_load = 1;
            left.is_arr = 0;
        }else if(type == '.'){
            
            if(left.ptr_depth && need_load){
                emit_mov_addr2r(p->m,left.reg_used,left.reg_used,TP_U64);
            }
            need_load = 0;
            token_t name_token = lexer_next(p->l);
            if(name_token.type != TK_IDENT){
                trigger_parser_err(p, "Need a word after '.'");
            }
            if(left.type != TP_CUSTOM){
                trigger_parser_err(p, ". or -> requires a struct!\n");
            }
            proto_sub_t pro_sub;
            int r = struct_offset(p, left.prot, name_token,&pro_sub);
            if(r == -1){
                trigger_parser_err(p, "Cannot find the member!");
            }
            left.prot = pro_sub.type;
            left.type = pro_sub.builtin;
            left.ptr_depth = pro_sub.ptr_depth;
            left.is_arr = pro_sub.is_arr;
            if(r)
                emit_add_regimm(p->m, left.reg_used, r);
            if(!left.is_arr)
                need_load = 1;
        }
        else {
    
            if(need_load && !is_left_only){
                char newr = load_var(p, &left);
                release_reg(p, left.reg_used);
                left.reg_used = newr;
                need_load = 0;
            }
            is_left_val = 0;
            if(type == '+' || type == '-'){
                // emit_pushrax(p->m);
                lexer_next(p->l);
                expr(p, &right, OPP_Mul,0);
                if(!is_numeric(p, left, right,1)){
                    trigger_parser_err(p, "Cannot add!");
                }
                if(left.is_const && right.is_const){
                    if(type == '+') left.base_addr += right.base_addr;
                    else            left.base_addr -= right.base_addr;
                }else{
                    load_const(p, &left);
                    load_const(p, &right);
                    if(type == '+'){
                        // emit_poprbx(p->m);
                        emit_addr2r(p->m, left.reg_used, right.reg_used);
                    }
                    else{
                        emit_minusr2r(p->m, left.reg_used, right.reg_used);
                    }
                    release_reg(p, right.reg_used);
                }
            }else if(type == '*' || type == '/' || type == '%'){
                lexer_next(p->l);
                //emit_pushrax(p->m);
                expr(p, &right, OPP_Inc,0);
                if(!is_numeric(p, left, right,1)){
                    trigger_parser_err(p, "Cannot add!");
                }
                if(left.is_const && right.is_const){
                    if(type == '*') left.base_addr *= right.base_addr;
                    else if(type == '/') left.base_addr /= right.base_addr;
                    else                 left.base_addr %= right.base_addr;
                }else{
                    load_const(p, &left);
                    load_const(p, &right);
                    save_raxrbx(p, &left, &right);
                    if(right.reg_used!= REG_BX){
                        emit_push_reg(p->m, right.reg_used);
                    }
                    if(left.reg_used != REG_AX){
                        emit_push_reg(p->m, left.reg_used);
                        emit_pop_reg(p->m, REG_AX);
                    }
                    if(right.reg_used != REG_BX)
                        emit_pop_reg(p->m, REG_BX);
                    if(p->reg_table.reg_used[REG_DX]){
                        emit_push_reg(p->m, REG_DX);
                    }
                    if(type == '*'){
                        emit_mulrbx(p->m);
                        emit_mov_r2r(p->m, REG_R13, REG_AX);

                    }else if(type == '/'){
                        emit_divrbx(p->m);
                        emit_mov_r2r(p->m, REG_R13, REG_AX);
                    }else{
                        emit_divrbx(p->m);
                        emit_mov_r2r(p->m, REG_R13, REG_DX);
                    }
                    if(p->reg_table.reg_used[REG_DX]){
                        emit_pop_reg(p->m, REG_DX);
                    }

                    release_raxrbx(p, &left, &right);
                    release_reg(p, right.reg_used);
                    emit_mov_r2r(p->m, left.reg_used, REG_R13);
                }
            }else if(type == '>' || type == '<' || type == TK_NEQ || type == TK_EQ || type == TK_LE || type == TK_GE){
                bool lt = type =='<',gt = type == '>',ne = type == TK_NEQ,eql = type == TK_EQ,fle = type == TK_LE;
                lexer_next(p->l);
                if(eql || ne){
                    expr(p, &right, OPP_Lt,0);
                }else {
                    expr(p,&right,OPP_Shl,0);
                }
                if(!is_numeric(p, left, right,0)){
                    trigger_parser_err(p, "Cannot compare!");
                }
                if(left.is_const && right.is_const){
                    if     (lt) left.base_addr = (left.base_addr > right.base_addr);
                    else if(gt) left.base_addr = (left.base_addr < right.base_addr);
                    else if(ne) left.base_addr = (left.base_addr != right.base_addr);
                    else if(eql) left.base_addr = (left.base_addr == right.base_addr);
                    else if(fle) left.base_addr = (left.base_addr <= right.base_addr);
                    else if(type == TK_GE) left.base_addr = (left.base_addr >= right.base_addr);

                }else{
                    load_const(p, &left);
                    load_const(p, &right);
                    char need_pop = 0;
                    if(left.reg_used != REG_AX && p->reg_table.reg_used[REG_AX]){
                        emit_push_reg(p->m, REG_AX);
                        need_pop = 1;
                    }
                    char isSigned = is_signed(left.type, right.type);
                    emit_binary(p->m, left.reg_used, right.reg_used, BOP_CMP,left.type);
                    emit(p->m,0x0f);
                    emit(p->m, lt ? (isSigned? 0x9c:0x92) /* < */ :
                            gt ? (isSigned? 0x9f:0x97) /* > */ :
                                    ne ? 0x95 /* != */ :
                                        eql ? 0x94 /* == */ :
                                            fle ? (isSigned?0x9e:0x96) /* <= */ :
                                                    isSigned? 0x9d:0x93 /* >= */);
                    
                        
                    emit(p->m, 0xc0); // setX al
                    emit(p->m, 0x48);emit(p->m, 0x0f);emit(p->m, 0xb6);emit(p->m, 0xc0); // mov %al,%rax
                    emit_mov_r2r(p->m, left.reg_used, REG_AX);
                    release_reg(p, right.reg_used);
                    if(need_pop)
                        emit_pop_reg(p->m, REG_AX);
                }
            }else if(type == '|' || type == '&' || type == '^' || type == TK_AND || type == TK_OR){
                bool or = (type == '|')||(type == TK_OR);
                bool and = (type == '&')||(type == TK_AND);
                bool xor = type == '^';
                // emit_pushrax(p->m);
                lexer_next(p->l);
                expr(p, &right, OPP_Assign,0);
                if(!is_numeric(p, left, right,1)){
                    trigger_parser_err(p, "Requre 2 numeric variable!");
                }
                if(left.is_const && right.is_const){
                    if(or)       left.base_addr |= right.base_addr;
                    else if(and) left.base_addr &= right.base_addr;
                    else if(xor) left.base_addr ^= right.base_addr;
                }else{
                    load_const(p, &left);
                    load_const(p, &right);
                    emit_binary(p->m, left.reg_used, right.reg_used, and?BOP_AND:or?BOP_OR:BOP_XOR,TP_U64);
                    release_reg(p, right.reg_used);
                }

            }else if(type == TK_LSHL || type == TK_LSHR){
                bool lsl = type == TK_LSHL;
                // emit_pushrax(p->m);
                lexer_next(p->l);
                expr(p,&right,OPP_Add,0);
                if(left.is_const && right.is_const){
                    if(type == TK_LSHL) left.base_addr <<= right.base_addr;
                    else                left.base_addr >>= right.base_addr;
                }else{
                    load_const(p, &left);
                    load_const(p, &right);
                    emit_push_reg(p->m, REG_CX);

                    if(left.reg_used != REG_AX){
                        emit_push_reg(p->m,REG_AX);
                        emit_push_reg(p->m, left.reg_used);
                    }
                    emit_push_reg(p->m, right.reg_used);
                    emit_pop_reg(p->m, REG_CX);
                    if(left.reg_used != REG_AX)
                        emit_pop_reg(p->m, REG_AX);
                    emit(p->m, 0x48);emit(p->m, 0xd3);
                    if(lsl){
                        emit(p->m, 0xe0);
                    }else {
                        emit(p->m, 0xe8);
                    }
                    emit_mov_r2r(p->m, REG_R13, REG_AX);
                    if(left.reg_used != REG_AX)
                        emit_pop_reg(p->m, REG_AX);
                    emit_pop_reg(p->m, REG_CX);
                    release_reg(p, right.reg_used);
                    emit_mov_r2r(p->m, left.reg_used, REG_R13);
                }
            }
        }
        token = lexer_peek(p->l);
        type = token.type;
    }
    if(need_load && !is_left_only){
        char newr = load_var(p, &left);
        release_reg(p, left.reg_used);
        left.reg_used = newr;
        is_left_val = 0;
    }
    if(is_left_only && !is_left_val){
        trigger_parser_err(p, "A left-val is required!");
    }
    if(no_const && left.is_const){
        load_const(p, &left);
    }
    *inf = left;
    return is_left_val;

}

