

#include <stdlib.h>
#include <stdio.h>
int main(int argc, char**argv){
    if(argc<3){
        printf("Not enough args!\n");
        return -1;
    }
    char *target = argv[1];
    char *src = argv[2];
   
    FILE *f2 = fopen(src, "rb");
    fseek(f2, 0, SEEK_END);
    int len_package = ftell(f2);
    fseek(f2, 0, SEEK_SET);
    char *buffer = calloc(1, len_package);
    printf("pgk len:%d\n",len_package);
    fread(buffer, len_package, 1, f2);
    FILE *f1 = fopen(target, "rb");
    fseek(f1, 0, SEEK_END);
    int len = ftell(f1);
    fseek(f1, 0, SEEK_SET);
    char *buffer_head = calloc(1, len);
    fread(buffer_head, len, 1, f1);
    fclose(f1);
    FILE *out = fopen(argv[3], "wb");
    fwrite(buffer_head, len, 1, out);
    fwrite(buffer, len_package, 1, out);
    fwrite(&len, sizeof(int), 1, out);
    fclose(f1);
    fclose(f2);
    fclose(out);
    //free(buffer);
    return 0;
}