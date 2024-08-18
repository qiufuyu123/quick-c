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


u8 main(){
    printf("result %d",test(6));
    test_loop();
    return 0;
}