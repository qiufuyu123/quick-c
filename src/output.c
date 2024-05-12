#include "define.h"
#include "hashmap.h"
#include "parser.h"
#include "vec.h"
#include "vm.h"
#include <bits/types/FILE.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#pragma pack(1)
typedef struct{
    u32 magic;
    u32 program_len;
    u32 str_len;
    u32 heap_len;
    u32 reloc_len;
}metainfo_t;
#pragma pack()

static u64 base_pc,base_data,base_str,code;
static u64 base_reloc;
static u32 heap_data_total,str_data_total,reloc_total,program_len;
void module_single_pack(module_t *m){
    memcpy((char*)code+base_pc, m->jit_compiled, m->jit_cur);
    memcpy((char*)(code+base_str), m->str_table.data, m->str_table.size);
    for (int i = 0; i<m->reloc_table.size; i++) {
        u64 *addr=  (u64*)*(u64*)vec_at(&m->reloc_table, i);
        u64 *dst =(u64*)((u64)addr - (u64)m->jit_compiled + (u64)code+base_pc);
        if(*addr>=(u64)m->jit_compiled && *addr<=(u64)m->jit_compiled+m->jit_cur){

            *dst = *addr-(u64)m->jit_compiled+base_pc;
            printf("code :%llx --> %llx\n",*addr,*dst);
        }else if(*addr>=(u64)m->str_table.data && *addr<=(u64)m->str_table.data+m->str_table.size){
            printf("str: %llx --> %llx\n",*addr,*dst);
            *dst = *addr-(u64)m->str_table.data+base_str;
        }else if(*addr>=(u64)m->heap.data && *addr<=(u64)m->heap.data+m->heap.size) {
            *dst = *addr-(u64)m->heap.data+base_data;
            printf("data :%llx --> %llx\n",*addr,*dst);
        }else {
            printf("Unknow reloc segment: 0x%llx\n",*addr);
        }
        //*(u64*)(code+(u64)base_reloc) = (u64)*addr - (u64)m->jit_compiled;
        //base_reloc+=8;
    }
    base_pc += m->jit_cur;
    base_data += m->heap.size;
    base_str += m->str_table.size;
}

int iter_all_mod(void *const context,struct hashmap_element_s *const e){
    char buf[100]={0};
    memcpy(buf, e->key, e->key_len);
    u32 *len = (u32*)context;
    module_t *m = (module_t*)(e->data);
    printf("Scan Module: %s ... ",buf);
    if(!m->is_native){
        *len = *len + m->jit_cur;
        heap_data_total += m->heap.size;
        str_data_total += m->str_table.size;
        reloc_total += m->reloc_table.size*8;
        printf("Imported --> Counted!\n");
    }else {
        printf("Native --> Skip!\n");
    }

    return 0;
}

int iter_pack_mod(void *const context, struct hashmap_element_s *const e){
    char buf[100]={0};
    memcpy(buf, e->key, e->key_len);
    module_t *m = (module_t*)(e->data);
    printf("Pack Module: %s ... ",buf);
    if(!m->is_native){
        module_single_pack(m);
        printf("Imported --> OK\n");
    }else {
        printf("Native --> Skip!\n");
    }

    return 0;
}

char* module_pack(module_t *m,char *output){
    u32 mem_need = m->jit_cur;
    heap_data_total = m->heap.size;
    str_data_total = m->str_table.size;
    reloc_total = m->reloc_table.size*8;
    hashmap_iterate_pairs(&glo_libs,iter_all_mod,&mem_need);
    
    char *codes = calloc(1, mem_need+str_data_total+reloc_total);
    if(!codes){
        printf("Fail to allocate size:%d\n",mem_need);
        exit(-1);
    }
    code = (u64)codes;
    base_pc = 0;
    base_str = mem_need;
    program_len = mem_need;
    // after codes segment
    // we place string pool
    base_data = mem_need+str_data_total;
    // after string pool
    // we place heap data
    base_reloc = base_data;
    printf("Start Packing ... \n");
    hashmap_iterate_pairs(&glo_libs,iter_pack_mod,0);
    printf("Pack Imports Finished, Now Pack Main Module...\n");
    module_single_pack(m);
    printf("Pack Main Module Finised!\n");
    metainfo_t meta;
    meta.magic = 0x20240512;
    meta.program_len = mem_need;
    meta.str_len = str_data_total;
    meta.heap_len = heap_data_total;
    meta.reloc_len = reloc_total;
    printf("Strsize:%d\n",meta.str_len);
    FILE *f = fopen(output, "wb");
    fwrite(&meta, sizeof(meta), 1, f);
    fwrite(codes, mem_need+str_data_total+reloc_total, 1, f);
    fclose(f);
    printf("Finished!\n");
    return codes;
}