/// Copyright (c) 2016-2021 Docentes de la Universidad Nacional de Rosario.
/// All rights reserved.  See `copyright.h` for copyright notice and
/// limitation of liability and disclaimer of warranty provisions.


#include "transfer.hh"
#include "machine/machine.hh"
#include "threads/system.hh"

#include <string.h>
#include <stdio.h> //BORRAR


static const unsigned MAX_ARG_COUNT  = 32;
static const unsigned MAX_ARG_LENGTH = 128;

/// Count the number of arguments up to a null (which is not counted).
///
/// Returns true if the number fit in the established limits and false if
/// too many arguments were provided.
static inline
bool CountArgsToSave(int address, unsigned *count)
{
    ASSERT(address != 0);
    ASSERT(count != nullptr);

    int val;
    unsigned c = 0;
    do {
        machine->ReadMem(address + 4 * c, 4, &val);
        c++;
    } while (c < MAX_ARG_COUNT && val != 0);
    if (c == MAX_ARG_COUNT && val != 0) {
        // The maximum number of arguments was reached but the last is not
        // null.
        return false;
    }

    *count = c - 1;
    return true;
}

char **
SaveArgs(int address)
{
    ASSERT(address != 0);

    unsigned count;
    if (!CountArgsToSave(address, &count)) {
        return nullptr;
    }

    DEBUG('e', "Saving %u command line arguments from parent process.\n",
          count);

    // Allocate an array of `count` pointers.  We know that `count` will
    // always be at least 1.
    char **args = new char * [count + 1];

    for (unsigned i = 0; i < count; i++) {
        args[i] = new char [MAX_ARG_LENGTH];
        int strAddr;
        // For each pointer, read the corresponding string.
        machine->ReadMem(address + i * 4, 4, &strAddr);
        ReadStringFromUser(strAddr, args[i], MAX_ARG_LENGTH);
    }
    args[count] = nullptr;  // Write the trailing null.

    return args;
}

unsigned
WriteArgs(char **args)
{
    ASSERT(args != nullptr);

    DEBUG('e', "Writing command line arguments into child process.\n");

    // Start writing the strings where the current SP points.  Write them in
    // reverse order (i.e. the string from the first argument will be in a
    // higher memory address than the string from the second argument).
    int argsAddress[MAX_ARG_COUNT];
    unsigned c;
    int sp = machine->ReadRegister(STACK_REG);
    for (c = 0; c < MAX_ARG_COUNT; c++) {
        if (args[c] == nullptr) {   // If the last was reached, terminate.
            break;
        }
        printf("SP0: %d\n", sp); //BORRAR
        sp -= strlen(args[c]) + 1;  // Decrease SP (leave one byte for \0).
        printf("SP1: %d\n", sp); //BORRAR
        WriteStringToUser(args[c], sp);  // Write the string there.
        argsAddress[c] = sp;        // Save the argument's address.
        delete args[c];             // Free the string.
    }
    delete args;  // Free the array.
    ASSERT(c < MAX_ARG_COUNT);

    sp -= sp % 4;     // Align the stack to a multiple of four.
    printf("SP2: %d\n", sp); //BORRAR
    sp -= c * 4 + 4;  // Make room for `argv`, including the trailing null.
    printf("SP3: %d\n", sp); //BORRAR
    // Write each argument's address.
    for (unsigned i = 0; i < c; i++) {
        printf("SPFOR: %d\n", sp + 4 * i); //BORRAR
        machine->WriteMem(sp + 4 * i, 4, argsAddress[i]);
    }
    printf("SPNULL: %d\n", sp + 4 * c); //BORRAR
    machine->WriteMem(sp + 4 * c, 4, 0);  // The last is null.

    machine->WriteRegister(STACK_REG, sp);
    return c;
}
