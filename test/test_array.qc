#include "../libqc/io.qh"
u8 main(){
    u8 list1[10];
    u8* addr_list[10];
    u8 index = 0;
    while(index<10){
        list1[index]=index;
        addr_list[index] = &list1[index];
        index=index+1;
    }
    index = 0;
    while(index<10){
        printnum(*addr_list[index],1);
        list1[index]=202;
        index=index+1;
    }
    index = 0;
    while(index<10){
        printnum(list1[index],1);
        index=index+1;
    }
    return 0;
}

main();