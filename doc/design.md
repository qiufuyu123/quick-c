## QLF
header  
string_pool
code_area  
relocate_table  
extern_sym_table  

## exception
use gs as a exception tracing data pointer  
currently only support one level exception (no nesting multiple exception)  

```
try{
    throw(3,"aaa");
}catch(Exception e){
    e.type ... // i32
    e.data ... // u64 ptr
}
```