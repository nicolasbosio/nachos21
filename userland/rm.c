#include "syscall.h"
#include "../userprog/syscall.h" // Include fix IntelliSense
#include "lib.h"

const char *MISSING_ARGS = "Missing args";
const char *FILE_NOT_FOUND = "File not found";

int main(int argc, char **argv)
{
    char buffer[MAX_LINE];
    if (argc != 1 && argv[0] == NULL)
    {
        strput(MISSING_ARGS);
        return ERROR_RET_VALUE;
    }
    char *fileName = argv[0];    
    if(Remove(fileName) == -1) {
        strput(FILE_NOT_FOUND);
    }
    return 0;
}
