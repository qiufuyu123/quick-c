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
    /* Value test */
    i32 a,b,c,d=5,e=10;

    assert_eq(d, 5);
    assert_eq(e, 10);

    a = 1;
    a = a + 101;
    assert_eq(a, 102);

    a = 10;
    a = a - 101;
    assert_eq(a, -91);

    a = 10;
    a = a* 101;
    assert_eq(a, 1010);

    /* precedence test */
    a = 1;
    a = a+ 3 * 4;
    assert_eq(a, 13);

    a = 1;
    a = a - 3 * 4;
    assert_eq(a, -11);

    a = 2;
    a = a* 3 * 4;
    assert_eq(a, 24);

    a = 10;
    a = a/ 5;
    assert_eq(a, 2);

    a = 4;
    a = a % 3;
    assert_eq(a, 1);

   a = 1;
   a = a<< 2;
   assert_eq(a, 4);

   a = 4;
   a = a>> 2;
   assert_eq(a, 1);

    a = 17;
    a = a | 14;
    assert_eq(a, 31);

    /* precedence test */
    a = 0xff;
    b = 1;
    a = a ^( b | 2);
    assert_eq(a, 0xfc);

    a = 17;
    a = a & 7;
    assert_eq(a, 1);

    u8 base = 233;
    u8 *ptr = &base;

    assert_eq((i32)(&ptr[3]),&ptr[5]-2);
    printf("currentline:%d %d\n",_LINE_,_IO_H);
    return 0;
}