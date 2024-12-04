#include <stdio.h>

int main(int argc,char**argv){
    if(argc!=3){
        printf("Usage: bindiff.exe a.bin b.bin");
    }
    char *src = argv[1];
    char *dst = argv[2];
    FILE *f1 = fopen(src, "rb");
    FILE *f2 = fopen(dst, "rb");
    fseek(f1, 0, SEEK_END);
    fseek(f2, 0, SEEK_END);
}