struct subsub{
    u8 a;
    u8 b;
    u8 c;
};

struct sub{
    u32 m;
    u32 n;
    subsub l;
};

struct aaa{
    u8 bbb;
    u8 ccc;
    u8 ddd;
    u64 eee;
    i16 *e;
    sub test_sub;
    sub *test_sub_ptr;
};
sub t;
t.m = 666;
t.n = 777;
i16 b = 12321;
aaa outside;
outside.test_sub_ptr = &t;
outside.e = &b;
outside.test_sub_ptr.m;
outside.test_sub_ptr.n;
u8 test(){
    aaa inside;
    sub sub_inside;
    sub_inside.l.a = 3;
    sub_inside.l.b = 4;
    sub_inside.l.c = 5;

    sub_inside.m = 555;
    sub_inside.n = 444;
    inside.bbb = 127;
    inside.ccc = 11;
    inside.ddd = 22;
    inside.e = &b;
    inside.eee = 0x2323232323232;
    inside.test_sub_ptr = &sub_inside;
    console.print(inside.test_sub_ptr.m,5);
    console.print(sub_inside.l.b,5);
    return 0;
}
test();

// expect:
// 555
// 4