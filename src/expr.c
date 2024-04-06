#include "parser.h"

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
    int no = 1;
    while (!lexer_skip(p->l, ')')) {
        if(no == 1){
            emit_pushrax(p->m);
            emit(p->m, 0x53); // push rbx
        }
        lexer_next(p->l);
        expr_root(p, &inf);
        if(inf.type == TP_CUSTOM && inf.ptr_depth == 0){
            trigger_parser_err(p, "Cannot pass a structure as argument!");
        }
        if(no >= 7)
            trigger_parser_err(p, "Too many arguments!");
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
    if(no > 1){
        emit(p->m, 0x5b); // pop rbx
        emit_poprax(p->m);
    }

    lexer_next(p->l);
}

void func_call(parser_t *p,var_t* inf,bool useaddr){
    if(!useaddr && inf->type == TP_FUNC){
        lexer_next(p->l);
        function_frame_t *func = (function_frame_t*)inf->base_addr;
        // set up arguments
        prepare_calling(p);
        //                
        emit_call(p->m,func->ptr);
        inf->ptr_depth = func->ret_type.ptr_depth;
        inf->type = func->ret_type.builtin;
        inf->prot = func->ret_type.type;
    }else if(inf->ptr_depth){
        emit_mov_addr2r(p->m, REG_AX, REG_BX);
        // ax--> jump dst
        lexer_next(p->l);
        prepare_calling(p);
        emit(p->m, 0xff);emit(p->m,0xd0); // callq [rax]
    }else {
        trigger_parser_err(p, "Cannot Call!");
    }
}

void prep_assign(parser_t *p,var_t *v){
    if(v->type == TP_CUSTOM && v->ptr_depth == 0)
        trigger_parser_err(p, "Struct is not supported");
    if(v->isglo)
        emit_load(p->m, REG_BX, v->base_addr);
}


void assignment(parser_t *p,var_t *v){
    if(v->type >= TP_INTEGER && v->type < TP_CUSTOM){
        trigger_parser_err(p, "Cannot assign to a constant");
    }
    var_t left;
    if(v->isglo)
        emit(p->m, 0x53); // push rbx
    expr_root(p, &left);
    if(v->isglo)
        emit(p->m, 0x5b); // pop rbx
    if(left.type != TP_CUSTOM){
        // basic types --> already loaded into rax
        
        if(!v->isglo){
            emit_reg2rbp(p->m, v->ptr_depth?TP_U64:v->type, v->base_addr);
            //TODO: local variable
            return;
        }
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
    }else {
        trigger_parser_err(p, "struct assign not support!");
    }
}


int struct_offset(parser_t*p ,proto_t *type,token_t name,proto_sub_t *sub_){
    proto_debug(type);
    proto_sub_t *sub = (proto_sub_t*)hashmap_get(&type->subs, &p->l->code[name.start],name.length);
    if(sub){
        if(sub_)
            *sub_ = *sub;
        return sub->offset;
    }
    return -1;
}

void ident_update_off(parser_t *p,var_t*parent, int offset,bool force){
    if(offset){
        if(parent->base_addr == 0|| force){
            
            emit_load(p->m, REG_CX, offset);
            if(!parent->isglo){
                parent->isglo = TRUE;
            }
            emit_addr2r(p->m, REG_BX, REG_CX);
        }
        else {
            if(parent->isglo){
                emit_load(p->m, REG_BX, parent->base_addr+offset);
            }
            else {
                parent->base_addr-=offset;
            }
        }
    }
}

void expr_ident(parser_t* p, var_t *inf){
    var_t *t = VAR_EXIST_GLO;
    if(t == NULL){
        t = VAR_EXIST_LOC;
    }
    if(t == NULL){
        trigger_parser_err(p, "Cannot find symbol");
    }
    var_t parent = *t;
    *inf = parent;
    if(parent.type != TP_FUNC && parent.isglo){
        emit_load(p->m, REG_BX, parent.base_addr);
    }
    int offset = 0;
    bool is_fst = 0;
    while (1) {
        
        if(lexer_skip(p->l, '.') && parent.type == TP_CUSTOM){
            lexer_next(p->l);
            token_t name = lexer_next(p->l);
            if(name.type != TK_IDENT){
                trigger_parser_err(p, "Struct visit '.' needs a name!");
            }
            proto_sub_t sub;
            int r = struct_offset(p, parent.prot, name,&sub);
            if(r == -1){
                trigger_parser_err(p, "Fail to find a member with this name!");
            }
            
            inf->prot = sub.type;
            inf->ptr_depth = sub.ptr_depth;
            inf->type = sub.builtin;
            if(parent.ptr_depth){
                is_fst = 1;
                if(!parent.isglo){
                    emit_mov_r2r(p->m, REG_BX, REG_BP);
                    emit_load(p->m, REG_CX, parent.base_addr);
                    emit_minusr2r(p->m, REG_BX, REG_CX);
                }
                ident_update_off(p, &parent, offset,1);
                // emit_load(p->m, REG_CX, offset);
                // if(!p->isglo){
                //     emit_addr2r(p->m, REG_BX, REG_SP);
                //     parent.isglo = TRUE;
                // }
                // emit_addr2r(p->m, REG_BX, REG_CX);
                emit_mov_addr2r(p->m, REG_BX, REG_BX);
                offset = 0;
                // now base_addr is meaningless
                parent.base_addr = 0;
            }
            offset += r;
            parent.ptr_depth = sub.ptr_depth;
            parent.prot = sub.type;
            parent.type = sub.builtin;
            
        }else if(lexer_skip(p->l, '[')){
            trigger_parser_err(p, "Not supported yet");
        }else if(lexer_skip(p->l, '(')){
            if(!is_fst){
                parent.base_addr+=offset;
            }
            func_call(p, &parent,is_fst);
            emit_mov_r2r(p->m, REG_BX, REG_AX);
            parent.type = TP_U64;
            *inf = parent;
            inf->isglo = 2;
            is_fst=1;
        }else {
           break;
        }
    }
    ident_update_off(p, &parent, offset,0);
    inf->base_addr = parent.base_addr;
}

void expr_prim(parser_t* p,var_t *inf,bool left_val_only){
    Tk t = p->l->tk_now.type;
    if(t == TK_INT){
        if(left_val_only){
            trigger_parser_err(p, "Number cannot be a left-value!");
        }
        emit_load(p->m, REG_AX, p->l->tk_now.integer);
    }else if(t == '+'){
        lexer_next(p->l);
        expr_prim(p,inf,left_val_only);
        return;
    }else if(t == '-'){
        lexer_next(p->l);
        var_t left;
        expr_prim(p,&left,left_val_only);
        if (left.type < TP_INTEGER) {
            
            // rax --> Im
            emit_load(p->m, REG_BX, 0);  // mov rbx,0
            emit_minusr2r(p->m, REG_BX, REG_AX); //sub rbx,rax
            emit_mov_r2r(p->m, REG_AX, REG_BX); //mov rax,rbx

        }else{
            trigger_parser_err(p, "'-' is only valid between numbers");
        }
    }else if(t == '\"'){
        if(left_val_only){
            trigger_parser_err(p, "String cannot be a left-value");
        }
        //string constant
        int i = p->l->cursor;
        int len = 0;
        char end='\0';
        void* start =&p->l->code[i];
        while(p->l->code[i] != '\"'){
            i++;len++;
        }
        p->l->cursor=i+1;
        if(lexer_next(p->l).type == '['){
            //TODO
            lexer_next(p->l);
            
        }
        else {
            p->l->cursor=i+1;
            u64 v = (u64)vec_push_n(&p->m->heap,start,len);
            vec_push(&p->m->heap, &end);
            emit_load(p->m, REG_AX, v);
            inf->ptr_depth = 1;
            inf->type = TP_U8;
            return;
        }
        
    }else if(t == TK_IDENT){

        expr_ident(p, inf);
        if(inf->isglo == 2){
            // function call return
            inf->isglo = 1;
            return;
        }
        if(left_val_only)
            return;
        // only 2 possible:
        // inf.isglo ==> addr of it loaded to BX
        // !inf.isglo ==> nothing is loaded
        if(inf->type == TP_CUSTOM && inf->ptr_depth == 0){
                trigger_parser_err(p, "Struct loaded is not supported!");
        }

        if(lexer_skip(p->l, '=')){
            // possible assignment:
            lexer_next(p->l);lexer_next(p->l);
            assignment(p, inf);
            return;
        }
        
        if(inf->isglo || inf->base_addr == 0){    
            load_b2a(p, inf->ptr_depth?TP_U64: inf->type);
            return;
        }else
            emit_rbpload(p->m, inf->ptr_depth?TP_U64:inf->type, inf->base_addr);
        inf->isglo = TRUE;
        return;
    }else if(t == '('){
        lexer_next(p->l);
        expr_root(p, inf);
        lexer_expect(p->l, ')');
        return;
    }else if(t == '&'){
        if(left_val_only){
            trigger_parser_err(p, "Pointer cannot be a left-value");
        }
        lexer_next(p->l);
        var_t left;
        expr_prim(p, &left,TRUE);
        if(left.isglo){
            emit_mov_r2r(p->m, REG_AX, REG_BX);
        }else {
            emit_mov_r2r(p->m, REG_AX, REG_BP);
            emit_load(p->m, REG_BX, left.base_addr);
            emit_minusr2r(p->m, REG_AX, REG_BX);
        }
        inf->ptr_depth++;
    }else if(t == '*'){
        if(left_val_only)
            trigger_parser_err(p, "De-ptr cannot be a left val");
        lexer_next(p->l);
        var_t left;
        expr_prim(p, &left,FALSE);
        if(left.ptr_depth == 0){
            trigger_parser_err(p, "De-ptr must be operated on a pointer!");
        }
        *inf = left;
        if(lexer_skip(p->l, '=')){
            lexer_next(p->l);lexer_next(p->l);

            if(left.isglo)
                emit_mov_r2r(p->m, REG_BX, REG_AX);
            assignment(p, &left);
            return;
        }
        //De-ptr
        emit_mov_addr2r(p->m, REG_AX, REG_AX);
        return;
        
    }else{
        trigger_parser_err(p, "Unexpected token");
    }
    inf->type = TP_U64;
}

void expr_muldiv(parser_t *p,var_t *inf){
    var_t left,right;
    expr_prim(p,&left,FALSE);
    bool mul=0,div=0,mod=0;
    while ((mul = lexer_skip(p->l, '*'))||
            (div = lexer_skip(p->l, '/'))||
            (mod = lexer_skip(p->l, '%'))) {
        emit_pushrax(p->m); // push rax
        lexer_next(p->l); //skip operand
        lexer_next(p->l);
        expr_prim(p,&right,FALSE);
        if(left.type == TP_FLOAT || right.type == TP_FLOAT){
            trigger_parser_err(p, "Float operation is not supported");
        }
        
        emit_mov_r2r(p->m, REG_BX, REG_AX);
        emit_poprax(p->m);
        if(mul){
            emit_mulrbx(p->m);
        }else if(div){
            emit_divrbx(p->m);
        }else {
            emit_load(p->m, REG_DI, 0);
            emit_divrbx(p->m);
            emit_mov_r2r(p->m, REG_AX, REG_DX);
        }
    }
    *inf = left;
}

void expr_addsub(parser_t *p,var_t*inf){
    var_t left,right;
    expr_muldiv(p,&left);
    bool add=0,sub=0;
    while ((add = lexer_skip(p->l, '+'))||
            (sub = lexer_skip(p->l, '-'))) {
        emit_pushrax(p->m); // push rax
        lexer_next(p->l); //skip operand
        lexer_next(p->l);
        expr_muldiv(p,&right);
        if(left.type == TP_FLOAT || right.type == TP_FLOAT){
            trigger_parser_err(p, "Float operation is not supported");
        }
        emit_mov_r2r(p->m, REG_BX, REG_AX);
        emit_poprax(p->m);
        if(add){
            emit_addr2r(p->m,REG_AX,REG_BX);
        }else{
            emit_minusr2r(p->m,REG_AX,REG_BX);
        }
    }
    *inf = left;
}

void expr_condi(parser_t *p,var_t*inf){
    var_t left,right;
    expr_addsub(p,&left);
    bool lt = 0, gt = 0, ne = 0, eql = 0, fle = 0;
    while ((lt = lexer_skip(p->l, '<'))||
            (gt = lexer_skip(p->l, '>'))||
            (ne = lexer_skip(p->l, TK_NEQ))||
            (eql = lexer_skip(p->l, TK_EQ))||
            (fle = lexer_skip(p->l, TK_LE))||
            (lexer_skip(p->l, TK_GE))) {
        emit_pushrax(p->m); // push rax
        lexer_next(p->l); //skip operand
        lexer_next(p->l);
        expr_addsub(p,&right);
        if(left.type == TP_FLOAT || right.type == TP_FLOAT){
            trigger_parser_err(p, "Float operation is not supported");
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
        
    }
    *inf = left;
}

void expr_root(parser_t*p,var_t*inf){
    expr_condi(p, inf);
}