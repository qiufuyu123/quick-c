u8 *a = array.new(1,8);
console.print(a,7);
a[0]=1;
a[1]=2;
a[2]=3;

console.print(a[0],0);
console.print(a[1],0);
console.print(a[2],0);
console.print(a,7);
array.free(a);

u8 test(){
    u8 *b = array.new(1,8);
    console.print(b,7);
    b[0]=1;
    b[1]=2;
    b[2]=3;
    u8 **c = array.new(8,2);
    c[0]= &b[0];
    c[1]= &b[1];
    console.print(*c[0],0);
    console.print(b[0],0);
    console.print(b[1],0);
    console.print(b[2],0);
    console.print(b,7);
    array.free(b);
    return 0;
}
test();