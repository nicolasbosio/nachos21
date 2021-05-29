/// Simple program to test whether running a user program works.
///
/// Just do a “syscall” that shuts down the OS.
///
/// NOTE: for some reason, user programs with global data structures
/// sometimes have not worked in the Nachos environment.  So be careful out
/// there!  One option is to allocate data structures as automatics within a
/// procedure, but if you do this, you have to be careful to allocate a big
/// enough stack to hold the automatics!


#include "syscall.h"
#include "../userprog/syscall.h" // Include fix IntelliSense
#include "lib.h"

int
main(int argc, char **argv)
{
    if (argc > 0) {
        strput("Hello filetest2..");
    }
    Create("test2.txt");
    OpenFileId o = Open("test2.txt");
    Write(argv[0],strlen(argv[0]),o);
    Close(o);
}

