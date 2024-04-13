u8 cnt=0;
while(cnt<10){
    console.prints("bello!");
    console.print(cnt,1);
    cnt=cnt+1;
}

u8 test(){
    u8 cnt2=0;
    while(cnt2<10){
        console.prints("cello!");
        console.print(cnt2,1);
        cnt2=cnt2+1;
    }
    return 1;
}
test();