u8 _debug_ = 1;


u64 a = 0xffff8888ffff8888;
a;
u8 *addr = &a;
addr;
u8 a2;
a2 = *addr;
a2;
a= a - (a-a+1);
a;
a2=a;
a2;

// expect:
// 136
// 135

u64 c = 0x123456;
c;
addr = &c;
u64 **addr2 = &addr;
addr2;
u64 r = **addr2;
r;

// [debug lib]: ffff8888ffff8888
// [debug lib]: 562d30154081
// [debug lib]: 136
// [debug lib]: 136
// [debug lib]: ffff8888ffff8887
// [debug lib]: ffff8888ffff8887
// [debug lib]: 135
// [debug lib]: 135
// [debug lib]: 123456
// [debug lib]: 562d30154092
// [debug lib]: 562d30154089
// [debug lib]: 123456