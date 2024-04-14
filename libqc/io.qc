import "libqc.vm.native"
u8 println(u8* str){
    native.console.prints(str);
    return 1;
}