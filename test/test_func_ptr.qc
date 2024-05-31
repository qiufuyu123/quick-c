import "../libqc/io.qh"

u8 (*funcptr)(u8 arg1,u8 arg2);

u8 main(){
    funcptr = printnum;
    printnum(funcptr,7);
    funcptr(printnum,7);
    return 0;
}

main();