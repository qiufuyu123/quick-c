import "string.qh"
import "io.qh"
u32 strlen(u8 *str){
    u32 len = 0;
    while(*str){
        len=len+1;
        str = str+1;
    }
    return len;
}

i8 strcmp(u8 *str,u8 *str2){
    while((*str != 0) && (*str == *str2)){
        str = str +1;
        str2 = str2 +1;
        
    }
    i8 t = *str - *str2;
    return t;
}