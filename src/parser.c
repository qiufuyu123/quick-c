#include "parser.h"
#include "define.h"
#include "hashmap.h"
#include "lex.h"
#include "lib/console.h"
#include "lib/array.h"
#include "vec.h"
#include "vm.h"
#include "debuglib.h"


#include <bits/types/FILE.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <sys/mman.h>


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
        if(hashmap_get(&p->m->sym_table, "_debug_", 7)){
            u64 addr = debuglibs[DBG_PRINT_INT];
            // load params
            //printf("will debug:%llx\n",inf.base_addr);
            if(!inf.isglo){
                emit_rbpload(p->m, inf.ptr_depth?TP_U64:inf.type, inf.base_addr);
            }
            emit_mov_r2r(p->m, REG_DI, REG_AX); // mov rcx,rax
            emit_load(p->m, REG_SI, inf.ptr_depth?TP_U64-TP_I8: inf.type - TP_I8); 
            //
            //int s = emit_call_enter(p->m, 2); // 2params
            emit_call(p->m, addr);
            //emit_call_leave(p->m, s);
        }
        
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
    qc_lib_array(m);
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
