#include "../../libqc/io.qh"

u8 arr[32];
i8 test(u64 a,u64 b,u64 c,u64 d){
    printf("a:%llx\n",a);
    printf("b:%llx\n",b);
    printf("c:%llx\n",c);
    printf("d:%d\n",d);

}
i32 main()
{
    arr[0]=1;
    test(arr,&arr,123,arr[0]);
    printf("original:0x%llx %llx 0x%llx data %d",arr,&arr,&arr[0],arr[0]);
    return 0;
}