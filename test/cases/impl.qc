#include "../../libqc/def.qh"
#include "../../libqc/io.qh"
#define _IO_H

i32 assert_eq(i32 a, i32 b)
{
    printf("a:%d,b:%d\n",a,b);
    if (a != b) {
        printf("ASSERT!\n");
        
    }else{

        printf("[PASS]\n");
    }
    return 0;
}


typedef struct{
    u64 c;
    u64 d;
}sub_t;

typedef struct{
    u64 a;
    u64 b;
    sub_t sub;
    sub_t *sub2;
}test_t;

typedef struct{
    u64 a;
    u64 b;
}test_init_t;
// i8 a,b,c;
// impl(test_t) extern i8 set(test_t *this);
// impl(test_t) extern i8 get(test_t *this);

impl(test_t) i8 set(test_t *this){
    this->a = 1;
    this->b = 2;
    
    return 1;
}

impl(sub_t) i8 set(sub_t *this,u64 c,u64 d){
    printf("%llx %d %d\n",this,c,d);
    this->c = c;
    this->d = d;
    return 1;
}

impl(sub_t) i8 get(sub_t *this){
    printf("Sub: %d %d\n",this->c,this->d);
    return this->c+this->d;
}

impl(test_t) i8 get(test_t *this){
    printf("%d %d\n",this->a,this->b);
    return 1;
}

impl(test_init_t) i8 __init__(test_init_t *this){
    this->a = 1;
    this->b = 2;
}

i8 f2(u64 arg1,u64 arg2){
    printf("%d %d\n",arg1,arg2);
    return 1;
}

i8 f3(u64 arg1){
    return arg1;
}

i8 main(){
    test_t t1;
    sub_t s2;
    s2.c = 9;
    s2.d = 10;
    t1.sub2 = &s2;
    t1.set();
    t1.get();
    t1.sub.set(1,4);
    t1.sub.get();
    t1.sub2->get();
    f2(100, t1.sub2->get());
    t1.sub.set(f3(100),200);
    t1.sub.get();
    assert_eq(__test_t_get$impl, t1.get);
    printf("impl_test: %x, impl_sub: %x\n",t1.set,t1.sub.set);
    test_init_t t3 = ();
    printf("t3: %d %d\n",t3.a,t3.b);
    return 1;
}