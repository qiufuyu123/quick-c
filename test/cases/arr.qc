#include "../../libqc/io.qh"

u8 arr[32];
i8 test(u64 a,u64 b,u64 c,u64 d){
    printf("a:%llx\n",a);
    printf("b:%llx\n",b);
    printf("c:%llx\n",c);
    printf("d:%d\n",d);

}
typedef struct{
    u64 a;
    u8 v1[10];
    u16 v2[10];
    u32 v3[10];
    u64 v4[10];
}t2_t;
i32 main()
{
    t2_t t;
    t2_t *ptr = &t;
    arr[0]=1;
    test(arr,&arr,123,arr[0]);
    printf("original:0x%llx %llx 0x%llx data %d\n",arr,&arr,&arr[0],arr[0]);
    printf("%x  ==  %x\n",&t.v1[1]-t.v1,1);
    printf("%x  ==  %x\n",&t.v2[1]-t.v2,2);
    printf("%x  ==  %x\n",&t.v3[1]-t.v3,4);
    printf("%x  ==  %x\n",&t.v4[1]-t.v4,8);
    printf("%llx --   %llx -- %llx\n",&t.v1[0],&t.v1[1],(&t.v1[1])-(&t.v1[0]));
    u64 a = &ptr.v2[1]-&ptr.v2[0];
    printf("%llx --   %llx -- %llx -- %llx\n",&ptr.v1[0],&ptr.v1[1],&ptr.v2[1]-&ptr.v2[0],a);
    printf("'0' + 1 = '%c'\n",'0'+1);
    return a;
}