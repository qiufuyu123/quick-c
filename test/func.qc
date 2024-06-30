#include "../libqc/io.qh"


u8 f(u8 e,u8 g,u8 h){
    e;
    g;
    h;
    
    u8 a = 123;
    u16 b = 32762;
    u32 c = 123123123;
    u64 d = 0x8888ffff8888ffff;
    a;
    return 123;
}

u16 f2(){
    u8 r = f();
    r;
    return 2024;
}
u16 glo_val = 666;
u16 *test(){
    return &glo_val;
}
u16 v = f(3,3,4);
printnum(v,2);
printnum(f2(),2);

//expect:
// 3
// 3
// 4
// 123
// 123
// 0
// 0
// 0
// 123
// 2147
// 666