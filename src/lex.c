#include "lex.h"
#include "define.h"
#include "hashmap.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
token_t line_macro;

static inline int is_whitespace(char ch) {
	return ch == '\r' || ch == '\n' || ch == '\t' || ch == ' ';
}

static inline int is_decimal_digit(char ch) {
	return ch >= '0' && ch <= '9';
}

static inline int is_ident_start(char ch) {
	return (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || ch == '_' || ch == '#' || ch == '$';
}

static inline int is_ident_continue(char ch) {
	return is_ident_start(ch) || is_decimal_digit(ch);
}

static void lex_whitespace(Lexer_t *lxr) {
	while (is_whitespace(lxr->code[lxr->cursor])) {
		char ch = lxr->code[lxr->cursor];
		if (ch == '\n' || ch == '\r') {
			// Treat `\r\n` as a single newline
			if (ch == '\r' && lxr->code[lxr->cursor + 1] == '\n') {
				lxr->cursor++;
			}
            lxr->col = lxr->cursor;
			lxr->line++;
            // hashmap_put(&const_table, "_LINE_", 6, (void*)lxr->line);
		}
		lxr->cursor++;
	}
}

char* lex_get_line(Lexer_t *lxr){
    int i = 0;
    int j = 0;
    while (lxr->code[lxr->cursor-i] != '\n' && (lxr->cursor - i > 0))
        i++; 
    while (lxr->code[lxr->cursor+j] != '\n' && lxr->code[lxr->cursor+j] != '\0')
        j++;
    char *info = malloc(i+j+1);
    memset(info, 0, i+j+1);
    memcpy(info, &lxr->code[lxr->cursor-i], i+j);
    return info;
}

static void trigger_lex_error(Lexer_t *lxr, const char * msg,...){
    char buf[100]={0};
    va_list va;
    va_start(va, msg);
    vsprintf(buf, msg, va);
    va_end(va);
    char *l = lex_get_line(lxr);
    int col = lxr->cursor - lxr->col+1;
    printf("Lexer: %s! At line(%d) col(%d)\nNear: %s.\n\033[31m",buf,lxr->line,col,l);
    //printf("~~~~~");
    for (int i = 0; i<col; i++) {
        printf("~");
    }
    printf("^\033[0m\n");
    exit(1);
}

void lex_def_const(Lexer_t *lxr, token_t name, token_t val){
    // if(val.type != TK_INT){
    //     trigger_lex_error(lxr, "#define/const expect a number const!");
    // }
    // printf("call lex_def_const!\n");
    token_t *old=hashmap_get(lxr->const_table, &lxr->code[name.start], name.length);
    if(old){
        *old = val;
        return;
    }
    token_t *copy = calloc(1, sizeof(token_t));
    *copy = val;
    hashmap_put(lxr->const_table, &lxr->code[name.start], name.length, (void*)copy);
}

char lex_ifdef_const(Lexer_t *lxr, token_t name){
    void *x = 0;
    return hashmap_exist(lxr->const_table, &lxr->code[name.start], name.length, &x);
}

static void lex_float(Lexer_t *lxr){
    char *start = &lxr->code[lxr->cursor];
    char *end;
    float val = strtof(start, &end);
    if(errno != 0){
        trigger_lex_error(lxr, "Cannot parse float const");
    }
    lxr->tk_now.type = TK_FLOAT;
    lxr->tk_now.length = end - start;
    lxr->tk_now.fl = val;
    lxr->cursor+=lxr->tk_now.length;
}

static void lex_int(Lexer_t *lxr,int base){
    if(base == 16)
        lxr->cursor+=2;
    char *start = &lxr->code[lxr->cursor];
    char *end;
    u64 val = strtoull(start, &end,base);
    errno = 0;
    if(errno != 0){
        trigger_lex_error(lxr, "Cannot parse integer const %s",strerror(errno));
    }
    lxr->tk_now.type = TK_INT;
    lxr->tk_now.length = end - start;
    lxr->tk_now.integer = val;
    lxr->cursor+=lxr->tk_now.length;
}

static token_t lex_num(Lexer_t *lxr){
    int i = lxr->cursor;
    if(lxr->code[i] == '0' && lxr->code[i+1] == 'x'){
        lex_int(lxr, 16);
        return lxr->tk_now;
    }
    while(is_decimal_digit(lxr->code[i])){
        i++;
    }
    if(lxr->code[i] == '.'){
        lex_float(lxr);
    }else {
        lex_int(lxr, 10);
    }
    return lxr->tk_now;
}

static token_t lex_ident(Lexer_t *lxr) {
	// Find the end of the identifier
	char *ident = &lxr->code[lxr->cursor];
	while (is_ident_continue(lxr->code[lxr->cursor])) {
		lxr->cursor++;
	}
	lxr->tk_now.length = lxr->cursor - lxr->tk_now.start;
	// A list of reserved keywords and their corresponding token values
	static char *keywords[] = {
		 "if", "else", "#ifdef", "#endif", "while", "for", "typedef", "struct","#include","#pragma","#define","extern","break","continue","true",
		"false", "#ifndef", "i8","u8","i16","u16","i32","u32",
        "i64","u64","return","sizeof","enum","__jit__","offsetof","impl",NULL,
	};
	static Tk keyword_tks[] = {
		 TK_IF, TK_ELSE, TK_IFDEF, TK_ENDIF, TK_WHILE, TK_FOR, TK_TYPEDEF,TK_STRUCT,TK_IMPORT,TK_PRAGMA,TK_DEFINE,TK_EXTERN,TK_BREAK,TK_CONTINUE,
		TK_TRUE, TK_FALSE, TK_IFNDEF,TK_I8,TK_U8,TK_I16,TK_U16,TK_I32,TK_U32,TK_I64
        ,TK_U64,TK_RETURN,TK_SIZEOF,TK_ENUM,TK_JIT,TK_OFFSETOF,TK_IMPL
	};

	// Compare the identifier against reserved language keywords
	for (int i = 0; keywords[i] != NULL; i++) {
		if (lxr->tk_now.length == strlen(keywords[i]) &&
				strncmp(ident, keywords[i], lxr->tk_now.length) == 0) {
			// Found a matching keyword
			lxr->tk_now.type = keyword_tks[i];
			return lxr->tk_now;
		}
	}
    
    token_t *macro_data;
    if(lxr->need_macro && hashmap_exist(lxr->const_table, &lxr->code[lxr->tk_now.start], lxr->tk_now.length, (void*)&macro_data)){
       lxr->tk_now.type = macro_data->type;
       lxr->tk_now.integer = macro_data->integer;
    }else {
        // Didn't find a matching keyword, so we have an identifier
        lxr->tk_now.type = TK_IDENT;
        lxr->tk_now.id = 0;
    }

	
    return lxr->tk_now;
}


void lexer_init(Lexer_t *lex,char *path, char *code){
    lex->path = path;
    lex->code = code;
    lex->cursor = 0;
    lex->line = 1;
    lex->col = 0;
    lex->tk_now.type = 0;
    lex->need_macro = 1;
    
    // token_t *now_line = calloc(1, sizeof(token_t));
    // line_macro.type = TK_INT;
    // line_macro.integer = lex->line;
    // hashmap_put(&const_table, "_LINE_", 6, (void*)now_line);

}

void lexer_free(Lexer_t *lex){
    //hashmap_destroy(&const_table);
    //free(lex->code);
}


void macro_free(){
    // hashmap_destroy(&const_table);
}
#define MUTLI_SUB_TK(b,t)\
        if (lex->code[lex->cursor + 1] == b) { \
			lex->tk_now.type = t;                 \
			lex->tk_now.length = 2;                     \
            lex->cursor+=2;          \
			break;                                  \
		}


#define MULTI_CHAR_TK(a,b,t)\
    case a:\
        MUTLI_SUB_TK(b,t)

void lexer_match(Lexer_t *lex, Tk left, Tk right){
    int depth = 0;
    while (1) {
        Tk tt = lexer_next(lex).type;
        if(tt == left){
            depth++;
        }else if(tt == right){
            depth--;
            if(depth == 0){
                break;
            }
        }else if(tt == TK_EOF){
            trigger_lex_error(lex, "Fail to make a loop match:%c - %c\n",left,right);
        }
    }
}

int binary_op[18][3] ={
    {'+','+',TK_ADD2},
    {'+','=',TK_ADD_ASSIGN},
    {'-','-',TK_MINUS2},
    {'-','=',TK_SUB_ASSIGN},
    {'*','=',TK_MUL_ASSIGN},
    {'/','=',TK_DIV_ASSIGN},
    {'%','=',TK_MOD_ASSIGN},
    {'|','=',TK_OR_ASSIGN},
    {'&','=',TK_AND_ASSIGN},
    {'<','=',TK_LE},
    {'>','=',TK_GE},
    {'=','=',TK_EQ},
    {'!','=',TK_NEQ},
    {'&','&',TK_LOGIC_AND},
    {'|','|',TK_LOGIC_OR},
    {'<','<',TK_LSHL},
    {'>','>',TK_LSHR},
    {'-','>','.'}
};

token_t lexer_next(Lexer_t *lex){
    lex->tk_now.col = lex->cursor - lex->col + 1;
    lex->tk_now.line = lex->line;
    lex->tk_now.length = 0;
    lex->tk_now.start = lex->cursor;
    lex->tk_now.id = 0;
    lex->tk_now.type = 0;
    char c = lex->code[lex->cursor];
    if(c == '\0'){
        lex->tk_now.type = TK_EOF;
        return lex->tk_now;
    }else if(c == '\n' || c == ' ' || c == '\t' || c == '\r'){
        lex_whitespace(lex);
        return lexer_next(lex);
    }
    else if(is_ident_start(c)){
        return lex_ident(lex);
        
    }else if(is_decimal_digit(c)){
        return lex_num(lex);
    }
    else if(c == '\''){
        lex->cursor++;
        char o = lex->code[lex->cursor];
        lex->cursor++;
        if(lex->code[lex->cursor] != '\''){
            trigger_lex_error(lex, "Expect ' after '");
        }
        lex->tk_now.type = TK_INT;
        lex->tk_now.integer = o;
        lex->cursor++;
        return  lex->tk_now;
    }
    if(c=='/' && lex->code[lex->cursor+1]=='/'){
        while (lex->code[lex->cursor]) {
            if(lex->code[lex->cursor] == '\n'){
                lex->cursor++;
                lex->line++;
                lex->col = lex->cursor;
                break;
            }
            lex->cursor++;
        }
        return lexer_next(lex);
    }
    if(c == '/' &&  lex->code[lex->cursor+1]=='*'){
        lex->cursor+=2;
        while (lex->code[lex->cursor]) {
            if(lex->code[lex->cursor] == '*' && lex->code[lex->cursor+1] == '/'){
                lex->cursor+=2;
                break;
            }else if(lex->code[lex->cursor] == '\n'){
                lex->line++;
                lex->col = lex->cursor;
                // notice colomn calc here
            }
            lex->cursor++;
        }
        return lexer_next(lex);
    }
    // symbols
    if(c == '.' && lex->code[lex->cursor+1] == '.' && lex->code[lex->cursor+2] == '.' ){
        lex->tk_now.length = 3;
        lex->tk_now.type = TK_VA_ARG;
        lex->cursor+=3;
        return lex->tk_now;
    }
    for (int i =0; i<18; i++) {
        if(c == binary_op[i][0] && lex->code[lex->cursor+1] == binary_op[i][1]){
            lex->tk_now.length=2;
            lex->tk_now.type = binary_op[i][2];
            lex->cursor+=2;
            break;
        }
    }
    if(lex->tk_now.type == 0){
        if(lex->code[lex->cursor] == '@'){
            lex->tk_now.type = TK_JIT;
        }else{
            lex->tk_now.type = lex->code[lex->cursor];
        }
        lex->tk_now.length = 1;
        lex->cursor++;
        
    }

    return lex->tk_now;
}

token_t lexer_expect(Lexer_t *lex,Tk type){
    if(lexer_next(lex).type != type){
        trigger_lex_error(lex, "Unexpected token,need:%c",type);
    }
    return lex->tk_now;
}

void lexer_now(Lexer_t *lex,Tk type){
    if(lex->tk_now.type != type){
        trigger_lex_error(lex, "Unexpected token,need:%c",type);
    }
}

token_t lexer_peek(Lexer_t *lex){
    int old_col = lex->line;
    token_t tk_old = lex->tk_now;
    int old = lex->cursor;
    token_t ntk = lexer_next(lex);
    
    lex->cursor = old;
    lex->line = old_col;
    lex->tk_now = tk_old;
    return ntk;
}

bool lexer_skip(Lexer_t *lex,Tk type){
    return lexer_peek(lex).type == type;
}

void lexer_skip_till(Lexer_t *lex, Tk stop){
    while (!lexer_skip(lex, stop)) {
        lexer_next(lex);
    }
}

void lexer_debug(char *content){
    Lexer_t lex;
    token_t tk;
    static char* translate[] = {"+=","-=","*=","/=","%=","==",
    "!=","<=",">=","&&","||","if","else","elseif","loop","while",
    "for","fn","struct","#include","impl","new","ID","FLOAT","INT","i8","u8","i16","u16","i32","u32",
        "i64","u64","FALSE","TRUE","RET","NIL"};
    lexer_init(&lex, "debug/test.q.c", content);
    while (1) {
        tk = lexer_next(&lex);
        if(tk.type == TK_EOF)
            return;
        printf("%d",tk.type);
        if(tk.type>256){
            
            if(tk.type == TK_FLOAT){
                printf("float %f\n",tk.fl);
            }else if(tk.type == TK_INT){
                printf("int %llu\n",tk.integer);
            }else if(tk.type == TK_IDENT){
                char buf[100]={0};
                memcpy(buf, lex.code+tk.start, tk.length);
                printf("id %s\n",buf);
            }else {
                printf("%s\n",translate[tk.type-257]);
            }
        }else {
            printf("%c\n",tk.type);
        }
    }
}