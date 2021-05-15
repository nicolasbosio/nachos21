///
///
///
///


#include "synch_console.hh"
#include "stdio.h"

Semaphore *readAvail;
Semaphore *writeDone;
Lock *lockRead;
Lock *lockWrite;

static void
ReadAvail(void *arg)
{
    readAvail->V();
}

static void
WriteDone(void *arg)
{
    writeDone->V();
}

/// Initialize the synchronous interface to the console
SynchConsole::SynchConsole(const char *readFile, const char *writeFile)
{
    console   = new Console(readFile, writeFile, ReadAvail, WriteDone, nullptr);
    readAvail = new Semaphore("read avail", 0);
    writeDone = new Semaphore("write done", 0);
    lockRead = new Lock("synch console read lock");
    lockWrite = new Lock("synch console write lock");
}

/// De-allocate data structures needed for the synchronous console abstraction.
SynchConsole::~SynchConsole()
{
    delete console;
    delete readAvail;
    delete writeDone;
    delete lockRead;
    delete lockWrite;
}

/// Read the contents of a disk sector into a buffer.  Return only after the
/// data has been read.
///
/// * `sectorNumber` is the disk sector to read.
/// * `data` is the buffer to hold the contents of the disk sector.
void 
SynchConsole::Read(char *bufferDest, int size)
{
    ASSERT(bufferDest != nullptr);
    ASSERT(size > 0);
    char ch;
    int count = 0;
    lockRead->Acquire();
    while(count < size && ch != 10) {
        console->CheckCharAvail();
        readAvail->P();
        ch = console->GetChar();
        *bufferDest++ = ch;
        count++;
    }
    lockRead->Release();
}

/// Write the contents of a buffer into a disk sector.  Return only
/// after the data has been written.
///
/// * `sectorNumber` is the disk sector to be written.
/// * `data` are the new contents of the disk sector.
void
SynchConsole::Write(char *buffer, int size)
{
    ASSERT(buffer != nullptr);
    ASSERT(size > 0);
    int count = 0;
    lockWrite->Acquire();
    while(count < size) {
        console->PutChar(*buffer);
        console->WriteDone();
        writeDone->P();
        buffer++;
        count++;
    }
    lockWrite->Release();
}