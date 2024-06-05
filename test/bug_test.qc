import "../libqc/io.qh"

i8 main(){
    u8 *stra = "hello";
    u8 *strb = "bello";
    printnum(*(stra+1),1);
    printnum(*(strb+1),1);
    printnum(*(stra+0+0) == *(strb+1));
    return 0;
}

main();