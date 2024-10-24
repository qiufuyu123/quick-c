#include "../../libqc/io.qh"
typedef struct{
    i8 name[64];
}test_t;
test_t t1;
t1.name[0] = 'h';
t1.name[1] = 'e';
t1.name[2] = 0;

i8 main(){
    test_t *ptr = &t1;
    printf("0x%llx %llx %c\n",&t1.name[0],t1.name,t1.name[1]);
    printf("0x%llx %llx\n",&ptr->name[0],ptr->name);

}