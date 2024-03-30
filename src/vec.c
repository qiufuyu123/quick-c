#include "vec.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void vec_init(vec_t*v, int unit,int init)
{
    v->unit = unit;
    v->capacity = init;
    v->data = malloc(v->capacity*v->unit);
    memset(v->data, 0, v->capacity*v->unit);
    v->size=0;
}

void vec_release(vec_t*v ){
    free(v->data);
}

void* vec_at(vec_t*v,int idx){
    if(idx>=v->size)
        return NULL;
    return (void*)(&((char*)v->data)[idx*v->unit]);
}

void vec_push(vec_t*v,void*data){
    vec_push_n(v, data, 1);
}

void* vec_reserv(vec_t*v,int len){
    if(v->size+len-1 >= v->capacity){
        // enlarge
        void* m = realloc(v->data, (v->size+len-1)*2);
        if(m == NULL){
            printf("Vec Allocation OOM!\n");
            exit(1);
        }
        v->data = m;
    }
    void *r = &v->data[v->size*v->unit];
    v->size+=len;
    memset(r, 0, v->unit*len);
    return r;
}

void* vec_push_n(vec_t*v,void *data,int n){
    void *r = vec_reserv(v, n);
    memcpy(r, data, v->unit*n);
    return r;
}

void* vec_top(vec_t*v){
    return (void*)(&((char*)v->data)[(v->size-1)*v->unit]);
}

void* vec_pop(vec_t*v){
    void *r = vec_top(v);
    v->size--;
    return r;
}