#include "../../libqc/io.qh"
i32 fib(u64 a){
    u64 b = 1;
    if(a <= 2){
        return b;
    }
    b = fib(a-1)+fib(a-2);
    return b;
}
u8 main(){
    printf("%d %d %d %d %d \n",fib(1),fib(2),fib(3),fib(4),fib(5));
    return 0;
}