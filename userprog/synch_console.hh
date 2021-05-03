/// Data structures to export a synchronous interface to the raw disk device.
///
/// Copyright (c) 1992-1993 The Regents of the University of California.
///               2016-2021 Docentes de la Universidad Nacional de Rosario.
/// All rights reserved.  See `copyright.h` for copyright notice and
/// limitation of liability and disclaimer of warranty provisions.

#ifndef NACHOS_FILESYS_SYNCHCONSOLE__HH
#define NACHOS_FILESYS_SYNCHCONSOLE__HH


#include "machine/disk.hh"
#include "threads/lock.hh"
#include "machine/console.hh"


///
///
/// COMENTAR
///
///
///
class SynchConsole {
public:

    /// 
    SynchConsole(const char *readFile, const char *writeFile);

    /// 
    ~SynchConsole();

    /// 
    /// 
    /// 

    void Read(char *bufferDest, int size);
    void Write(char *buffer, int size);

private:
    Console *console;
};


#endif
