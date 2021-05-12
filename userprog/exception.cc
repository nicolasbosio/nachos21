/// Entry points into the Nachos kernel from user programs.
///
/// There are two kinds of things that can cause control to transfer back to
/// here from user code:
///
/// * System calls: the user code explicitly requests to call a procedure in
///   the Nachos kernel.  Right now, the only function we support is `Halt`.
///
/// * Exceptions: the user code does something that the CPU cannot handle.
///   For instance, accessing memory that does not exist, arithmetic errors,
///   etc.
///
/// Interrupts (which can also cause control to transfer from user code into
/// the Nachos kernel) are handled elsewhere.
///
/// For now, this only handles the `Halt` system call.  Everything else core-
/// dumps.
///
/// Copyright (c) 1992-1993 The Regents of the University of California.
///               2016-2021 Docentes de la Universidad Nacional de Rosario.
/// All rights reserved.  See `copyright.h` for copyright notice and
/// limitation of liability and disclaimer of warranty provisions.


#include "transfer.hh"
#include "syscall.h"
#include "filesys/directory_entry.hh"
#include "threads/system.hh"
#include "threads/thread.hh"
#include "synch_console.hh"
#include "address_space.hh"
#include <stdio.h>

static void
IncrementPC()
{
    unsigned pc;

    pc = machine->ReadRegister(PC_REG);
    machine->WriteRegister(PREV_PC_REG, pc);
    pc = machine->ReadRegister(NEXT_PC_REG);
    machine->WriteRegister(PC_REG, pc);
    pc += 4;
    machine->WriteRegister(NEXT_PC_REG, pc);
}

void newThread(void *name)
{
    currentThread->space->InitRegisters(); // Set the initial register values.
    currentThread->space->RestoreState();  // Load page table register.

    machine->Run(); // Jump to the user progam.
}

/// Do some default behavior for an unexpected exception.
///
/// NOTE: this function is meant specifically for unexpected exceptions.  If
/// you implement a new behavior for some exception, do not extend this
/// function: assign a new handler instead.
///
/// * `et` is the kind of exception.  The list of possible exceptions is in
///   `machine/exception_type.hh`.
static void
DefaultHandler(ExceptionType et)
{
    int exceptionArg = machine->ReadRegister(2);

    fprintf(stderr, "Unexpected user mode exception: %s, arg %d.\n",
            ExceptionTypeToString(et), exceptionArg);
    ASSERT(false);
}

/// Run a user program.
///
/// Open the executable, load it into memory, and jump to it.
void
StartProcess(const char *filename)
{
    ASSERT(filename != nullptr);

    OpenFile *executable = fileSystem->Open(filename);
    if (executable == nullptr) {
        printf("Unable to open file %s\n", filename);
        return;
    }

    AddressSpace *space = new AddressSpace(executable);
    currentThread->space = space;

    delete executable;

    space->InitRegisters();  // Set the initial register values.
    space->RestoreState();   // Load page table register.
    machine->Run();  // Jump to the user progam.
    ASSERT(false);   // `machine->Run` never returns; the address space
                     // exits by doing the system call `Exit`.
}

/// Handle a system call exception.
///
/// * `et` is the kind of exception.  The list of possible exceptions is in
///   `machine/exception_type.hh`.
///
/// The calling convention is the following:
///
/// * system call identifier in `r2`;
/// * 1st argument in `r4`;
/// * 2nd argument in `r5`;
/// * 3rd argument in `r6`;
/// * 4th argument in `r7`;
/// * the result of the system call, if any, must be put back into `r2`.
///
/// And do not forget to increment the program counter before returning. (Or
/// else you will loop making the same system call forever!)
static void
SyscallHandler(ExceptionType _et)
{
    int scid = machine->ReadRegister(2);

    switch (scid) {

        case SC_HALT:
            DEBUG('e', "Shutdown, initiated by user program.\n");
            interrupt->Halt();
            break;

        case SC_CREATE: {
            int filenameAddr = machine->ReadRegister(4);
            if (filenameAddr == 0) {
                machine->WriteRegister(2, -1);
                DEBUG('e', "Error: address to filename string is null.\n");
                break;
            }

            char filename[FILE_NAME_MAX_LEN + 1];
            if (!ReadStringFromUser(filenameAddr,
                                    filename, sizeof filename)) {
                machine->WriteRegister(2, -1);
                DEBUG('e', "Error: filename string too long (maximum is %u bytes).\n",
                      FILE_NAME_MAX_LEN);
                break;
            }
            DEBUG('e', "`Create` requested for file `%s`.\n", filename);

            if(!fileSystem->Create(filename, 1000)){
                machine->WriteRegister(2, -1);
                DEBUG('e', "Error: Failed to create the file: %s\n", filename);
                break;
            }

            machine->WriteRegister(2, 1);
            break;
        }

        case SC_EXEC: {

            DEBUG('e', "Request for Starting process\n");
            /////////////////////////////////// VER DE REEMPLAZAR CON UNA LLAMADA A OPEN
            int filenameAddr = machine->ReadRegister(4);
            if (filenameAddr == 0)
            {
                machine->WriteRegister(2, -1);
                DEBUG('e', "Error: address to filename string is null.\n");
                break;
            }

            char filename[FILE_NAME_MAX_LEN + 1];
            if (!ReadStringFromUser(filenameAddr,
                                    filename, sizeof filename))
            {
                machine->WriteRegister(2, -1);
                DEBUG('e', "NOMBRE %s.\n", filename);
                DEBUG('e', "Error: filename string too long (maximum is %u bytes).\n",
                      FILE_NAME_MAX_LEN);
                break;
            }
            DEBUG('e', "'Exec' Request for file `%s`.\n", filename);
            char *threadName = new char[FILE_NAME_MAX_LEN + 1];
            sprintf(threadName, "%s", filename);
            Thread *child = new Thread(threadName, true, 5);

            DEBUG('e', "`Open` requested for filename %s.\n", filename);

            OpenFile *file = fileSystem->Open(filename);
            ////////////////////////////

            AddressSpace *newSpace = new AddressSpace(file);
            if(!newSpace->IsInitialized()) {
                DEBUG('e', "Error al inicializar el address space\n");
                break;
            }
            child->space = newSpace;
            int i = 0;
            for ( ; i < 10 ; i++) {
                if(tableThread[i].space == NULL) {
                    tableThread[i].space = (SpaceId*)newSpace;
                    tableThread[i].thread = child;
                    break;
                }
            }

            child->Fork(newThread, nullptr);

            // DEBUG('e', "Success in Exec for %s\n", filename);

            delete file;

            machine->WriteRegister(2, i);
            //scheduler->Print();
            //REMINDER DE QUE SOMOS COMPLETAMENTE RETRASADOS
            //Y MERECEMOS LA MUERTE POR NO SABER COMO PORONGA FUNCIONA UN SWITCH
            //SDS
            break;
        }

        case SC_JOIN: {
            // VER TODAS LAS CONDICIONES DONDE PUEDE FALLAR
            // ESTO DEBERIA PONER UN HILO A CORRER PARA CHEQUEAR QUE EL QUE ESTOY ESPERANDO TERMINE????
            int index = machine->ReadRegister(4);
            DEBUG('e', "´Join´ called for space %p\n", tableThread[index].space);
            Thread *thread = tableThread[index].thread;
            // for (int i = 0; i < 10; i++)
            // {
            //     if(i == index) {
            //         thread = tableThread[i].thread;
            //         break;
            //     }
            // }
            DEBUG('e', "´Join Tread´ called for thread %s\n", thread->GetName());
            thread->Join();
            machine->WriteRegister(2, 1);
            break;
        }

        case SC_REMOVE: {
            int filenameAddr = machine->ReadRegister(4);
            if (filenameAddr == 0) {
                machine->WriteRegister(2, -1);
                DEBUG('e', "Error: address to filename string is null.\n");
                break;
            }

            char filename[FILE_NAME_MAX_LEN + 1];
            if (!ReadStringFromUser(filenameAddr,
                                    filename, sizeof filename)) {
                machine->WriteRegister(2, -1);
                DEBUG('e', "Error: filename string too long (maximum is %u bytes).\n",
                      FILE_NAME_MAX_LEN);
                break;
            }
            DEBUG('e', "`Remove` requested for file `%s`.\n", filename);

            if(!fileSystem->Remove(filename)){
                machine->WriteRegister(2, -1);
                DEBUG('e', "Error: Failed to remove the file: %s\n", filename);
                break;
            }

            machine->WriteRegister(2, 1);
            break;
        }

        case SC_EXIT: {
            //HALT if there is nothing left to run
            //Delete bitmap
            delete currentThread->space;
            currentThread->Finish(machine->ReadRegister(4));
            break;
        }

        case SC_OPEN: {
            int filenameAddr = machine->ReadRegister(4);
            if (filenameAddr == 0)
            {
                machine->WriteRegister(2, -1);
                DEBUG('e', "Error: address to filename string is null.\n");
                break;
            }

            char filename[FILE_NAME_MAX_LEN + 1];
            if (!ReadStringFromUser(filenameAddr,
                                    filename, sizeof filename))
            {
                machine->WriteRegister(2, -1);
                DEBUG('e', "Error: filename string too long (maximum is %u bytes).\n",
                      FILE_NAME_MAX_LEN);
                break;
            }
            DEBUG('e', "`Open` EXCEPTION requested for filename %s.\n", filename);

            OpenFile *file = fileSystem->Open(filename);
            OpenFileId fid = currentThread->AddOpenFile(file);

            if(fid == -1)
            {
                machine->WriteRegister(2, -1);
                DEBUG('e', "Error: no space left to open file.\n");
                break;
            }

            machine->WriteRegister(2, fid);
            break;
        }

        case SC_CLOSE: {
            OpenFileId fid = machine->ReadRegister(4);
            if (fid < 0)
            {
                DEBUG('e',"File id not valid\n");
                break;
            }
            DEBUG('e', "`Close` requested for id %u.\n", fid);
            bool success = currentThread->DeleteOpenFile(fid);
            if(!success)
            {
                DEBUG('e', "`Close` requested for id failed %u.\n", fid);
                machine->WriteRegister(2, -1);
                break;
            }
            machine->WriteRegister(2, 1);
            break;
        }

        case SC_READ: {
            int bufferDest = machine->ReadRegister(4);
            int size = machine->ReadRegister(5);
            int fid = machine->ReadRegister(6);

            if(size <= 0)
            {
                DEBUG('e', "Size read equal to 0\n");
                break;
            }

            DEBUG('e', "`Read` requested for id %u.\n", fid);

            char dest[size];
            WriteStringToUser(dest, bufferDest);
            if (fid == CONSOLE_INPUT) { ///CONSOLA
                synchConsole->Read(dest, size);
            }
            else { ///ARCHIVO
                OpenFile* file = currentThread->GetOpenFileByFileId(fid);
                if(!file)
                {
                    DEBUG('e', "Error: File is not open.\n.");
                    machine->WriteRegister(2, -1);
                    break;
                }

                file->Read(dest, (unsigned)size);
                WriteBufferToUser(dest, bufferDest, (unsigned)size);
            }
            machine->WriteRegister(2, 1);
            break;
        }

        case SC_WRITE:
        {
            int bufferSource = machine->ReadRegister(4);
            int size = machine->ReadRegister(5);
            OpenFileId fid = machine->ReadRegister(6);
            if(size <= 0)
            {
                DEBUG('e', "Size read equal to 0\n");
                break;
            }
            
            DEBUG('e', "`Write` requested for id %u.\n", fid);

            char out[size];
            ReadBufferFromUser(bufferSource, out, size);
            if (fid == CONSOLE_OUTPUT ) {
                DEBUG('e', "Console output\n");
                synchConsole->Write(out, size);
            }
            else {
                OpenFile* file = currentThread->GetOpenFileByFileId(fid);
                if(!file)
                {
                    DEBUG('e', "Error: File is not open.\n.");
                    machine->WriteRegister(2, -1);
                    break;
                }

                file->Write(out, (unsigned)size);
                //delete file; // PROBAR
            }
            machine->WriteRegister(2, 1);
            break;
        }

        case SC_STATS:
        {
            DEBUG('e', "Scheduler stats requested.\n");
            scheduler->Print();
            break;
        }

        default:
            fprintf(stderr, "Unexpected system call: id %d.\n", scid);
            ASSERT(false);

    }

    IncrementPC();
}


/// By default, only system calls have their own handler.  All other
/// exception types are assigned the default handler.
void
SetExceptionHandlers()
{
    machine->SetHandler(NO_EXCEPTION,            &DefaultHandler);
    machine->SetHandler(SYSCALL_EXCEPTION,       &SyscallHandler);
    machine->SetHandler(PAGE_FAULT_EXCEPTION,    &DefaultHandler);
    machine->SetHandler(READ_ONLY_EXCEPTION,     &DefaultHandler);
    machine->SetHandler(BUS_ERROR_EXCEPTION,     &DefaultHandler);
    machine->SetHandler(ADDRESS_ERROR_EXCEPTION, &DefaultHandler);
    machine->SetHandler(OVERFLOW_EXCEPTION,      &DefaultHandler);
    machine->SetHandler(ILLEGAL_INSTR_EXCEPTION, &DefaultHandler);
}
