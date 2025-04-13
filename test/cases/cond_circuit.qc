// #include "../../libqc/def.qh"
// #include "../../libqc/io.qh"
// #define _IO_H

// i32 assert_eq(i32 a, i32 b)
// {
//     printf("a:%d,b:%d\n",a,b);
//     if (a != b) {
//         printf("ASSERT!\n");
        
//     }else{

//         printf("[PASS]\n");
//     }
//     return 0;
// }

// typedef struct{
//     u64 a;
// }test_t;

// i32 main(){
//     test_t *t1 = 0;
//     u64 a = 0;
//     printf("%llx\n",a==0);
//     if( (a==0)){
//         printf("You should see this branch (0 || 0||0||0||1)\n");
//     }
//     if(1 && 0 ){
//         printf("[ERROR ]You should NOT see this branch (1 && 0)\n");
//     }else{
//         printf("You should see this branch (1 && 0)\n");
//     }
//     if(1 || 0 ){
//         printf("You should see this branch (1 || 0)\n");
//     }else{
//         printf("[ERROR ]You should NOT see this branch (1 || 0)\n");
//     }
//     if(0 && 1 ){
//         printf("[ERROR ]You should NOT see this branch (0 && 1)\n");
//     }else{
//         printf("You should see this branch (0 && 1)\n");
//     }
//     if(0 || 1 ){
//         printf("You should see this branch (0 || 1)\n");
//     }else{
//         printf("[ERROR ]You should NOT see this branch (0 || 1)\n");
//     }
//     if( 1 && (1 || 0)){
//     if(t1 && t1->a){
//         printf("[ERROR ]You should NOT see this branch (t1 && t1->a)\n");
//     }else{
//         printf("You should see this branch (t1 && t1->a)\n");
//     }
//     }
//     return 0;
// }

u64 a=1;
if(a==0||a==1){
    1;
}else{
    0;
}