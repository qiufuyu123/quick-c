#include "lex.h"
#include "define.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

// Magic prime number for FNV hashing.
#define FNV_64_PRIME ((u64) 0x100000001b3ULL)
u64 hash_string(char *string, int length) {
	// Convert to an unsigned string
	unsigned char *str = (unsigned char *) string;

	// Hash each byte of the string
	u64 hash = 0;
	for (int i = 0; i < length; i++) {
		// Multiply by the magic prime, modulo 2^64 from integer overflow
		hash *= FNV_64_PRIME;

		// XOR the lowest byte of the hash with the current octet
		hash ^= (u64) *str++;
	}

	return hash;
}
static inline int is_whitespace(char ch) {
	return ch == '\r' || ch == '\n' || ch == '\t' || ch == ' ';
}

static inline int is_decimal_digit(char ch) {
	return ch >= '0' && ch <= '9';
}

static inline int is_ident_start(char ch) {
	return (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || ch == '_';
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
			lxr->line++;
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
    printf("Lexer: %s! At line(%d)\nNear: %s.\n",buf,lxr->line,l);
    exit(1);
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
    if(errno != 0){
        trigger_lex_error(lxr, "Cannot parse integer const");
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
		 "if", "else", "elseif", "loop", "while", "for", "fn", "struct","true",
		"false", "nil", "i8","u8","i16","u16","i32","u32",
        "i64","u64","return",NULL,
	};
	static Tk keyword_tks[] = {
		 TK_IF, TK_ELSE, TK_ELSEIF, TK_LOOP, TK_WHILE, TK_FOR, TK_FN,TK_STRUCT,
		TK_TRUE, TK_FALSE, TK_NIL,TK_I8,TK_U8,TK_I16,TK_U16,TK_I32,TK_U32,TK_I64
        ,TK_U64,TK_RETURN
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

	// Didn't find a matching keyword, so we have an identifier
	lxr->tk_now.type = TK_IDENT;
	lxr->tk_now.id = hash_string(ident, lxr->tk_now.length);
    return lxr->tk_now;
}


void lexer_init(Lexer_t *lex,char *path, char *code){
    lex->path = path;
    lex->code = code;
    lex->cursor = 0;
    lex->line = 1;
    lex->tk_now.type = 0;
}

#define MULTI_CHAR_TK(a,b,t)\
    case a:\
        if (lex->code[lex->cursor + 1] == b) { \
			lex->tk_now.type = t;                 \
			lex->tk_now.length = 2;                     \
            lex->cursor+=2;          \
			break;                                  \
		}


token_t lexer_next(Lexer_t *lex){
    lex->tk_now.line = lex->line;
    lex->tk_now.length = 0;
    lex->tk_now.start = lex->cursor;
    lex->tk_now.id = 0;

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
    if(c=='/' && lex->code[lex->cursor+1]=='/'){
        while (lex->code[lex->cursor]) {
            if(lex->code[lex->cursor] == '\n'){
                lex->cursor++;
                break;
            }
            lex->cursor++;
        }
        return lexer_next(lex);
    }
    // symbols
    switch (c) {
        MULTI_CHAR_TK('+', '=', TK_ADD_ASSIGN)
        MULTI_CHAR_TK('-', '=', TK_SUB_ASSIGN)
        MULTI_CHAR_TK('*', '=', TK_MUL_ASSIGN)
        MULTI_CHAR_TK('/', '=', TK_DIV_ASSIGN)
        MULTI_CHAR_TK('%', '=', TK_MOD_ASSIGN)

        MULTI_CHAR_TK('<', '=', TK_LE)
        MULTI_CHAR_TK('>', '=', TK_GE)
        MULTI_CHAR_TK('=', '=', TK_EQ)
        MULTI_CHAR_TK('!', '=', TK_NEQ)

        MULTI_CHAR_TK('&', '&', TK_AND)
        MULTI_CHAR_TK('|', '|', TK_OR)
        default:
            lex->tk_now.type = lex->code[lex->cursor];
		    lex->tk_now.length = 1;
            lex->cursor++;
		    break;
    }

    return lex->tk_now;
}

token_t lexer_expect(Lexer_t *lex,Tk type){
    if(lexer_next(lex).type != type){
        trigger_lex_error(lex, "Unexpected token,need:%c",type);
    }
    return lex->tk_now;
}

bool lexer_skip(Lexer_t *lex,Tk type){
    int old_col = lex->line;
    token_t tk_old = lex->tk_now;
    int old = lex->cursor;
    bool r = lexer_next(lex).type == type;
    lex->cursor = old;
    lex->line = old_col;
    lex->tk_now = tk_old;
    return r;
}

void lexer_debug(char *content){
    Lexer_t lex;
    token_t tk;
    static char* translate[] = {"+=","-=","*=","/=","%=","==",
    "!=","<=",">=","&&","||","if","else","elseif","loop","while",
    "for","fn","struct","ID","FLOAT","INT","i8","u8","i16","u16","i32","u32",
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