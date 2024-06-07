import "../../libqc/io.qh"

u8 test(u8 a){
    if(a == 3){
        return 3;
    }else if(a%2){
        return 2;
    }else{
        return 99;
    }
    return 4;
}


u8 main(){
    printf("result %d",test(6));
    return 0;
}