u8 _debug_ = 1;


struct sub{
    u32 m;
    u32 n;
};

struct aaa{
    u8 bbb;
    u8 ccc;
    u8 ddd;
    i16 *e;
    sub test_sub;
    sub *test_sub_ptr;
};
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
    inside.bbb = 127;
    inside.ccc = 11;
    inside.ddd = 33;
    inside.bbb;
    return inside.bbb;
}
console.print(test2(),2);
//expect:
// 255
// 128
// 123
// 2333
// 3222
// 999
// 889
// 127
// 11
// 33
// 127
// 127