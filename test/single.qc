#include "libqc.io"
struct aaa{
    u8 c;
    u8 d;
    u8 e;
    i16 *f;

};

u8 scope(){
    aaa t;
    t.c = 1;
    t.d=2;
    t.e=3;
    t.f = &t.d;

    io.printu8(t.c);
    io.printu8(t.d);
    io.printu8(t.e);
    io.printptr(t.f);
    io.printu8(*(t.f));
    return 0;
}
scope();
