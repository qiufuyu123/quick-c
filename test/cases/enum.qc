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

typedef enum{
    E_A,
    E_B,
    E_C,
    E_D=10,
    E_E,
    E_F,
    E_G= 1<<10
}E_;

i8 main(){
    assert_eq(E_A, 0);
    assert_eq(E_B, 1);
    assert_eq(E_C,2);

    assert_eq(E_D, 10);
    assert_eq(E_E, 11);
    assert_eq(E_F,12);
    
    assert_eq(E_G,1024);

    E_ test = E_G;
    printf("test is %d\n",test);
}