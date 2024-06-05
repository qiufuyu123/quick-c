import "../libqc/io.qh"
u64 v;
u8 main(){
    v =_libqc_func_list[1].addr;
    u64 a=1,b=2;
    //u64 v = (&_libqc_func_list[0]).addr;
    printnum(v,7);
    printnum(printnum,7);
    printnum(printstr,7);

    printnum(&v,7);
    return 0;
}