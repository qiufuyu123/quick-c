/*
 * IDE Highlight provider for quick-c
 * This file will NOT be compiled by quick-c
 * due to `#progma ignore`!
 */
#pragma ignore

#ifndef _H_DEF
#define _H_DEF

typedef char i8;
typedef unsigned char u8;
typedef short i16;
typedef unsigned short u16;
typedef int i32;
typedef unsigned int u32;
typedef long long i64;
typedef long long u64;

// #ifndef BOOT_C
// #define char ERROR_ERROR
// #define int ERROR_ERROR
// #define unsigned ERROR_ERROR
// #endif
#define bool u8
#define TRUE 1 
#define FALSE 0
#define NULL 0

// Built-in keyword
#define __jit__(a,__VA_ARGS__) // macro used for embedded assembly
// #define invlpg       // invlpg [rax] for --^
//                      // e.g. __jit__(invlpg,&vaddr);
//                      // remember to use '&'
// #define sti          // __jit__(sti)
// #define cli           // __jit__(cli)
// #define bin

// Built-in expr, provided by quick-c compiler
#define offsetof(a,b) (u64)(&(((a*)0)->b))

#endif