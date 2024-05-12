import "libqc.vm.native"
u8 println(u8* str){
    native.console.prints(str);
    return 1;
}

u8 printptr(u64 ptr){
    native.console.print(ptr,7);
    return 1;
}

u8 printu8(u8 v){
    native.console.print(v,1);
    return 1;
}