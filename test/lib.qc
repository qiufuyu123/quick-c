console.print(console.whoami,7);
console.print(console.print,7);

console.whoami()()()();
u64* log = console.print;
log(222,2);

u8 test()
{
    u64* log2 = console.print;
    log2(432,2);
    return 1;
}
test();
console.prints("hello world!");
console.print("hello world!"[0],1);