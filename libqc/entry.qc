
/*
    libqc main
*/

extern u8 main();

struct _libqc_func{
    u64 addr;
    u8 *cstr_name;
};

_libqc_func *_libqc_func_list;

u8 _start_(_libqc_func** list,u8 cnt){
    _libqc_func_list = list;
    return main();
}

