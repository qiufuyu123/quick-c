import "../../libqc/io.qh"

i32 mcmp(i8 *a, i8 *b, i32 n)
{
    i32 ret;
    while (n) {
        n=n-1;
        ret = (*a - *b);
        a=a+1;b=b+1;
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
    while (n){
        n=n-1;
        [tag = assign]
        *a = *b;        
        a=a+1;b=b+1;
       
    }
    return 0;
}

i32 main()
{
    u8 p[128];
    for(i32 i =0;i<128;i=i+1){
        p[i]=0;
    }
    i32 v;
    mcpy(p, "hello world", 12);
    printf("%s\n", p);
    printf("memcmp = %d\n", mcmp(p, "hello world", 12));
    printf("memcmp = %d\n", mcmp(p, "hello xorld", 12));
    printf("memcmp = %d\n", mcmp(p, "hello yorld", 12));
    p[0] = -1;
    v = p[0];
    printf("%x %d %d %x\n", p[0], p[0], v, p[1]);
    printf("\0"); /* shall be nothing generated */
    return 0;
}