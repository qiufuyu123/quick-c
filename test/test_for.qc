import "../libqc/io.qh"
u8 main(){
    for(u8 i = 0;i<8;i=i+1){
        printnum(i,1);
    }
    printstr("=====");
    for(i = 0;i<5;i=i+2){
        printnum(i,1);
    }
    return 0;
}
main();