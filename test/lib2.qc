#include "libqc.io"

io.println("233");

struct test{
    u8 *f1;
    u8 *f2;
};

u8* f1(){
    io.println("f1call");
    return 0;
}

u8* f2(){
    io.println("f2call");
    return 0;
}

u8 init_test(test *v){
    v.f1 = f1;
    v.f2 = f2;
    return 0;
}

struct s2{
    test t1;
    test *t2;
};

u8 scope(){
    test t;
    init_test(&t);
    f1();
    t.f2();
    return 0;
}
scope();
test s1;
