#include "../../include/qlib.h"
#include "../../include/debuglib.h"
#include "../../include/define.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>

char is_debug = 0; 

native_func_t native_list[DBG_NUM] = {
    {(u64)0,"printnum"},
    {0,"printstr"},
    {(u64)printf,"printf"},
    {(u64)calloc,"calloc"},
    {(u64)free,"free"}
};

int link(char *buffer_old, int offset){
    char *buffer = buffer_old+offset;
    qlib_exe_t file_header;
    memcpy(&file_header, buffer, sizeof(file_header));
    if(strncmp(&file_header.magic[0], "QLIB", 4)){
        printf("Bad Magic!\n");
        return -1;
    }
    if(is_debug){
        printf("Qlib, Ver:%d,%s\n",file_header.version,file_header.compile_info);
        printf("----- Base Addr -----\n");
        printf("Code: %llx (%d bytes)\nData: %llx (%d bytes)\nStr : %llx (%d bytes)\n",
        file_header.code_base,file_header.code_sz,
        file_header.data_base,0,
        file_header.str_base,file_header.str_sz);
        printf("Relocate Entries Num: %d\n",file_header.reloc_num);
    }
    char *pc_mem = mmap(NULL,             // address
                      file_header.code_sz,             // size
                      PROT_READ | PROT_WRITE | PROT_EXEC,
                      MAP_PRIVATE | MAP_ANONYMOUS,
                      -1,               // fd (not used here)
                      0);               // offset 
    if(!pc_mem){
        printf("OOM pc_mem\n");
        return -1;
    }
    memcpy(pc_mem, buffer+file_header.code_offset, file_header.code_sz);
    char *data_mem = calloc(1, file_header.data_sz);
    char *str_mem = calloc(1, file_header.str_sz+1);
    if(!data_mem || !str_mem){
        printf("OOM Data_mem\n");
        return -1;
    }
    memcpy(str_mem, buffer+file_header.str_offset, file_header.str_sz);
    u32 *relocate_addr = (u32*)(buffer+file_header.reloc_table_offset);
    for (int i=0; i<file_header.reloc_num; i++) {
        u64 *adr_ptr = (u64*)( pc_mem + (*relocate_addr));
        u64 adr = *adr_ptr;
        if(adr >= file_header.code_base && adr <= file_header.code_base+file_header.code_sz){
            *adr_ptr = adr - file_header.code_base + (u64)pc_mem;
            //printf("Rel: %llx --> %llx CODE\n",adr,*adr_ptr);
        }else if(adr >= file_header.data_base && adr <= file_header.data_base+file_header.data_sz){
            *adr_ptr = adr - file_header.data_base + (u64)data_mem;
            //printf("Rel: %llx --> %llx DATA\n",adr,*adr_ptr);
        }else if(adr >= file_header.str_base && adr <= file_header.str_base+file_header.str_sz){
            *adr_ptr = adr - file_header.str_base+(u64)str_mem;
            //printf("Rel: %llx --> %llx STR\n",adr,*adr_ptr);            
        }else {
            printf("Rel: %llx  UNKNOW!\n",adr);
            return  -1;
        }
        relocate_addr++;
    }
    free(buffer_old);
    
    if(file_header.main_entry){
        u8 *entry_point = file_header.main_entry - file_header.code_base + pc_mem;
        //printf("Entry Point: %llx\n",(u64)entry_point);
        int (*exec)(native_func_t *) = entry_point;
        return exec(native_list);
    }else {
        printf("NO Entry Point!\n");
        return -1;
    }
    
    return 1;
}

void check_self_contain(char *path){
    FILE *f = fopen(path, "rb");
    fseek(f, 0, SEEK_END);
    int len = ftell(f);
    char *buffer = calloc(1, len);
    fseek(f, 0, SEEK_SET);
    fread(buffer, len, 1, f);
    u32 skip_size = *(u32*)(buffer+len-sizeof(u32));
    if(skip_size<len){
        char *magic = buffer+skip_size;
        if(!strncmp(magic, "QLIB", 4)){
            exit(link(buffer,skip_size));
        }
    }
    fclose(f);
    //free(buffer);
}


int main(int argc, char** argv){
    check_self_contain(argv[0]);
    char *buffer = 0;
    if(argc>1){
        FILE *f = fopen(argv[1], "rb");
        if(argc>2){
            if(!strcmp(argv[2], "-d")){
                is_debug = 1;
            }
        }
        if(!f){
            printf("Fail to open:%s\n",argv[1]);
            return -1;;
        }
        fseek(f, 0, SEEK_END);
        int len = ftell(f);
        buffer = calloc(1, len);
        if(!buffer){
            printf("OOM\n");
            return -1;
        }
        fseek(f, 0, SEEK_SET);
        fread(buffer, len, 1, f);
        return link(buffer,0);
    }
}