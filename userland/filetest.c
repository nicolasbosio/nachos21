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
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>


int
main(void)
{
    Create("test.txt");
    OpenFileId o = Open("test.txt");
    Write("Hello world\n",12,o);
    Close(o);
    o = Open("test.txt");
    char temp[12];
    Read(temp, 12, o);
    Write(temp, 12, 1);
    Close(o);
    Remove("test.txt");
    Halt();
}
