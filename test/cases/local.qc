#include "../../libqc/io.qh"

i32 main()
{
    i32 n = 10;
    printf("%d\n", n);
    u8 cc = 'a';
    u8 *ptr = &cc;
    printf("%c\n", *ptr);
    i32 *c = &n;
    printf("%d\n", *c);
    i32 r;
    r = 0;
    return r;
}