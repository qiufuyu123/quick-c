
extern u8 printnum(u64 v,u8 len);
extern u8 printstr(u8 *str);
u8 cnt=0;
while(cnt<10){
    printstr("bello!");
    printnum(cnt,1);
    cnt=cnt+1;
}

u8 test(){
    u8 cnt2=0;
    while(1){
        if(cnt2>=5){
            break;
        }
        //printstr("cello!");
        printnum(cnt2,1);
        cnt2=cnt2+1;
    }
    return 1;
}
test();