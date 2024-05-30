#include "parser.h"
#include "define.h"
#include "hashmap.h"
#include "lex.h"
#include "vec.h"
#include "vm.h"
#include "debuglib.h"

#include <math.h>
#include <setjmp.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>

static jmp_buf err_callback;

void trigger_parser_err(parser_t* p,const char *s,...){
    va_list va;
    char buf[100]={0};
    va_start(va, s);
    vsprintf(buf, s, va);
    va_end(va);
    char bufname[64]={0};
    if(p->l->tk_now.type == TK_IDENT){
        memcpy(bufname, &p->l->code[p->l->tk_now.start],p->l->tk_now.length);
    }
    printf("Parser: Line:%d\nNear:%s\n",p->l->line,lex_get_line(p->l));
    printf("Error: %s('%s')\n",buf,bufname);
    // exit(1);
    glo_sym_debug(&p->m->sym_table);
    longjmp(err_callback, 1);
}


var_t* var_exist(parser_t*p){
    var_t *v = var_exist_glo(p);
    if(!v){
        v = VAR_EXIST_LOC;
    }
    return v;
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

int arg_decl(parser_t *p,int offset,hashmap_t *dst,char *btin,bool is_decl){
    int ptr_depth;
    char builtin;
    proto_t *type;
    
    int sz = def_stmt(p, &ptr_depth, &builtin, &type,NULL);
    *btin = builtin;
    if(!is_decl){
        return sz;
    }
    var_t *r = var_new_base(builtin, offset+sz, ptr_depth, 0, type);
    token_t name = p->l->tk_now;
    if(!hashmap_get(dst, &p->l->code[name.start], name.length)){
        hashmap_put(dst, &p->l->code[name.start], name.length, r);
        
    }
    if(!hashmap_get(&p->m->local_sym_table, &p->l->code[name.start], name.length)){
        hashmap_put(&p->m->local_sym_table, &p->l->code[name.start], name.length, r);
    }
    
    return sz;
}

int proto_decl(parser_t *p,int offset,hashmap_t *dst){
    int ptr_depth;
    char builtin;
    proto_t *type;
    int sz = def_stmt(p, &ptr_depth, &builtin, &type,NULL);
    token_t name = p->l->tk_now;
    if(dst){
        proto_sub_t *prot = subproto_new(offset,builtin,type,ptr_depth);    
        hashmap_put(dst, &p->l->code[name.start], name.length, prot);
    }
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

bool var_is_extern(var_t *v){
    if(v == 0)
        return 0;
    if(v->base_addr == 0 && v->isglo)
        return 1;
    if(v->type == TP_FUNC){
        return ((function_frame_t*)v->base_addr)->ptr == 0;
    }
    return 0;
}

int solve_func_sig(parser_t *p,function_frame_t *func,bool is_restore){
    int offset = 0;
    int no  = 1;
    int sz = 0;
    char builtin;
    while (!lexer_skip(p->l, ')')) {
        lexer_next(p->l);
        
        sz = arg_decl(p, offset, &func->arg_table,&builtin,is_restore);
        offset+=sz;
        if(is_restore)
            restore_arg(p, no, offset, sz==8?TP_U64:builtin);
        no++;
        if(lexer_skip(p->l, ')')){
            break;
        }
        lexer_expect(p->l, ',');
    }
    return sz;
}

var_t* var_def(parser_t*p, bool is_extern){
    int ptr_depth;
    char builtin;
    proto_t *prot;
    function_frame_t *func = NULL;
    u64 arr = 0;
    int sz = def_stmt(p, &ptr_depth, &builtin, &prot,NULL);
    var_t* nv = var_exist(p);
    if(nv && !var_is_extern(nv)){
        if(is_extern){
            // ignore this line...
            lexer_skip_till(p->l, ';');
            return nv;
        }
        trigger_parser_err(p, "variable is already existed!");
    }

    token_t name = p->l->tk_now;
    if(lexer_skip(p->l, '(')){
        if(!p->isglo){
            trigger_parser_err(p, "Cannot declare a function inside a function");
        }
        if(is_extern && nv){
            if(nv->type != TP_FUNC){
                trigger_parser_err(p, "Extern prototype conflict!");
            }
            lexer_match(p->l, '(', ')');
            if(lexer_skip(p->l, '{')){
                trigger_parser_err(p, "Extern function cannot have a function body");
            }
            return nv;
        }
            
            if(!nv){  
                nv = var_new_base(TP_FUNC, 0, 1,1,prot);
                hashmap_put(&p->m->sym_table, &p->l->code[name.start], name.length, nv);
            }
            //printf("Extern function!\n");
        // function declare:
            lexer_next(p->l);
            int offset= 0;
            
            p->isglo = FALSE;
            
            
            if(!nv->base_addr){
                func = function_new(0);
                nv->base_addr = (u64)func;
            }else{
                func = (function_frame_t*)nv->base_addr;
            }
            u64* jmp = 0;
            u64* preserv = 0;
            if(!is_extern){
                jmp = emit_jmp_flg(p->m);
                func->ptr = (u64)jit_top(p->m);
                emit(p->m, 0x55);//push rbp
                emit_mov_r2r(p->m, REG_BP, REG_SP); // mov rbp,rsp
                preserv = emit_offsetrsp(p->m, 0,1);
            }
            offset = solve_func_sig(p, func,is_extern == 0);
            lexer_next(p->l);
            func->ret_type.builtin = builtin;
            func->ret_type.type = prot;
            func->ret_type.ptr_depth = ptr_depth;
            if(is_extern){
                p->isglo = TRUE;
                return nv;
            }
            
            
            if(!lexer_skip(p->l,'{')){    
                trigger_parser_err(p,"Expect function body");
                
            }else {
                lexer_next(p->l);
                p->m->stack=offset;
                stmt_loop(p);
                stack_debug(&p->m->local_sym_table);
                module_clean_stack_sym(p->m);
                lexer_next(p->l);
                p->m->stack = BYTE_ALIGN(p->m->stack, 16);
                printf("Total Allocate %d bytes on stack\n",p->m->stack);
                if(p->m->stack < 128){
                    *(u8*)(preserv)=p->m->stack;
                }else {
                    *(u32*)(preserv)=p->m->stack;
                }
                
                *jmp = (u64)jit_top(p->m) - (u64)p->m->jit_compiled;
                module_add_reloc(p->m, (u64)jmp - (u64)p->m->jit_compiled);
            }
        
        
        p->isglo = TRUE;
        return nv;
    }else if(lexer_skip(p->l, '[')){
        // u8 a[8];
        lexer_next(p->l);
        token_t num = lexer_next(p->l);
        if(num.type != TK_INT){
            trigger_parser_err(p, "Array init must be a integer token!");
        }
        arr = num.integer;
        lexer_expect(p->l, ']');
        
    }
    if(!nv){
        nv = var_new_base(func?TP_FUNC:builtin, 0, func?1:ptr_depth,p->isglo,prot);
        nv->base_addr = 0;
        if(nv->isglo){
            hashmap_put(&p->m->sym_table, &p->l->code[name.start], name.length, nv);
        }else {
            hashmap_put(&p->m->local_sym_table, &p->l->code[name.start], name.length, nv);
        }
        if(is_extern){
            printf("Extern variable\n");
            return nv;
        }
    }
    if(p->isglo){
        printf("variable alloc %d bytes on heap!\n",arr?arr*sz:sz);
        nv->base_addr = func?(u64)func:reserv_data(p->m, arr?arr*sz:sz);
        if(arr){
            u64* ptr = (u64*)reserv_data(p->m, 8);
            *ptr = nv->base_addr;
            nv->base_addr = (u64)ptr;
        } 
    }else {
        printf("variable alloc %d bytes on stack!\n",arr?arr*sz:sz);
        // notice here:

        p->m->stack+=(arr?arr*sz:sz);
        nv->base_addr = p->m->stack;
        if(arr){
            u32 offset = p->m->stack;
            p->m->stack+=8;
            nv->base_addr = p->m->stack;
            emit_storelocaddr(p->m, nv->base_addr, offset);
        }            
        if(prot && ptr_depth == 0){
            if(arr){
                printf("WARN: impl for static array will not be applied!");

            }else{
                // emit_mov_r2r(p->m, REG_AX, REG_BP);
                // emit_load(p->m, REG_BX, nv->base_addr);
                // emit_minusr2r(p->m, REG_AX, REG_BX);
                // proto_impl(p->m, prot);
            }
        }
    }
    if(arr){
        nv->ptr_depth++;
    }
    return nv;

}

int def_or_assign(parser_t *p){
    var_t *nv= var_def(p,0);
    if(nv->type == TP_FUNC)
        return 1;
    if(lexer_skip(p->l, '=')){

        //assignment
        lexer_next(p->l);lexer_next(p->l);
        prep_assign(p, nv);
        assignment(p, nv);
    }
}

int expression(parser_t*p){
    Tk tt = p->l->tk_now.type;
    if(tt == TK_IDENT){
        token_t tk = p->l->tk_now;
        if(hashmap_get(&p->m->prototypes,&p->l->code[tk.start] ,tk.length)){
            return def_or_assign(p);
        }
        // just assignment?
        var_t inf;
        expr_base(p, &inf,FALSE);        
        // print the variable
        //
        // if(inf.type!=TP_CUSTOM && inf.isglo){
        //     emit_mov_addr2r(p->m, REG_AX, REG_AX);
        // }
        if(hashmap_get(&p->m->sym_table, "_debug_", 7)){
            u64 addr = debuglibs[DBG_PRINT_INT].addr;
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
        return def_or_assign(p);
    }
    return 0;
    
}

static char pwd_buf[100]={0};

void stmt(parser_t *p){
    Tk tt= p->l->tk_now.type;
    if(tt == TK_STRUCT){
        token_t tk = lexer_next(p->l);
        if(tk.type != TK_IDENT){
            trigger_parser_err(p, "Struct needs an identity name!");
        }
        lexer_expect(p->l, '{');
        bool is_exist = 0;
        proto_t *new_type=0;
        if(!hashmap_get(&p->m->prototypes, &p->l->code[tk.start], tk.length))
            new_type= proto_new(0);
        int offset=0;
        while (!lexer_skip(p->l, '}')) {
            lexer_next(p->l);
            offset+=proto_decl(p, offset, new_type?&new_type->subs:0);
            lexer_expect(p->l, ';');
        }
        lexer_next(p->l);
        
        if(new_type){
            new_type->len = offset;
            hashmap_put(&p->m->prototypes, &p->l->code[tk.start], tk.length, new_type);
            proto_debug(new_type);
        }
        
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
            
            u64 *els_end = emit_jmp_flg(p->m);
            *els = (u64)jit_top(p->m);
            if(lexer_skip(p->l, TK_IF)){
                // special case: else if{}
                lexer_next(p->l);
                stmt(p);
            }
            else{
                lexer_expect(p->l, '{');
                stmt_loop(p);
                lexer_next(p->l);
            }
            *els_end = (u64)jit_top(p->m);
        }
        return;
    }
    else if(tt == TK_BREAK){
        if(p->loop_reloc == 0){
            trigger_parser_err(p, "Break should use inside a loop!");
        }
        u64 *addr = emit_jmp_flg(p->m);
        *addr = p->loop_reloc;
        module_add_reloc(p->m, (u64)addr - (u64)p->m->jit_compiled);
    }
    else if(tt == TK_WHILE){
        lexer_expect(p->l, '(');
        lexer_next(p->l);
        u64 top_adt = (u64)jit_top(p->m);
        var_t inf;
        expr_root(p, &inf);
        lexer_expect(p->l, ')');
        lexer_expect(p->l, '{');
        if(inf.type == TP_CUSTOM && inf.ptr_depth == 0)
            trigger_parser_err(p, "Cannot compare!");
        emit(p->m, 0x3c);emit(p->m,0x00); // cmp al,0
        emit(p->m, 0x75);emit(p->m,0x0c);  // je +0x0c +
        u64 break_adr = (u64)jit_top(p->m) - (u64)p->m->jit_compiled;
        u64* els = emit_jmp_flg(p->m);  // jmpq rax |
        u64 old_break = p->loop_reloc;
        p->loop_reloc = break_adr;
        stmt_loop(p);
        p->loop_reloc = old_break;
        emit_load(p->m, REG_AX, top_adt);
        emit(p->m, 0xff);emit(p->m, 0xe0);//jmp rax
        lexer_next(p->l);
        *els = (u64)jit_top(p->m);
        return;
    }else if(tt == TK_IMPORT){
        token_t name = lexer_next(p->l);
        if(name.type != '\"')
            trigger_parser_err(p, "Expect a string!");
        int i = p->l->cursor;
        int start = i;
        int len = 0, pwd = i;
        int last_name = i;
        while (p->l->code[i]!='\"') {
            i++;len++;
            if(p->l->code[i] == '/'){
                pwd = i;
            }
        }
        p->l->cursor=i;
        lexer_next(p->l);
        module_t *mod = 0;
        char path[100]={0};
        char old_pwd[100]={0};
        strcpy(old_pwd, pwd_buf);
        strcat(path, pwd_buf);
        strncat(path, &p->l->code[start],len);
        if(pwd)
            memcpy(pwd_buf, &p->l->code[start], pwd-start+1);
        printf("NOW import work on %s\n",pwd_buf);
        mod = module_compile(path, &p->l->code[last_name], len-(last_name-start), 1,p->m);
        if(!mod){
            trigger_parser_err(p, "Fail to import:%s\n",path);
        }
        printf("Load one new module:%s\n",path);
        memset(pwd_buf, 0, 100);
        strcpy(pwd_buf,old_pwd);
        if(!mod){
            trigger_parser_err(p, "fail to load module!");
        }
        // var_t * lib = var_new_base(TP_LIB, (u64)&mod->sym_table, 0, 1, 0);
        // hashmap_put(&p->m->sym_table, &p->l->code[last_name], len-(last_name-start),lib );

        return;
    }else if(tt == TK_EXTERN){
        lexer_next(p->l);
        var_def(p,1);
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
    p.loop_reloc = 0;
    p.isglo = TRUE;
    while (!lexer_skip(lxr, TK_EOF)) {
        lexer_next(lxr);
        stmt(&p);
    }
    
    // int (*test)() = m->jit_compiled;
    
    // printf("JIT(%d bytes) CALLED:%d\n",m->jit_cur, test());
}


module_t* module_compile(char *path,char *module_name, int name_len,bool is_module,module_t *previous){
    Lexer_t lex;

    module_t *mod = previous?previous:calloc(1, sizeof(module_t));
    FILE *fp = fopen(path, "r");
    if(!fp){
        printf("Fail to open%s",path);
        return 0;
    }
    int pwd = 0,i = 0;
    while (path[i]) {
        if(path[i] == '/'){
            pwd = i;
        }
        i++;
    }
    if(pwd){
        memcpy(pwd_buf, path, pwd+1);
    }
    fseek(fp, 0, SEEK_END);
    u32 sz = ftell(fp)+1;
    fseek(fp, 0, SEEK_SET);
    char *buf = calloc(1, sz);
    fread(buf, sz-1, 1, fp);
    fclose(fp);
    lexer_init(&lex, path, buf);
    jmp_buf old;
    old[0]=err_callback[0];
    int ret = setjmp(err_callback);
    if(!previous){
        module_init(mod, TKSTR2VMSTR(module_name, name_len));
        //qc_lib_console(mod);
        // module_add_var(module_t *m, var_t *v, vm_string_t name)
    }
    if(!ret){
        if(!previous){
            emit(mod, 0x55);
            emit_mov_r2r(mod, REG_BP, REG_SP);
        }
        
        parser_start(mod, &lex);
        free(buf);
        if(!previous){
            emit(mod, 0xc9); // leave
            emit(mod, 0xc3); // ret
        }
        err_callback[0]=old[0];
        return mod;
    }else {
        err_callback[0]=old[0];
        free(buf);
        return 0;
    }
}

