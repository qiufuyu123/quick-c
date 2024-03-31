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
