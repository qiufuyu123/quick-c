#ifndef _H_LEX
#define _H_LEX

#include "define.h"

typedef u16 Tk;

enum multi_tks {
	TK_CONCAT = 256,
	TK_ADD_ASSIGN, TK_SUB_ASSIGN, TK_MUL_ASSIGN, TK_DIV_ASSIGN, TK_MOD_ASSIGN,
	TK_EQ, TK_NEQ, TK_LE, TK_GE,
	TK_AND, TK_OR,
	TK_IF, TK_ELSE, TK_ELSEIF, TK_LOOP, TK_WHILE, TK_FOR, TK_FN, TK_STRUCT,
	TK_IDENT, TK_FLOAT,TK_INT,TK_I8,TK_U8,TK_I16,TK_U16,TK_I32,TK_U32,TK_I64,TK_U64, TK_FALSE, TK_TRUE, TK_NIL
    ,TK_RETURN,
	TK_EOF,
};

typedef struct{
    Tk type;    
    int start;
    int length;
    int line;
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

    token_t tk_now;

}Lexer_t;

char* lex_get_line(Lexer_t *lxr);

u64 hash_string(char *string, int length);

void lexer_init(Lexer_t *lex,char *path, char *code);

token_t lexer_next(Lexer_t *lex);

token_t lexer_expect(Lexer_t *lex,Tk type);

bool lexer_skip(Lexer_t *lex,Tk type);

void lexer_debug(char *content);

#endif