import "libqc.io"
u8 cnt=0;
while(cnt<10){
    io.println("bello!");
    io.printu8(cnt);
    cnt=cnt+1;
}

u8 test(){
    u8 cnt2=0;
    while(cnt2<10){
        io.println("cello!");
        io.printu8(cnt2,1);
        cnt2=cnt2+1;
    }
    return 1;
}
test();