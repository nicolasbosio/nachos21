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
#include "args.hh"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

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

void newThread(void *arg)
{
    char** argv = (char**) arg;
    currentThread->space->InitRegisters(); // Set the initial register values.
    currentThread->space->RestoreState(nullptr);  // Load page table register.
    int sp = machine->ReadRegister(STACK_REG);
    if(arg != nullptr) {
        sp = machine->ReadRegister(STACK_REG);
        unsigned int count = 0;
        if(argv != nullptr) {
            count = WriteArgs(argv);
        }
        sp = machine->ReadRegister(STACK_REG);
        machine->WriteRegister(4, count); //argc
        machine->WriteRegister(5, sp); //argv
        machine->WriteRegister(STACK_REG, sp - 16);
    }
    machine->Run(); // Jump to the user progam.
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

#ifdef SWAP
    char fileSwap[FILE_NAME_MAX_LEN];
    sprintf(fileSwap, "SWAP.%d", 0);
    DEBUG('e', "`Create` requested for SWAP File `%s`.\n", fileSwap);
    if (fileSystem->Create(fileSwap, space->GetSize()))
    {
        tableThread->Add(currentThread);
        currentThread->SetPid(0);
        DEBUG('e', "´Create´ SWAP File for spaceId: %p on thread: %s with name :%s ans size : %d\n",
              space, currentThread->GetName(), fileSwap, space->GetSize());
    }
#endif

#ifndef DEMAND_LOADING
    delete executable;
#endif
    space->InitRegisters();  // Set the initial register values.
    space->RestoreState(nullptr);   // Load page table register.
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

        case SC_HALT: {
            DEBUG('e', "Shutdown, initiated by user program.\n");
#ifdef SWAP
            for (int i = tableThread->GetCurrent() ; i >= 0 ; i--)
            { 
                char fileSwap[FILE_NAME_MAX_LEN];
                sprintf(fileSwap, "SWAP.%d", i);
                fileSystem->Remove(fileSwap);
            }
#endif
            interrupt->Halt();
            break;
        }

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
            if(!fileSystem->Create(filename, 100)){
                machine->WriteRegister(2, -1);
                DEBUG('e', "Error: Failed to create the file: %s\n", filename);
                break;
            }

            machine->WriteRegister(2, 1);
            break;
        }

        case SC_EXEC: {
            DEBUG('e', "Request for starting process\n");
            int filenameAddr = machine->ReadRegister(4);
            int joinable = machine->ReadRegister(5);
            int argvAddr = machine->ReadRegister(6);
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
            
            DEBUG('e', "'EXEC' Request for file `%s`.\n", filename);
            char *threadName = new char[FILE_NAME_MAX_LEN + 1];
            sprintf(threadName, "%s", filename);
            Thread *child = new Thread(threadName, joinable, 2);
            int pid = tableThread->Add(child);
            if (pid == -1) {
                delete child;
                delete threadName;
                DEBUG('e', "Error: max thread count reached... :( \n");
                interrupt->Halt();
            }
            child->SetPid((unsigned int)pid);

            DEBUG('e', "`Open` requested for filename %s.\n", filename);

            OpenFile *file = fileSystem->Open(filename);

            if(file == nullptr)
            {
                machine->WriteRegister(2, -1);
                DEBUG('e', "File not found\n");
                break;
            }

            AddressSpace *newSpace = new AddressSpace(file);
            if(!newSpace->IsInitialized())
            {
                machine->WriteRegister(2, -1);
                DEBUG('e', "Error al inicializar el address space\n");
                break;
            }
            child->space = newSpace;

            char **args = nullptr;
            if (argvAddr != 0)
            {
                args = SaveArgs(argvAddr);
            }
            child->Fork(newThread, args);

#ifdef SWAP
            char fileSwap[FILE_NAME_MAX_LEN];
            sprintf(fileSwap, "SWAP.%d", child->GetPid());
            if (fileSystem->Create(fileSwap, newSpace->GetSize()))
            {
                DEBUG('e', "´Create´ SWAP File for thread: %s with name :%s\n",
                        child->GetName(), fileSwap);
            }
#endif
            DEBUG('e', "Success in Exec for %s\n", filename);

#ifndef DEMAND_LOADING
            delete file;
#endif
            machine->WriteRegister(2, child->GetPid());
            break;
        }

        case SC_JOIN: {
            int pid = machine->ReadRegister(4);
            Thread *thread = tableThread->Get(pid);
            
            if(thread->IsJoinable()) 
            {
                DEBUG('e', "´Join Thread´ called for thread %s\n", thread->GetName());
                int ret = thread->Join();
                machine->WriteRegister(2, ret);
            }
            else
            {
                DEBUG('e', "´Join Thread´ called for thread no joineable, thread: %s\n", thread->GetName());
                machine->WriteRegister(2, -1);
            }
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

            if (file == nullptr)
            {
                machine->WriteRegister(2, -1);
                DEBUG('e', "File not found\n");
                break;
            }

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
                DEBUG('e', "`Close` requested for id %u failed.\n", fid);
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
            int bytesRead = 0;
            if (fid == CONSOLE_INPUT) {
                synchConsole->Read(dest, size);
                WriteStringToUser(dest, bufferDest);
                bytesRead = size;
            }
            else {
                OpenFile* file = currentThread->GetOpenFileByFileId(fid);
                if(!file)
                {
                    DEBUG('e', "Error: File is not open.\n.");
                    machine->WriteRegister(2, -1);
                    break;
                }

                bytesRead = file->Read(dest, (unsigned)size);
                if(bytesRead > 0)
                {
                    WriteBufferToUser(dest, bufferDest, bytesRead);
                }
            }
            machine->WriteRegister(2, bytesRead);
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

            char out[size + 1];
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

                out[size] = '\0';

                file->Write(out, (unsigned)size);
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

static void
PageFaultHandler(ExceptionType _et) 
{
    unsigned badVAddr = machine->ReadRegister(BAD_VADDR_REG);
    unsigned int numPage = badVAddr / PAGE_SIZE;
    DEBUG('e', "'Page Fault Handler' vAddr: %d vPage: %d Current thread: %s\n"
            ,badVAddr, numPage, currentThread->GetName());
    TranslationEntry *pageTranslation = currentThread->space->GetTranslationEntry(numPage);

#ifdef DEMAND_LOADING
    if(!pageTranslation->valid) {
        DEBUG('e', "'Demand loading required' vAddr: %d vPage: %d\n", badVAddr, numPage);
        pageTranslation = currentThread->space->LoadPage(badVAddr);
    }
    else {
        // DEBUG('e', "Translation valid found for page %d in pageTable\n",numPage);
    }
#endif

#ifdef SWAP
    if (pageTranslation->virtualPage >= currentThread->space->GetNumPages() && pageTranslation->valid)
    {   
        DEBUG('e', "Page in SWAP!\n");
        bool valid = currentThread->space->LoadPageFromSwap(pageTranslation);
        ASSERT(valid);
    }
#endif
    
    currentThread->space->SetTlbPage(pageTranslation);

    //currentThread->space->PrintCoreMap(); //BORRAR
    //currentThread->space->PrintPageTable(); //BORRAR
    // MMU *mem = machine->GetMMU(); //BORRAR
    // mem->PrintTLB(); //BORRAR
}

static void
ReadOnlyExceptionHandler(ExceptionType _et)
{
    unsigned badVAddr = machine->ReadRegister(BAD_VADDR_REG);
    unsigned int numPage = badVAddr / PAGE_SIZE;
    DEBUG('e', "'Page 'RO' exception' Virtual address: %d -- Page: %d\n", badVAddr, numPage);
    /// TODO: que hacer???
    // 
    //
    ASSERT(false);
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


/// By default, only system calls have their own handler.  All other
/// exception types are assigned the default handler.
void
SetExceptionHandlers()
{
    machine->SetHandler(NO_EXCEPTION,            &DefaultHandler);
    machine->SetHandler(SYSCALL_EXCEPTION,       &SyscallHandler);
    machine->SetHandler(PAGE_FAULT_EXCEPTION,    &PageFaultHandler);
    machine->SetHandler(READ_ONLY_EXCEPTION,     &ReadOnlyExceptionHandler);
    machine->SetHandler(BUS_ERROR_EXCEPTION,     &DefaultHandler);
    machine->SetHandler(ADDRESS_ERROR_EXCEPTION, &DefaultHandler);
    machine->SetHandler(OVERFLOW_EXCEPTION,      &DefaultHandler);
    machine->SetHandler(ILLEGAL_INSTR_EXCEPTION, &DefaultHandler);
}
