#include "../../libqc/io.qh"
u64 fib(u64 a){
    u64 b = 1;
    if(a <= 2){
        return b;
    }
    b = fib(a-1)+fib(a-2);
    return b;
}
u8 main(){
    printf("%d %d %d %d %d \n",fib(8),2,3,4,5);
    return 0;
}