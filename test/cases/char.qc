#include "../../libqc/io.qh"

i32 mcmp(i8 *a, i8 *b, i32 n)
{
    i32 ret;
    while (n--) {
        ret = (*a++ - *b++);
        //a=a+1;b=b+1;
        if (ret){
            return ret;
        }
    }
    return 0;
}

u8 mcpy(u8 *a, u8 *b, i32 n)
{
    u8 *dst;
    dst = a;
    while (n--){
        //n=n-1;
        
        *a++ = *b++;        
        //a=a+1;b=b+1;
    }
    return 0;
}

i32 main()
{
    u8 p[128];
    for(i32 i =0;i<128;i=i+1){
        //printf("%d\n",i);
        p[i]=0;
    }
    //printf("stage1");
    i32 v;
    mcpy(p, "hello world", 12);
    printf("%s\n", p);
    printf("memcmp = %d\n", mcmp(p, "hello world", 12));
    printf("memcmp = %d\n", mcmp(p, "hello xorld", 12));
    printf("memcmp = %d\n", mcmp(p, "hello yorld", 12));
    // *p = -1;
    // v = *p;
    // printf("%x %d %d %x\n", p[0], *p, v, *(p+1));
    printf("\0"); /* shall be nothing generated */
    return 0;
}