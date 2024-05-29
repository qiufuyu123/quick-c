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

void func_call(parser_t *p,var_t* inf){
    if(inf->type == TP_FUNC){
        lexer_next(p->l);
        function_frame_t *func = (function_frame_t*)inf->base_addr;
        // set up arguments
        prepare_calling(p);
        // emit_mov_r2r(p->m, REG_AX, REG_BX);
        //                
        emit(p->m, 0xff);emit(p->m,0xd0);
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
    if(v->isglo){
        emit_loadglo(p->m, v->base_addr == 0?(u64)&v->base_addr:v->base_addr,1,v->base_addr == 0);
    }
    else{
        emit_mov_r2r(p->m, REG_BX, REG_BP);
        emit_sub_rbx(p->m, v->base_addr);
    };
}


void assignment(parser_t *p,var_t *v){
    if(v->type >= TP_INTEGER && v->type < TP_CUSTOM){
        trigger_parser_err(p, "Cannot assign to a constant");
    }
    var_t left;
    emit(p->m, 0x53); // push rbx
    expr_root(p, &left);
    emit(p->m, 0x5b); // pop rbx
    if(left.type != TP_CUSTOM || left.ptr_depth){
        // basic types --> already loaded into rax
        
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

void array_visit(parser_t*p,var_t *inf,bool leftval){
    if(!inf->ptr_depth){
        trigger_parser_err(p, "'[' must be applied on a pointer!");
    }
    lexer_expect(p->l,'[');
    // emit(p->m, 0x53); // push rbx
    emit_pushrax(p->m);
    var_t left;
    lexer_next(p->l);
    expr_root(p, &left);
    lexer_expect(p->l, ']');
    emit_load(p->m, REG_BX, inf->ptr_depth-1?8:inf->type==TP_CUSTOM?inf->prot->len:var_get_base_len(inf->type));
    emit_mulrbx(p->m);
    emit_mov_r2r(p->m, REG_BX, REG_AX);
    emit_poprax(p->m);
    emit_addr2r(p->m, REG_AX, REG_BX);
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
                printf("Load extern sym!\n");
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


bool expr_prim(parser_t* p,var_t *inf,bool left_val_only){
    Tk t = p->l->tk_now.type;
    if(t == TK_INT){
        if(left_val_only){
            trigger_parser_err(p, "Number cannot be a left-value!");
        }
        emit_load(p->m, REG_AX, p->l->tk_now.integer);
    }else if(t == '+'){
        lexer_next(p->l);
        return expr_base(p,inf,left_val_only);
        
    }else if(t == '-'){
        lexer_next(p->l);
        var_t left;
        bool ret = expr_base(p,&left,left_val_only);
        if (left.type < TP_INTEGER) {
            
            // rax --> Im
            emit_load(p->m, REG_BX, 0);  // mov rbx,0
            emit_minusr2r(p->m, REG_BX, REG_AX); //sub rbx,rax
            emit_mov_r2r(p->m, REG_AX, REG_BX); //mov rax,rbx

        }else{
            trigger_parser_err(p, "'-' is only valid between numbers");
        }
        return ret;
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
        u64 v = (u64)module_add_string(p->m, TKSTR2VMSTR(start, len));
        // emit_load(p->m, REG_AX, v);
        
        u64* addr = (u64*)emit_label_load(p->m, 0);
        *addr = v | STRTABLE_MASK;
        module_add_reloc(p->m, (u64)addr - (u64)p->m->jit_compiled);
        printf("NEW str:%llx\n",v | STRTABLE_MASK);
        inf->ptr_depth = 1;
        inf->type = TP_U8;
        while(lexer_skip(p->l, '[')){
            array_visit(p, inf,0);
        }
        return 1;
        
    }else if(t == TK_IDENT){
        expr_ident(p, inf);
        return inf->type == TP_FUNC;
    }else if(t == '('){
        lexer_next(p->l);
        expr_root(p, inf);
        lexer_expect(p->l, ')');
        return 1;
    }else if(t == '&'){
        if(left_val_only){
            trigger_parser_err(p, "Pointer cannot be a left-value");
        }
        lexer_next(p->l);
        var_t left;
        expr_base(p, &left,1);
        emit_mov_r2r(p->m, REG_AX, REG_BX);
        inf->ptr_depth++;
    }else if(t == '*'){
        if(left_val_only)
            trigger_parser_err(p, "De-ptr cannot be a left val");
        lexer_next(p->l);
        var_t left;
        expr_base(p, &left,0);
        if(left.ptr_depth == 0){
            trigger_parser_err(p, "De-ptr must be operated on a pointer!");
        }
        *inf = left;
        if(lexer_skip(p->l, '=')){
            lexer_next(p->l);lexer_next(p->l);

            if(left.isglo)
                emit_mov_r2r(p->m, REG_BX, REG_AX);
            assignment(p, &left);
            return 1;
        }
        //De-ptr
        emit_mov_addr2r(p->m, REG_AX, REG_AX);
        return 1;
        
    }else if(t == TK_NEW){
        // token_t name = lexer_next(p->l);
        // proto_t *type = hashmap_get(&p->m->prototypes, &p->l->code[name.start], name.length);
        // if(!type){
        //     trigger_parser_err(p, "Struct does not exist!");
        // }
        // emit_load(p->m, REG_AX, (u64)vec_reserv(&p->m->heap, type->len));
        // proto_impl(p->m, type);
        // inf->type = TP_CUSTOM;
        // inf->prot = type;
        // inf->ptr_depth = 1;
        // return 1;
    }
    else{
        trigger_parser_err(p, "Unexpected token");
    }
    inf->type = TP_U64;
    return 1;
}

bool expr_base(parser_t *p,var_t *inf,bool ptr_only){
    var_t left;
    bool need_load = 1;
    bool is_const = expr_prim(p, &left, 0);
    while (1) {
        
        if(lexer_skip(p->l, '.') && left.type == TP_CUSTOM){
            if(left.ptr_depth){
                emit_mov_addr2r(p->m, REG_BX, REG_BX);
            }
            lexer_next(p->l);
            token_t name = lexer_next(p->l);
            proto_sub_t sub;
            if(name.type != TK_IDENT){
                trigger_parser_err(p, "Struct visit '.' needs a name!");
            }
            int r = struct_offset(p, left.prot, name,&sub);
            if(r == -1){
                trigger_parser_err(p, "Fail to find a member with this name!");
            }

            // we ignore 0 offset here
            if(r){
                emit_add_rbx(p->m, r);
            }
            left.prot = sub.type;
            left.type = sub.builtin;
            left.ptr_depth = sub.ptr_depth;
            
        }else if(lexer_skip(p->l, '[')){
            if(!left.ptr_depth){
                trigger_parser_err(p, "'[' can only be applied to a pointer");
            }
            
            // offset=0;
            emit_mov_r2r(p->m, REG_AX, REG_BX);
            array_visit(p, &left,1);
            emit_mov_r2r(p->m, REG_BX, REG_AX);
            // while(lexer_skip(p->l, ']')){
            //     emit_mov_addr2r(p->m, REG_AX, REG_BX);
            //     array_visit(p, &parent,1);
            // }
            // *inf = parent;
            //trigger_parser_err(p, "Array visit not realized!");
        }else if(lexer_skip(p->l, '(')){
            func_call(p, &left);
            emit_mov_r2r(p->m, REG_BX, REG_AX);
            //parent.type = TP_U64;
            need_load = 0;
        }else {
           break;
        }
    }
    *inf = left;
    if(lexer_skip(p->l, '=')){
            // possible assignment:
            lexer_next(p->l);lexer_next(p->l);
            assignment(p, inf);
    }
    else if(need_load && !ptr_only && !is_const){
        if(inf->type == TP_CUSTOM && inf->ptr_depth == 0){
            trigger_parser_err(p, "Cannot load a structure!");
        }
        load_b2a(p, inf->ptr_depth?TP_U64:inf->type);
    }

    return 0;
}

void expr_muldiv(parser_t *p,var_t *inf){
    var_t left,right;
    expr_base(p,&left,0);
    bool mul=0,div=0,mod=0;
    while ((mul = lexer_skip(p->l, '*'))||
            (div = lexer_skip(p->l, '/'))||
            (mod = lexer_skip(p->l, '%'))) {
        emit_pushrax(p->m); // push rax
        lexer_next(p->l); //skip operand
        lexer_next(p->l);
        expr_base(p,&right,0);
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