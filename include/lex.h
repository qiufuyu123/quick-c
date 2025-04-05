#ifndef _H_LEX
#define _H_LEX

#include "define.h"
#include "hashmap.h"

typedef u16 Tk;

enum multi_tks {
	TK_CONCAT = 256,
    TK_VA_ARG,
	TK_ADD_ASSIGN, TK_SUB_ASSIGN, TK_MUL_ASSIGN, TK_DIV_ASSIGN, TK_MOD_ASSIGN,
    TK_OR_ASSIGN, TK_AND_ASSIGN,
	TK_EQ, TK_NEQ, TK_LE, TK_GE,
	TK_AND, TK_OR,TK_ADD2,TK_MINUS2,
    TK_LSHL,TK_LSHR,
	TK_IF, TK_ELSE, TK_IFDEF, TK_ENDIF, TK_WHILE, TK_FOR, TK_TYPEDEF, TK_STRUCT,TK_IMPORT,TK_PRAGMA,TK_DEFINE,TK_EXTERN,TK_ENUM,
	TK_IDENT,TK_BREAK,TK_CONTINUE, TK_FLOAT,TK_INT,TK_I8,TK_U8,TK_I16,TK_U16,TK_I32,TK_U32,TK_I64,TK_U64, TK_FALSE, TK_TRUE, TK_IFNDEF
    ,TK_RETURN,TK_SIZEOF,TK_JIT,TK_OFFSETOF,
	TK_EOF,
};

typedef struct{
    Tk type;    
    int start;
    int length;
    int line;
    int col;
    union{
        double db; // double
        float fl;  // float
        i64 integer; // integer
        u64 id; // identity's hash
    };
}token_t;

typedef struct{

    char* path;
    char* code;

    int cursor;
    int len;
    int line;
    int col;

    token_t tk_now;
    bool need_macro;

}Lexer_t;

void lex_def_const(Lexer_t *lxr, token_t name, token_t val);
char lex_ifdef_const(Lexer_t *lxr, token_t name);
char* lex_get_line(Lexer_t *lxr);

u64 hash_string(char *string, int length);

void lexer_init(Lexer_t *lex,char *path, char *code);

void lexer_free(Lexer_t *lex);

void macro_free();

token_t lexer_peek(Lexer_t *lex);

token_t lexer_next(Lexer_t *lex);

void lexer_now(Lexer_t *lex,Tk type);

token_t lexer_expect(Lexer_t *lex,Tk type);

bool lexer_skip(Lexer_t *lex,Tk type);

void lexer_match(Lexer_t *lex, Tk left, Tk right);

void lexer_debug(char *content);

void lexer_skip_till(Lexer_t *lex, Tk stop);
#endif