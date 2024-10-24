#include "../../libqc/io.qh"

u8 test(u8 a){
    if(a == 3){
        return 3;
    }else if(a%2){
        return 2;
    }else{
        return 99;
    }
    return 4;
}
u8 test_loop(){
    for(u8 i=0;i<8;i++){
        printf("%d\n",i);
    }
    return 0;
}

typedef struct{
    u64 a;
    u64 b;
    u8 c[36];
}test_t;

u8 main(){
    test_t tt;
    printf("result %d\n",test(6));
    i8 c = -1;
    printf("i8 %x\n",c<0);
    printf("offset :%d\n size:%d\n", offsetof(test_t,b),sizeof(test_t));
    test_loop();
    return 0;
}