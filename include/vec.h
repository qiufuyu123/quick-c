#ifndef _H_VEC
#define _H_VEC

typedef struct{
    int capacity;
    int size;
    int unit;
    char* data;
}vec_t;

void vec_init(vec_t*v, int unit,int init);

void vec_release(vec_t*v );

void* vec_at(vec_t*v,int idx);

void vec_push(vec_t*v,void*data);

void* vec_push_n(vec_t*v,void *data,int n);

void* vec_top(vec_t*v);

void* vec_pop(vec_t*v);

void* vec_reserv(vec_t*v,int len);

#endif