u8 _debug_ = 1;
import "libqc.io"


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
    inside.bbb = 1;
    inside.ccc = 2;
    inside.ddd = 3;
    inside.e = &e;
    io.printu8(inside.bbb);
    io.printu8(inside.ccc);
    io.printu8(inside.ddd);
    inside.e;
    return inside.e;
}
test2();
// [debug lib]: 999
// [debug lib]: 888
// [debug lib]: 5625ce05f891
// [debug lib]: 5625ce05f899
// [debug lib]: 5625ce05f899
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
// [debug lib]: 127
// [debug lib]: 11
// [debug lib]: 33
// [debug lib]: 5625ce05f899
// [debug lib]: 127
// [debug lib]: 11
// [debug lib]: 33
// [debug lib]: 5625ce05f899
// [debug lib]: 5625ce05f899
// [debug lib]: 0