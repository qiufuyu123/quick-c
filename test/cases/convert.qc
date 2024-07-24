#include "../../libqc/def.qh"
#include "../../libqc/io.qh"
typedef struct{
    u8 a;
    u16 b;
    u32 c;
    u8* d;
} test_struct;

i8 main(){
    test_struct s1;
    s1.a = 1;
    s1.b = 2;
    s1.c = 3;
    s1.d = (u8*)&s1.b;
    printf("s1: %d %d %d %d\n",s1.a,s1.b,s1.c,*s1.d);
    u64 adr = (u64)&s1;
    printf("s1: %d\n",*((test_struct*)(adr))->d);
    u64 num_64 = 0x1111222233334444;
    u8 num_8 = (u8)num_64;
    printf("%llx %llx\n",num_64,num_8);
    
    
    
    
    printf("sizeof: %d %d %d %d\n",sizeof 1 test_struct),sizeof(test_struct*),sizeof(u16),sizeof(u8));
    return 0;
}