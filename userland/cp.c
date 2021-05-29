#include "syscall.h"
#include "../userprog/syscall.h" // Include fix IntelliSense
#include "lib.h"

const char *MISSING_ARGS = "Missing args for copy";
const char *FILE_NOT_FOUND = "Source file not found";
const char *CANT_CREATE_NEW_FILE = "Couldn't create new file";
const char *NEW_FILE_NOT_FOUND = "New file not found";

int main(int argc, char const *argv[])
{
    if(argc != 2)
    {
        char buffer[10];
        itoa(argc, buffer);
        strput(buffer);
        strput(MISSING_ARGS);
        return ERROR_RET_VALUE;
    }

    char const *src = argv[0];
    char const *out = argv[1];

    OpenFileId srcFile = Open(src);

    if(srcFile == -1)
    {
        strput(FILE_NOT_FOUND);
        return ERROR_RET_VALUE;
    }

    if(!Create(out))
    {
        strput(CANT_CREATE_NEW_FILE);
        return ERROR_RET_VALUE;
    }

    OpenFileId outFile = Open(out);

    if (outFile == -1)
    {
        strput(NEW_FILE_NOT_FOUND);
        return ERROR_RET_VALUE;
    }

    char c = 'a';
    int count = 0;
    char buffer[MAX_LINE];
    while (c != EOF)
    {
        do
        {
            Read(&c, 1, srcFile);
            buffer[count] = c;
            count++;
        } while (c != EOF && c != '\n' && count < MAX_LINE);
        Write(buffer, count, outFile);
        count = 0;
    }
    
    Close(srcFile);
    Close(outFile);

    return 0;
}

