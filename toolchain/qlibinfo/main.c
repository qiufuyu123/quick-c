#include<stdio.h>
#include<stdlib.h>
#include <string.h>

#include "../../include/qlib.h"

int main(int argc, char **argv){
    if(argc < 2){
        printf("Usage: qlibinfo <filename>\n");
        exit(-1);
    }

    FILE *f = fopen(argv[1], "rb");
    qlib_exe_t file_header;
    fread(&file_header, sizeof(qlib_exe_t), 1, f);
    if(strncmp(file_header.magic, "QLIB", 4)){
        printf("QLIB signature not found!\n");
        exit(-1);
    }
    printf("Qlib file format recognized!\n");
    printf("Version: %d\nInfo: %s\n",file_header.version,file_header.compile_info);
    printf("Code : %dbytes\nData : %dbytes\nStr : %dbytes\nGot : %dbytes\nExport : %d elems\n",file_header.code_sz,
    file_header.data_sz,
    file_header.str_sz,
    file_header.got_size,
    file_header.export_size/8);
    fseek(f, file_header.export_offset, SEEK_SET);
    u32 *export_table = calloc(1, file_header.export_size);
    fread(export_table, file_header.export_size, 1, f);
    char *str_table = calloc(1, file_header.str_sz);
    fseek(f, file_header.str_offset, SEEK_SET);
    fread(str_table, file_header.str_sz, 1, f);
    // printf("%s\n",str_table);
    printf("Export table:\n");
    for (int i =0; i<file_header.export_size/8; i++) {
        u32 name = *export_table;
        export_table++;
        u32 got_index = *export_table;
        export_table++;
        printf("@ %s --> %d\n",str_table+name,got_index);
    }
    

}