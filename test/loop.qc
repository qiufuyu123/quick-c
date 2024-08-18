#include "../libqc/io.qh"
// #include <stdio.h>

u8 cnt=0;

u8 test(){
    while(cnt<10){
        printf("bello:%d\n",cnt);
        cnt=cnt+1;
    }

    u8 cnt2=0;
    while(1){
        if(cnt2>=5){
            break;
        }
        //printstr("cello!");
        printf("%d\n",cnt2);
        cnt2=cnt2+1;
    }
    return 1;
}
u8 main(){
    test();
    return 0;
}