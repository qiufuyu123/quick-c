#include "../libqc/def.qh"

typedef struct {
    u64 addr;
    u8 *cstr_name;
}_libqc_func;

u8 *printf = 0;

i8 _start_(_libqc_func *list, u8 cnt){
    printf = list[2].addr;
    for (u8 i = 0; i<10; i++) {
        printf("hello world!%d\n",i);
    }
    
    return 0;
}
u8 *cons = "MMM";