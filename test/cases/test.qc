
typedef struct  {
    u64 base;
    u64 length;
    u64 type;
    
}limine_memmap_entry;


typedef struct{
    limine_memmap_entry **phy_mem_info;
    u64 phy_mem_entry_num;
    u8 (*t2)(u8 a, ...);
}trampoline_info;
trampoline_info test;
u8 main(){
    u8 a = 2;
    return test.t2();
}   

main();