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

i8 print_bin(u8 *bin, i32 len){
    for(i32 i =0;i<len;i++){
        printf("%x\n",bin[i]);
    }
    return 0;
}

i8 print_u64(u64 *bin, i32 len){
    u32 real_len = *(u32*)((u64)bin-4);
    printf("real len:%d\n",real_len);
    for(i32 i =0;i<len;i++){
        printf("%llx\n",bin[i]);
    }
    return 0;
}

i8 main(){
    u8 *bin_data = [u8 0x1,2,3,4,5,6,6+1];
    printf("%llx\n",bin_data);
    print_bin(bin_data,7);
    u64 *bin_data2 = [u64 0x1234,0x5678];
    print_u64(bin_data2, 2);
    return 1;
}