/// Test routines for demonstrating that Nachos can load a user program and
/// execute it.
///
/// Also, routines for testing the Console hardware device.
///
/// Copyright (c) 1992-1993 The Regents of the University of California.
///               2016-2021 Docentes de la Universidad Nacional de Rosario.
/// All rights reserved.  See `copyright.h` for copyright notice and
/// limitation of liability and disclaimer of warranty provisions.


#include "address_space.hh"
#include "machine/console.hh"
#include "threads/semaphore.hh"
#include "threads/system.hh"
#include "synch_console.hh"
#include "syscall.h"

#include <stdio.h>
#include <string.h>

/// Data structures needed for the console test.
///
/// Threads making I/O requests wait on a `Semaphore` to delay until the I/O
/// completes.

static Console   *console;
static Semaphore *readAvail;
static Semaphore *writeDone;

/// Console interrupt handlers.
///
/// Wake up the thread that requested the I/O.

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

/// Test the console by echoing characters typed at the input onto the
/// output.
///
/// Stop when the user types a `q`.
void
ConsoleTest(const char *in, const char *out)
{
    console   = new Console(in, out, ReadAvail, WriteDone, 0);
    readAvail = new Semaphore("read avail", 0);
    writeDone = new Semaphore("write done", 0);

    for (;;) {
        readAvail->P();        // Wait for character to arrive.
        char ch = console->GetChar();
        console->PutChar(ch);  // Echo it!
        writeDone->P();        // Wait for write to finish.
        if (ch == 'q') {
            return;  // If `q`, then quit.
        }
    }
}

static SynchConsole *synchConsole2;

/// Test the console by echoing characters typed at the input onto the
/// output.
///
/// Stop when the user types a `q`.
void
SynchConsoleTest(const char *in, const char *out)
{
    synchConsole2   = new SynchConsole(in, out);
    char *text = new char[40];
    char *read = new char[20];
    sprintf(text, "Ingrese un comando\n");
    synchConsole2->Write(text, strlen(text));
    synchConsole2->Read(read, 20);
    sprintf(text, "El comando ingresado es: %s\n", read);
    printf("PRINTF READ: %s\n", read);
    printf("PRINTF TEXT: %s\n", text);
    synchConsole2->Write(text, strlen(text));
}

void
SynchConsoleTest2(const char *in, const char *out)
{

}
