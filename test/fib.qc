extern u8 printnum(u64 v,u8 len);

u64 fib(u64 a){
    u64 b = 1;
    if(a <= 2){
        return b;
    }
    b = fib(a-1)+fib(a-2);
    return b;
}
printnum(fib(8),1);