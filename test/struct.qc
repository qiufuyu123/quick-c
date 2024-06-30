u8 _debug_ = 1;
extern u8 printnum(u64 v,u8 len);


typedef struct {
    u32 m;
    u32 n;
}sub;

typedef struct {
    u8 bbb;
    u8 ccc;
    u8 ddd;
    i16 *e;
    sub test_sub;
    sub *test_sub_ptr;
}aaa;
sub sub2;
sub2.m  = 999;
sub2.n = 888;
i16 e = 123;
aaa c;
c.test_sub_ptr = &sub2;
c.e = &e ;
c.e;
c.bbb = 255;
c.ccc = 128;
u8 r = c.bbb;
r;
u8 r2 = c.ccc;
r2;
u16 r3 = *c.e;
r3;
c.test_sub.m = 2333;
c.test_sub.n = 3222;
c.test_sub.m;
c.test_sub.n;
c.test_sub_ptr.m;
c.test_sub_ptr.n = c.test_sub_ptr.n+1;
c.test_sub_ptr.n;

u16 test2(){
    aaa inside;
    inside->bbb = 1;
    inside.ccc = 2;
    inside.ddd = 3;
    inside.e = &e;
    printnum(inside.bbb,0);
    printnum(inside.ccc,0);
    printnum(inside.ddd,0);
    printnum(*inside.e,0);
    printnum(e,0);
    return inside.e;
}
test2();
// [debug lib]: 999
// [debug lib]: 888
// [debug lib]: 557f810757a1
// [debug lib]: 557f810757a9
// [debug lib]: 557f810757a9
// [debug lib]: 255
// [debug lib]: 128
// [debug lib]: 255
// [debug lib]: 128
// [debug lib]: 123
// [debug lib]: 2333
// [debug lib]: 3222
// [debug lib]: 2333
// [debug lib]: 3222
// [debug lib]: 999
// [debug lib]: 889
// [debug lib]: 889
// [debug lib]: 1
// [debug lib]: 1
// [debug lib]: 1
// [debug lib]: 7f810757a9030201
// [debug lib]: 1
// [debug lib]: 10
// [debug lib]: 2
// [debug lib]: 10
// [debug lib]: 3
// [debug lib]: 10
// [debug lib]: 123
// [debug lib]: 10
// [debug lib]: 22441