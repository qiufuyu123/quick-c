import "libqc.io"


u64 fib(u64 a){
    u64 b = 1;
    if(a <= 2){
        return b;
    }
    b = fib(a-1)+fib(a-2);
    return b;
}
io.println("233");
io.printu8(fib(8));