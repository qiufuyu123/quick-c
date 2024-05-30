import "lib3.qc"
import "../libqc/io.qh"
import "../libqc/LinkedList.qh"
extern u8 var;
//u8 var=3;
extern u8 func2();
u8 add(u8 op, u8 op2){
    return op+op2;
}
u8 main(){
    u8 a = 1;
    a = add(a+lib_func_1(),2);
    LinkNode node1;
    LinkNodeInit(&node1,123,2);
    printnum(node1.this,1);
    printnum(a+var+func2());
    return 0;
}
u8 var = 3;
extern u8 func2();
u8 func2(){
    return 1;
}
extern u8 var;
extern u8 func2();
