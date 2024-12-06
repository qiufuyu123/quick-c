#include "../../libqc/def.qh"
#include "../../libqc/io.qh"
#define _IO_H

i32 assert_eq(i32 a, i32 b)
{
    printf("a:%d,b:%d\n",a,b);
    if (a != b) {
        printf("ASSERT!\n");
        
    }else{

        printf("[PASS]\n");
    }
    return 0;
}
u32 main()
{
    i32 a = 1;
    i32 i=0;
    for (i=0; i<10; i++) {
        a*=2;
    }
    assert_eq(a, 1024); // 1024
    for (i=0; i<10; i++) {
        a/=2;
    }
    assert_eq(a, 1); // 1024

    a = 0x01;
    a |= 0x02;
    assert_eq(a, 3);

    a = 0xff;
    a &= 0x08;
    assert_eq(a, 0x08);

    return 0;
}

//

// u64 a;
// a *= 2;