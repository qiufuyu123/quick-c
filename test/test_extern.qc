import "../libqc/io.qh"
extern u8 var(u8 op1);
u8 main(){
    printnum(var(),2);
    return 0;
}
u8 var(u8 op1){
    return 233;
}
main();