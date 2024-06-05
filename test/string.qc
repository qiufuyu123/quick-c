import "../libqc/io.qh"
import "../libqc/string.qh"
u8 main(){
u8 *text = "hello world!";
    printstr(text);
    printnum(strlen(text),1);
    printnum(strcmp("23345","2334"),0);
    return 0;
}
main();