#include "parser.h"
#include "define.h"
#include "hashmap.h"
#include "lex.h"
#include "lib/console.h"
#include "vec.h"
#include "vm.h"
#include "debuglib.h"


#include <bits/types/FILE.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <sys/mman.h>


#define VAR_EXIST_GLO (hashmap_get(&p->m->sym_table,&p->l->code[p->l->tk_now.start], p->l->tk_now.length))
#define VAR_EXIST_LOC (hashmap_get(&p->m->local_sym_table,&p->l->code[p->l->tk_now.start], p->l->tk_now.length))

void trigger_parser_err(parser_t* p,const char *s,...){
    va_list va;
    char buf[100]={0};
    va_start(va, s);
    vsprintf(buf, s, va);
    va_end(va);
    printf("Parser: Line:%d\nNear:%s\n",p->l->line,lex_get_line(p->l));
    printf("Error: %s\n",buf);
    exit(1);
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

void expr_root(parser_t*p,var_t*inf);

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
    }else{
        emit_mov_addr2r(p->m, REG_AX, REG_BX);
        // ax--> jump dst
        lexer_next(p->l);
        prepare_calling(p);
        emit(p->m, 0xff);emit(p->m,0xd0); // callq [rax]
    }
}

void prep_assign(parser_t *p,var_t *v){
    if(v->type == TP_CUSTOM && v->ptr_depth == 0)
        trigger_parser_err(p, "Struct is not supported");
    if(v->isglo)
        emit_load(p->m, REG_BX, v->base_addr);
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

void ident_update_off(parser_t *p,var_t*parent, int offset){
    if(offset){
        if(parent->base_addr == 0){
            emit_load(p->m, REG_CX, offset);
            if(!parent->isglo){
                emit_addr2r(p->m, REG_BX, REG_SP);
                parent->isglo = TRUE;
            }
            emit_addr2r(p->m, REG_BX, REG_CX);
        }
        else {
            emit_load(p->m, REG_BX, parent->base_addr+offset);
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
                ident_update_off(p, &parent, offset);
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
    ident_update_off(p, &parent, offset);

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
        
        if(inf->isglo){    
            load_b2a(p, inf->ptr_depth?TP_U64: inf->type);
            return;
        }
        emit_rbpload(p->m, inf->type, inf->base_addr);
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
            emit_mov_r2r(p->m, REG_AX, REG_SP);
            emit_load(p->m, REG_BX, left.base_addr);
            emit_addr2r(p->m, REG_AX, REG_BX);
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



bool var_exist(parser_t*p){
    return VAR_EXIST_GLO || VAR_EXIST_LOC;
    
}

int var_get_base_len(char type){
    switch (type) {
        case TP_I8: case TP_U8:
            return 1;
        case TP_I16: case TP_U16:
            return 2;
        case TP_I32: case TP_U32:
            return 4;
        case TP_I64: case TP_U64:
            return 8;
    }
}

int def_stmt(parser_t *p,int *ptr_depth,char *builtin,proto_t** proto,token_t *name){

    token_t t = p->l->tk_now;
    
    *proto = NULL;
    if(t.type<TK_I8 || t.type>TK_U64){
        //not built-in type
        *proto =  hashmap_get(&p->m->prototypes,&p->l->code[t.start] , t.length);
        if(! *proto){
            trigger_parser_err(p, "Cannot find type!");
        }
        if(name){
            *name = t;
        }
        *builtin = TP_CUSTOM;
    }
    else{
        *builtin = t.type - TK_I8 + TP_I8;
    }
    *ptr_depth = 0;
    while (lexer_next(p->l).type == '*') {
        (*ptr_depth)++;
    }
    if(p->l->tk_now.type!=TK_IDENT){
        trigger_parser_err(p, "An identity is needed after a type(%d)",p->l->tk_now.type);
    }
    if(*builtin != TP_CUSTOM){
        return *ptr_depth?8: var_get_base_len(*builtin);
    }
    return *ptr_depth?8:(**proto).len;
}

void stmt(parser_t *p);

int arg_decl(parser_t *p,int offset,hashmap_t *dst,char *btin){
    int ptr_depth;
    char builtin;
    proto_t *type;
    
    int sz = def_stmt(p, &ptr_depth, &builtin, &type,NULL);
    var_t *r = var_new_base(builtin, offset+sz, ptr_depth, 0, type);
    token_t name = p->l->tk_now;
    hashmap_put(dst, &p->l->code[name.start], name.length, r);
    hashmap_put(&p->m->local_sym_table, &p->l->code[name.start], name.length, r);
    *btin = builtin;
    return sz;
}

int proto_decl(parser_t *p,int offset,hashmap_t *dst){
    int ptr_depth;
    char builtin;
    proto_t *type;
    int sz = def_stmt(p, &ptr_depth, &builtin, &type,NULL);
    token_t name = p->l->tk_now;
    proto_sub_t *prot = subproto_new(offset,builtin,type,ptr_depth);    

    hashmap_put(dst, &p->l->code[name.start], name.length, prot);
    return sz;
}

void restore_arg(parser_t *p,int no,int offset,char w){
    if(no > 6){
        trigger_parser_err(p, "Too many args!");
    }
    emit_mov_r2r(p->m,REG_AX, no == 1?REG_DI:
                                no ==2?REG_SI:
                                no ==3?REG_DX:
                                no ==4?REG_CX:
                                no ==5?REG_R8:
                                REG_R9
                                );
    emit_reg2rbp(p->m, w, offset);

}

void stmt_loop(parser_t *p){
    while (!lexer_skip(p->l, '}')) {
            lexer_next(p->l);
            stmt(p);
    }
}

var_t* var_def(parser_t*p){
    int ptr_depth;
    char builtin;
    proto_t *prot;
    function_frame_t *func = NULL;
    int sz = def_stmt(p, &ptr_depth, &builtin, &prot,NULL);
    if(var_exist(p)){
        trigger_parser_err(p, "variable is already existed!");
    }
    token_t name = p->l->tk_now;
    if(lexer_skip(p->l, '(')){
        if(!p->isglo){
            trigger_parser_err(p, "Cannot declare a function inside a function");
        }
        // function declare:
        lexer_next(p->l);
        u64* jmp = emit_jmp_flg(p->m);
        func = function_new((u64)jit_top(p->m));
        func->ret_type.builtin = builtin;
        func->ret_type.type = prot;
        func->ret_type.ptr_depth = ptr_depth;
        int offset= 0;
        
        p->isglo = FALSE;
        emit(p->m, 0x55);//push rbp
        emit_mov_r2r(p->m, REG_BP, REG_SP); // mov rbp,rsp
        u64* preserv = emit_offsetrsp(p->m, 0,1);
        int no  = 1;
        while (!lexer_skip(p->l, ')')) {
            lexer_next(p->l);
            sz = arg_decl(p, offset, &func->arg_table,&builtin);
            offset+=sz;
            restore_arg(p, no, offset, builtin);
            no++;
            if(lexer_skip(p->l, ')')){
                break;
            }
            lexer_expect(p->l, ',');
        }
        lexer_next(p->l);
        if(lexer_next(p->l).type != '{'){
            trigger_parser_err(p,"Expect function body");
        }
        
        p->m->stack=offset;

        var_t* nv = var_new_base(func?TP_FUNC:builtin, (u64)func, func?1:ptr_depth,p->isglo,prot);
        hashmap_put(&p->m->sym_table, &p->l->code[name.start], name.length, nv);
        stmt_loop(p);

        module_clean_stack_sym(p->m);
        lexer_next(p->l);
        p->m->stack = BYTE_ALIGN(p->m->stack, 16);
        printf("Total Allocate %d bytes on stack\n",p->m->stack);
        p->isglo = TRUE;
        if(p->m->stack < 128){
            *(u8*)(preserv)=p->m->stack;
        }else {
            *(u32*)(preserv)=p->m->stack;
        }
        
        *jmp = (u64)jit_top(p->m);
        
        return nv;
    }
    var_t* nv = var_new_base(func?TP_FUNC:builtin, 0, func?1:ptr_depth,p->isglo,prot);
    if(p->isglo){
        printf("variable alloc %d bytes on heap!\n",sz);
        nv->base_addr = func?(u64)func:(u64)vec_reserv(&p->m->heap,sz);
        hashmap_put(&p->m->sym_table, &p->l->code[name.start], name.length, nv);
    }else {
        printf("variable alloc %d bytes on stack!\n",sz);
        // notice here:
        p->m->stack+=sz;
        nv->base_addr = p->m->stack;
        
        hashmap_put(&p->m->local_sym_table, &p->l->code[name.start], name.length, nv);
    }
    return nv;

}

int expression(parser_t*p){
    Tk tt = p->l->tk_now.type;
    if(tt == TK_IDENT){
        token_t tk = p->l->tk_now;
        if(hashmap_get(&p->m->prototypes,&p->l->code[tk.start] ,tk.length)){
            var_t* v = var_def(p);
            return v->type==TP_FUNC;
        }
        // just assignment?
        var_t inf;
        expr_prim(p, &inf,FALSE);        
        // print the variable
        //
        // if(inf.type!=TP_CUSTOM && inf.isglo){
        //     emit_mov_addr2r(p->m, REG_AX, REG_AX);
        // }
        u64 addr = debuglibs[DBG_PRINT_INT];
        // load params
        //printf("will debug:%llx\n",inf.base_addr);
        if(!inf.isglo){
            emit_rbpload(p->m, inf.type, inf.base_addr);
        }
        emit_mov_r2r(p->m, REG_DI, REG_AX); // mov rcx,rax
        emit_load(p->m, REG_SI, inf.ptr_depth?TP_U64-TP_I8: inf.type - TP_I8); 
        //
        //int s = emit_call_enter(p->m, 2); // 2params
        emit_call(p->m, addr);
        //emit_call_leave(p->m, s);
        
    }else if(tt == TK_INT || tt == TK_FLOAT){
        //const expression?
        var_t inf;
        expr_root(p, &inf);
    }else {
        var_t *nv= var_def(p);
        if(nv->type == TP_FUNC)
            return 1;
        if(lexer_skip(p->l, '=')){

            //assignment
            lexer_next(p->l);lexer_next(p->l);
            prep_assign(p, nv);
            assignment(p, nv);
        }
    }
    return 0;
    
}

void stmt(parser_t *p){
    Tk tt= p->l->tk_now.type;
    if(tt == TK_STRUCT){
        token_t tk = lexer_next(p->l);
        if(tk.type != TK_IDENT){
            trigger_parser_err(p, "Struct needs an identity name!");
        }
        lexer_expect(p->l, '{');
        proto_t *new_type = proto_new(0);
        int offset=0;
        while (!lexer_skip(p->l, '}')) {
            lexer_next(p->l);
            offset+=proto_decl(p, offset, &new_type->subs);
            lexer_expect(p->l, ';');
        }
        lexer_next(p->l);
        new_type->len = offset;
        hashmap_put(&p->m->prototypes, &p->l->code[tk.start], tk.length, new_type);
        proto_debug(new_type);
    }else if(tt == TK_RETURN){
        if(p->isglo){
            trigger_parser_err(p, "Return must be used inside a function");
        }
        var_t inf;
        lexer_next(p->l);
        expr_root(p,&inf);
        if(inf.ptr_depth == 0 && inf.type == TP_CUSTOM){
            trigger_parser_err(p, "Return a struct is not allowed!");
        }
        emit(p->m, 0xc9); //leave
        emit(p->m, 0xc3);
    }else if(tt == TK_IF){
        lexer_expect(p->l, '(');
        lexer_next(p->l);
        var_t inf;
        expr_root(p,&inf);
        lexer_expect(p->l, ')');
        lexer_expect(p->l, '{');
        if(inf.type == TP_CUSTOM && inf.ptr_depth == 0)
            trigger_parser_err(p, "Cannot compare!");
        /*
            if al == 0:
                jmp else_end
            else:

            else:end

        */
        emit(p->m, 0x3c);emit(p->m,0x00); // cmp al,0
        emit(p->m, 0x75);emit(p->m,0x0c);  // jne +0x0c +
        u64* els = emit_jmp_flg(p->m);  // jmpq rax |
                                                       //        <-+
        stmt_loop(p);
        lexer_next(p->l);
        *els = (u64)jit_top(p->m);
        if(lexer_skip(p->l, TK_ELSE)){
            lexer_next(p->l);
            lexer_expect(p->l, '{');
            u64 *els_end = emit_jmp_flg(p->m);
            *els = (u64)jit_top(p->m);
            stmt_loop(p);
            lexer_next(p->l);
            *els_end = (u64)jit_top(p->m);
        }
        return;
    }
    else {
        if(expression(p))
            return;
    }
    lexer_expect(p->l, ';');
}

void parser_start(module_t *m,Lexer_t* lxr){
    parser_t p;
    p.l = lxr;
    p.m = m;
    p.isglo = TRUE;
        qc_lib_console(m);

    emit(m, 0x55);
    emit_mov_r2r(m, REG_BP, REG_SP);
    while (!lexer_skip(lxr, TK_EOF)) {
        lexer_next(lxr);
        stmt(&p);
    }
    emit(m, 0xc9); // leave
    emit(m, 0xc3); // ret
    int (*test)() = m->jit_compiled;
    FILE *f = fopen("core.bin", "wc");
    fwrite(m->jit_compiled, m->jit_cur, 1, f);
    fclose(f);
    printf("JIT(%d bytes) CALLED:%d\n",m->jit_cur, test());
}
