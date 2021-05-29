#include "syscall.h"
#include "../userprog/syscall.h" // Include fix IntelliSense
#include "lib.h"

int
main(void)
{
    Create("test_lib.txt");
    OpenFileId o = Open("test_lib.txt");
    const char *str = "success\nhola\ncomo\nestas";
    Write(str, strlen(str), o);
    Close(o);
    int letra = -658046456;
    char letrita[10];
    itoa(letra, letrita);
    strput("LETRA: ");
    strput(letrita);
}