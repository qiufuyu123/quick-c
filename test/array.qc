i8 test(u8 a,u8 b,u8 c){
    return a;
}

u8 a[32];
test(a,&a,&a[0]);