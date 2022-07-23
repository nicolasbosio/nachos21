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
main(void)
{
    Create("test.txt");
    OpenFileId o = Open("test.txt");
    char *args[] = {"HOLAcm7\n"};
    SpaceId proc1 = Exec("../userland/filetest2", 1, args);
    char *cadena = "Hello world\n";
    Write(cadena, strlen(cadena), o);
    Close(o);
    int i = Join(proc1);
    if (i != -1)
        strput("Retorno bien el Join 1\n");

    char *args2[] = {"HOLAcm72\n"};
    SpaceId proc2 = Exec("../userland/filetest2", 1, args2);
    char *cadena2 = "Hello world2\n";
    strput(cadena2);
    int i2 = Join(proc2);
    if (i2 != -1) {
        char *msg = "Retorno bien el Join 2\n";
        Write(msg,strlen(msg),1);
    }
    strput("FIN FILETEST");
    return 0;
}

