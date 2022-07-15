#include "syscall.h"
#include "../userprog/syscall.h" // Include fix IntelliSense
#include "lib.h"

const char *MISSING_ARGS = "Missing args";
const char *FILE_NOT_FOUND = "Source file not found";

int main(int argc, char **argv)
{
    char buffer[MAX_LINE];
    if(argc != 1 && argv[0] == NULL) {
        strput(MISSING_ARGS);
        return ERROR_RET_VALUE;
    }
    char *src = argv[0];
    OpenFileId srcFile = Open(src);

    if(srcFile == -1)
    {
        strput(FILE_NOT_FOUND);
        return ERROR_RET_VALUE;
    }
    
    char c = 'a';
    int count = 0;

    while(Read(&c, 1, srcFile))
    {
        Write(&c, 1, CONSOLE_OUTPUT);
    }

    strput("");
    Close(srcFile);
    strput("PRE RETURN\n");
    return 0;
}

