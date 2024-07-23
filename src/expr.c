#include "define.h"
#include "hashmap.h"
#include "lex.h"
#include "parser.h"
#include "vec.h"
#include "vm.h"
#include <stdio.h>

extern u64 debuglibs[2];


void* var_exist_glo(parser_t *p){
    var_t *ret = hashmap_get(&p->m->sym_table,&p->l->code[p->l->tk_now.start], p->l->tk_now.length);
    return ret;
}

void load_b2a(parser_t*p, char type){
    if(type < TP_I64){
        emit(p->m, 0x48);emit(p->m, 0x31);emit(p->m, 0xc0); // xor rax,rax
    }
    switch (type) {
            case TP_I8: case TP_U8:
                emit(p->m, 0x8a);emit(p->m, 0x03);
                break;
            case TP_I16: case TP_U16:
                emit(p->m, 0x66);emit(p->m, 0x8b);emit(p->m, 0x03);
                break;
            case TP_I32: case TP_U32:
                emit(p->m, 0x8b);emit(p->m, 0x03);
                break;
            case TP_I64: case TP_U64:
                emit(p->m, 0x48); emit(p->m, 0x8b);emit(p->m, 0x03);
                break;
            case TP_CUSTOM:
                break;
    }

}



void prepare_calling(parser_t*p){
    var_t inf;
    int no = 1,old_no = p->caller_regs_used;
    for (int i = 1; i<=p->caller_regs_used; i++) {
        backup_caller_reg(p->m, i);
    }
    while (!lexer_skip(p->l, ')')) {
        if(no == 1){
            emit_pushrax(p->m);
            emit(p->m, 0x53); // push rbx
        }
        lexer_next(p->l);
        expr(p, &inf,OPP_Assign);
        if(inf.type == TP_CUSTOM && inf.ptr_depth == 0){
            trigger_parser_err(p, "Cannot pass a structure as argument!");
        }
        if(no >= 7)
            trigger_parser_err(p, "Too many arguments!");
        if(no > p->caller_regs_used){
            p->caller_regs_used = no;
        }
        emit_mov_r2r(p->m, no == 1?REG_DI:
                                no ==2?REG_SI:
                                no ==3?REG_DX:
                                no ==4?REG_CX:
                                no ==5?REG_R8:
                                REG_R9
                                , REG_AX);
        no++;
        if(lexer_skip(p->l, ')')){
            break;
        }
        lexer_expect(p->l, ',');
    }

    

    lexer_next(p->l);
    if(no > 1){
        emit(p->m, 0x5b); // pop rbx
        emit_poprax(p->m);
    }
    emit(p->m, 0xff);emit(p->m,0xd0);
    p->caller_regs_used = old_no;
    for (int i = p->caller_regs_used; i>=1; i--) {
        restore_caller_reg(p->m,i);
    }
    
}

void func_call(parser_t *p,var_t* inf){
    if(inf->type == TP_FUNC){
        
        function_frame_t *func = (function_frame_t*)inf->base_addr;
        // set up arguments
        prepare_calling(p);
        // emit_mov_r2r(p->m, REG_AX, REG_BX);
        //                
        
        inf->ptr_depth = func->ret_type.ptr_depth;
        inf->type = func->ret_type.builtin;
        inf->prot = func->ret_type.type;
    }else if(inf->ptr_depth){
        emit_mov_addr2r(p->m, REG_AX, REG_BX);
        // ax--> jump dst
        prepare_calling(p);
    }else {
        trigger_parser_err(p, "Cannot Call!");
    }
}

void prep_assign(parser_t *p,var_t *v){
    if(v->type == TP_CUSTOM && v->ptr_depth == 0)
        trigger_parser_err(p, "Struct is not supported");
    if(v->isglo){
        emit_loadglo(p->m, v->base_addr == 0?(u64)&v->base_addr:v->base_addr,1,v->base_addr == 0);
    }
    else{
        emit_mov_r2r(p->m, REG_BX, REG_BP);
        emit_sub_rbx(p->m, v->base_addr);
    };
}

void assign_var(parser_t *p,var_t*v){
    if(v->ptr_depth){
        emit(p->m,0x48);emit(p->m, 0x89);emit(p->m, 0x03);
        return;
    }
    if(v->type == TP_CUSTOM){
        trigger_parser_err(p, "Unmatched type while assigning");
    }
    switch (v->type) {
        case TP_I8: case TP_U8:
            emit(p->m, 0x88);emit(p->m, 0x03);
            break;
        case TP_I16: case TP_U16:
            emit(p->m, 0x66);emit(p->m, 0x89);emit(p->m, 0x03);
            break;
        case TP_I32: case TP_U32:
            emit(p->m, 0x89);emit(p->m, 0x03);
            break;
        default:
            emit(p->m,0x48);emit(p->m, 0x89);emit(p->m, 0x03);
            break;
    
    }
}

void assignment(parser_t *p,var_t *v){
    if(v->type >= TP_INTEGER && v->type < TP_CUSTOM){
        trigger_parser_err(p, "Cannot assign to a constant");
    }
    var_t left;
    emit(p->m, 0x53); // push rbx
    expr(p, &left,OPP_Assign);
    emit(p->m, 0x5b); // pop rbx
    if(left.type != TP_CUSTOM || left.ptr_depth){
        // basic types --> already loaded into rax
        assign_var(p, v);

    }else {
        trigger_parser_err(p, "struct assign not support!");
    }
}

void array_visit(parser_t*p,var_t *inf,bool leftval){
    if(!inf->ptr_depth){
        trigger_parser_err(p, "'[' must be applied on a pointer!");
    }
    emit_pushrax(p->m);
    emit_pushrbx(p->m);
    var_t left;
    lexer_next(p->l);
    expr(p, &left,OPP_Assign);
    lexer_expect(p->l, ']');
    emit_load(p->m, REG_BX, inf->ptr_depth-1?8:inf->type==TP_CUSTOM?inf->prot->len:var_get_base_len(inf->type));
    emit_mulrbx(p->m);
    //emit_mov_r2r(p->m, REG_BX, REG_AX);
    emit_poprbx(p->m);
    emit_addr2r(p->m, REG_BX, REG_AX);
    emit_poprax(p->m);
    inf->ptr_depth--;

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
    if(parent.isglo){
        if(parent.type == TP_FUNC){
            function_frame_t *fram = (function_frame_t*)parent.base_addr;
            if(fram->ptr == 0){
                // extern sym
                //printf("Load extern sym!\n");
                emit_loadglo(p->m, (u64)(t), 0,1);
            }
            else if((u64)fram->ptr >= (u64)p->m->jit_compiled && (u64)fram->ptr < (u64)p->m->jit_compiled + p->m->jit_compiled_len){
                emit_loadglo(p->m, (u64)fram->ptr - (u64)p->m->jit_compiled,0,0);
            }
        }
        else emit_loadglo(p->m, parent.base_addr == 0?((u64)t):parent.base_addr,1,parent.base_addr == 0);
    }else{
        emit_mov_r2r(p->m, REG_BX, REG_BP);
        emit_sub_rbx(p->m, parent.base_addr);
    }
    
    *inf = parent;
}


int token2opp(Tk type){
    static int opp_map[24][2] ={
        {'+',OPP_Add},
        {'-',OPP_Sub},
        {'*',OPP_Mul},
        {'/',OPP_Div},
        {'%',OPP_Mod},
        {'=',OPP_Assign},
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
    for (int i = 0; i<24; i++) {
        if(opp_map[i][0] == type){
            return opp_map[i][1];
        }
    }
    return 0;
}

bool is_numeric(parser_t *p,var_t left,var_t right){
    if(left.ptr_depth || right.ptr_depth){
        printf("WARN: Operating between pointers are dangerous!\n");
    }
    return (left.ptr_depth || left.type < TP_INTEGER) && (right.ptr_depth || right.type < TP_INTEGER);
}

void load_var(parser_t *p,var_t left){
    if(left.type >= TP_I64 && left.type <= TP_U64){
        emit_mov_addr2r(p->m, REG_AX, REG_BX);
        // optimize
    }else {
        //emit_mov_r2r(p->m, REG_BX, REG_AX);
        load_b2a(p, left.ptr_depth?TP_U64:left.type);
    }
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

bool expr(parser_t *p, var_t *inf,int ctx_priority){
    token_t token = p->l->tk_now;
    Tk type = token.type;
    var_t left = {0,0,0,0,0};
    bool need_load = 0;
    bool is_left_val = 0;
    bool is_left_only = (ctx_priority == OPP_LeftOnly);
    if(type == TK_INT){
        emit_load(p->m, REG_AX, token.integer);
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
        
        u64* addr = (u64*)emit_label_load(p->m, 0);
        *addr = v | STRTABLE_MASK;
        module_add_reloc(p->m, (u64)addr - (u64)p->m->jit_compiled);
        //printf("NEW str:%llx\n",v | STRTABLE_MASK);
        inf->ptr_depth = 1;
        inf->type = TP_U8;
    }else if(type == '+' ){
        lexer_next(p->l);
        expr(p, &left, OPP_Inc);
    }
    else if(type == '~'){
        lexer_next(p->l);
        expr(p, &left, OPP_Inc);
        emit(p->m, 0x48);
        emit(p->m, 0xf7);
        emit(p->m, 0xd0); // not rax;
    }else if(type == '-'){
        lexer_next(p->l);
        expr(p, &left, OPP_Inc);
        if(left.type < TP_INTEGER){
            // rax --> Im
            emit_load(p->m, REG_BX, 0);  // mov rbx,0
            emit_minusr2r(p->m, REG_BX, REG_AX); //sub rbx,rax
            emit_mov_r2r(p->m, REG_AX, REG_BX); //mov rax,rbx
        }
    }else if(type == '&'){
        lexer_next(p->l);
        if(expr(p, &left, OPP_LeftOnly) == 0){
            trigger_parser_err(p, "& operator must follow a left value!");
        }
        
        left.ptr_depth++;
        emit_mov_r2r(p->m, REG_AX, REG_BX);

    }else if(type == '*'){
        lexer_next(p->l);
        if(expr(p, &left, OPP_Inc)==0){
            emit_mov_r2r(p->m, REG_BX, REG_AX);
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
            emit_mov_r2r(p->m, REG_BX, REG_AX);
        }else if(lexer_skip(p->l, '[')){
            emit_mov_addr2r(p->m, REG_BX, REG_BX);
            lexer_next(p->l);
            array_visit(p, &left, 0);
            //left.ptr_depth--;
            need_load = 1;
        }else {
            need_load = 1;
        }
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
                    switch (builtin) {
                        case TP_U64: case TP_I64:
                            size = 8;
                            break;
                        case TP_U32: case TP_I32:
                            size = 4;
                            break;
                        case TP_U16: case TP_I16:
                            size = 2;
                            break;
                        default:
                            size = 1;
                            break;
                    }
                }else {
                    size = prot->len;
                }
            }
            emit_load(p->m, REG_AX, size);
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
            //lexer_expect(p->l, ')');
            lexer_next(p->l);
            expr(p, &left, OPP_Inc);
            left.ptr_depth = ptr_dep;
            left.prot = prot;
            left.type = builtin;
        }
        else{
            is_left_val = expr(p, &left, OPP_Assign);
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
            else emit_mov_r2r(p->m, REG_BX, REG_AX); // case for *a = b;
            lexer_next(p->l);
            assignment(p,&left);

        }
        else if(type == TK_ADD2 || type == TK_MINUS2){
            // add/minus, but return the original value
            if(!need_load){
                trigger_parser_err(p, "Inc/Dec must be applied to a left value!");
            }
            load_var(p, left);
            emit_pushrbx(p->m);
            emit_pushrax(p->m);
            emit(p->m, 0x48);emit(p->m, 0xff);
            if(type == TK_ADD2){
                emit(p->m, 0xc0);
            }else {
                emit(p->m, 0xc8);
            }
            assign_var(p, &left);
            emit_poprax(p->m);
            emit_poprbx(p->m);
            need_load = 0;
            is_left_val = 0;
        }
        else if(type == '('){
            func_call(p, &left);
            emit_mov_r2r(p->m, REG_BX, REG_AX);
            need_load = 0;
        }
        else if(type == '['){
            //lexer_next(p->l);
            emit_mov_addr2r(p->m, REG_BX, REG_BX);
            array_visit(p, &left, 0);
            need_load = 1;
        }else if(type == '.'){
            if(need_load) need_load = 0;
            if(left.ptr_depth){
                emit_mov_addr2r(p->m,REG_BX,REG_BX);
            }
            token_t name_token = lexer_next(p->l);
            if(name_token.type != TK_IDENT){
                trigger_parser_err(p, "Need a word after '.'");
            }
            proto_sub_t pro_sub;
            int r = struct_offset(p, left.prot, name_token,&pro_sub);
            if(r == -1){
                trigger_parser_err(p, "Cannot find the member!");
            }
            left.prot = pro_sub.type;
            left.type = pro_sub.builtin;
            left.ptr_depth = pro_sub.ptr_depth;
            emit_add_rbx(p->m, r);
            need_load = 1;
        }
        else {
    
            if(need_load && !is_left_only){
                load_var(p, left);
                need_load = 0;
            }
            is_left_val = 0;
            if(type == '+' || type == '-'){
                emit_pushrax(p->m);
                lexer_next(p->l);
                expr(p, &right, OPP_Mul);
                if(!is_numeric(p, left, right)){
                    trigger_parser_err(p, "Cannot add!");
                }
                
                if(type == '+'){
                    emit_poprbx(p->m);
                    emit_addr2r(p->m, REG_AX, REG_BX);
                }
                else{
                    emit_mov_r2r(p->m, REG_BX, REG_AX);
                    emit_poprax(p->m);
                    emit_minusr2r(p->m, REG_AX,REG_BX);
                }
            }else if(type == '*' || type == '/' || type == '%'){
                lexer_next(p->l);
                emit_pushrax(p->m);
                expr(p, &right, OPP_Inc);
                if(!is_numeric(p, left, right)){
                    trigger_parser_err(p, "Cannot add!");
                }
                emit_mov_r2r(p->m, REG_BX, REG_AX);
                emit_poprax(p->m);
                if(type == '*'){
                    emit_mulrbx(p->m);
                }else if(type == '/'){
                    emit_divrbx(p->m);
                }else{
                    emit_divrbx(p->m);
                    emit_mov_r2r(p->m, REG_AX, REG_DX);
                }
            }else if(type == '>' || type == '<' || type == TK_NEQ || type == TK_EQ || type == TK_LE || type == TK_GE){
                bool lt = type =='<',gt = type == '>',ne = type == TK_NEQ,eql = type == TK_EQ,fle = type == TK_LE;
                emit_pushrax(p->m);
                lexer_next(p->l);
                if(eql || ne){
                    expr(p, &right, OPP_Lt);
                }else {
                    expr(p,&right,OPP_Shl);
                }
                if(!is_numeric(p, left, right)){
                    trigger_parser_err(p, "Cannot compare!");
                }
                emit_mov_r2r(p->m, REG_BX, REG_AX);
                emit_poprax(p->m);
                emit(p->m, 0x48);emit(p->m, 0x39);emit(p->m, 0xd8); // cmp %rax,%rbx
                emit(p->m,0x0f);
                emit(p->m, lt ? 0x9c /* < */ :
                        gt ? 0x9f /* > */ :
                                ne ? 0x95 /* != */ :
                                    eql ? 0x94 /* == */ :
                                        fle ? 0x9e /* <= */ :
                                                0x9d /* >= */);
                emit(p->m, 0xc0); // setX al
                emit(p->m, 0x48);emit(p->m, 0x0f);emit(p->m, 0xb6);emit(p->m, 0xc0); // mov %al,%rax
            }else if(type == '|' || type == '&' || type == '^' || type == TK_AND || type == TK_OR){
                bool or = (type == '|')||(type == TK_OR);
                bool and = (type == '&')||(type == TK_AND);
                bool xor = type == '^';
                emit_pushrax(p->m);
                lexer_next(p->l);
                expr(p, &right, OPP_Assign);
                if(!is_numeric(p, left, right)){
                    trigger_parser_err(p, "Requre 2 numeric variable!");
                }
                emit_mov_r2r(p->m, REG_BX, REG_AX);
                emit_poprax(p->m);
                emit(p->m, 0x48);
                emit(p->m, and?0x21:or?0x09:0x31);
                emit(p->m, 0xd8);

            }else if(type == TK_LSHL || type == TK_LSHR){
                bool lsl = type == TK_LSHL;
                emit_pushrax(p->m);
                lexer_next(p->l);
                expr(p,&right,OPP_Add);
                emit(p->m, 0x48);emit(p->m, 0x31);emit(p->m, 0xc9); // xor rcx,rcx
                emit_mov_r2r(p->m, REG_CX, REG_AX);
                emit_poprax(p->m);
                emit(p->m, 0x48);emit(p->m, 0xd3);
                if(lsl){
                    emit(p->m, 0xe0);
                }else {
                    emit(p->m, 0xe8);
                }
                
            }
        }
        token = lexer_peek(p->l);
        type = token.type;
    }
    if(need_load && !is_left_only){
        load_var(p, left);
        is_left_val = 0;
    }
    if(is_left_only && !is_left_val){
        trigger_parser_err(p, "A left-val is required!");
    }
    *inf = left;
    return is_left_val;

}

