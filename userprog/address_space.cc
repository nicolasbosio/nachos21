/// Routines to manage address spaces (memory for executing user programs).
///
/// Copyright (c) 1992-1993 The Regents of the University of California.
///               2016-2021 Docentes de la Universidad Nacional de Rosario.
/// All rights reserved.  See `copyright.h` for copyright notice and
/// limitation of liability and disclaimer of warranty provisions.


#include "address_space.hh"
#include "executable.hh"
#include "threads/system.hh"

#include <string.h>


/// First, set up the translation from program memory to physical memory.
/// For now, this is really simple (1:1), since we are only uniprogramming,
/// and we have a single unsegmented page table.
AddressSpace::AddressSpace(OpenFile *executable_file)
{
    initialized = false;
    /// NO HACER ASSERT ESTO ROMPE EN LLAMADA A SISTEMA
    if(executable_file == nullptr) {
        DEBUG('a', "No executable to run\n"); //COMPLETAR
        //BREAK
        return;
    }

    Executable exe (executable_file);
    if (!exe.CheckMagic()) {
        DEBUG('a', "2\n"); //COMPLETAR
        //BREAK
        return;
    }

    // How big is address space?

    unsigned size = exe.GetSize() + USER_STACK_SIZE;
      // We need to increase the size to leave room for the stack.
    numPages = DivRoundUp(size, PAGE_SIZE);
    DEBUG('a', "Cantidad de paginas asignadas al proceso %d\n", numPages); //TRADUCIR
    size = numPages * PAGE_SIZE;

    // Check we are not trying to run anything too big -- at least until we
    // have virtual memory.
    if(numPages > bitmap->CountClear()) {
        DEBUG('a', "La cantidad de paginas requeridas no alcanza\n"); //TRADUCIR
        //BREAK
        return;
    }

    DEBUG('a', "Initializing address space, num pages %u, size %u\n",
          numPages, size);

    // First, set up the translation.

    pageTable = new TranslationEntry[numPages];
    for (unsigned i = 0; i < numPages; i++) {
        pageTable[i].virtualPage  = i;
        pageTable[i].physicalPage = bitmap->Find();
        pageTable[i].valid        = true;
        pageTable[i].use          = false;
        pageTable[i].dirty        = false;
        pageTable[i].readOnly     = false;
          // If the code segment was entirely on a separate page, we could
          // set its pages to be read-only.
    }

    char *mainMemory = machine->GetMMU()->mainMemory;

    // Zero out the entire address space, to zero the unitialized data
    // segment and the stack segment.
    //memset(mainMemory, 0, size); //BORRAR
    for(unsigned i = 0; i < numPages ; i++)
        memset(&mainMemory[pageTable[i].physicalPage * PAGE_SIZE], 0, PAGE_SIZE);


    // Then, copy in the code and data segments into memory.
    uint32_t codeSize = exe.GetCodeSize();
    uint32_t initDataSize = exe.GetInitDataSize();
    if(codeSize > 0)
    {
        uint32_t virtualAddr = exe.GetCodeAddr();
        DEBUG('a', "Initializing code segment, at 0x%X, size %u\n", virtualAddr, codeSize);
        for (uint32_t bytesRead = 0; bytesRead < codeSize; bytesRead += PAGE_SIZE)
        {
            uint32_t page = (virtualAddr + bytesRead) / PAGE_SIZE;
            uint32_t offset = (virtualAddr + bytesRead) % PAGE_SIZE;
            uint32_t readSize = min(codeSize - bytesRead, PAGE_SIZE);
            // pageTable[page].readOnly = true;
            exe.ReadCodeBlock(&mainMemory[(pageTable[page].physicalPage * PAGE_SIZE) + offset], readSize, bytesRead);
        }
    }

    if(initDataSize > 0)
    {
        uint32_t virtualAddr = exe.GetInitDataAddr();
        DEBUG('a', "Initializing data segment, at 0x%X, size %u\n", virtualAddr, initDataSize);
        //Complete the unfilled part of the last page of code with initData.
        uint32_t page = DivRoundDown(codeSize, PAGE_SIZE);
        uint32_t offset = codeSize % PAGE_SIZE;
        exe.ReadDataBlock(&mainMemory[(pageTable[page].physicalPage * PAGE_SIZE) + offset], PAGE_SIZE - offset, 0);
        page++;

        for (uint32_t bytesRead = PAGE_SIZE - offset; bytesRead < initDataSize; bytesRead += PAGE_SIZE)
        {
            page = (virtualAddr + bytesRead) / PAGE_SIZE;
            offset = (virtualAddr + bytesRead) % PAGE_SIZE;
            uint32_t readSize = min(initDataSize - bytesRead, PAGE_SIZE);
            exe.ReadDataBlock(&mainMemory[(pageTable[page].physicalPage * PAGE_SIZE) + offset], readSize, bytesRead);
        }
    }
    
    /*
    uint32_t codeSize = exe.GetCodeSize();
    uint32_t initDataSize = exe.GetInitDataSize();
    if (codeSize > 0)
    {
        uint32_t virtualAddr = exe.GetCodeAddr();
        uint32_t size;
        for (uint32_t i = 0; i < codeSize; i += size)
        {
            int page = (virtualAddr + i) / PAGE_SIZE;
            int offset = (virtualAddr + i) % PAGE_SIZE;
            size = (codeSize - i) < PAGE_SIZE ? (codeSize - i) : PAGE_SIZE;
            uint32_t physicalAddr = pageTable[page].physicalPage * PAGE_SIZE + offset;
            exe.ReadCodeBlock(&mainMemory[physicalAddr], size, i);
        }
        // DEBUG('a', "Initializing code segment, at 0x%X, size %u\n", virtualAddr, codeSize);
    }
    if (initDataSize > 0)
    {
        uint32_t virtualAddr = exe.GetInitDataAddr();
        uint32_t size;
        for (uint32_t i = 0; i < initDataSize; i += size)
        {
            int page = (virtualAddr + i) / PAGE_SIZE;
            int offset = (virtualAddr + i) % PAGE_SIZE;
            size = (codeSize - i) < PAGE_SIZE ? (codeSize - i) : PAGE_SIZE;
            uint32_t physicalAddr = pageTable[page].physicalPage * PAGE_SIZE + offset;
            exe.ReadDataBlock(&mainMemory[physicalAddr], size, i);
        }
        //DEBUG('a', "Initializing data segment, at 0x%X, size %u\n", virtualAddr, initDataSize);
    }
    */
    initialized = true;
    DEBUG('a', "Inicializacion de address space correcta\n"); //TRADUCIR
}

/// Deallocate an address space.
///
/// Nothing for now!
AddressSpace::~AddressSpace()
{
    for(unsigned i = 0 ; i < numPages ; i++)
        bitmap->Clear(i);
    delete [] pageTable;
}

/// Set the initial values for the user-level register set.
///
/// We write these directly into the “machine” registers, so that we can
/// immediately jump to user code.  Note that these will be saved/restored
/// into the `currentThread->userRegisters` when this thread is context
/// switched out.
void
AddressSpace::InitRegisters()
{
    for (unsigned i = 0; i < NUM_TOTAL_REGS; i++) {
        machine->WriteRegister(i, 0);
    }

    // Initial program counter -- must be location of `Start`.
    machine->WriteRegister(PC_REG, 0);

    // Need to also tell MIPS where next instruction is, because of branch
    // delay possibility.
    machine->WriteRegister(NEXT_PC_REG, 4);

    // Set the stack register to the end of the address space, where we
    // allocated the stack; but subtract off a bit, to make sure we do not
    // accidentally reference off the end!
    machine->WriteRegister(STACK_REG, numPages * PAGE_SIZE - 16);
    DEBUG('a', "Initializing stack register to %u\n",
          numPages * PAGE_SIZE - 16);
}

/// On a context switch, save any machine state, specific to this address
/// space, that needs saving.
///
/// For now, nothing!
void
AddressSpace::SaveState()
{}

/// On a context switch, restore the machine state so that this address space
/// can run.
///
/// For now, tell the machine where to find the page table.
void
AddressSpace::RestoreState()
{
    machine->GetMMU()->pageTable     = pageTable;
    machine->GetMMU()->pageTableSize = numPages;
}

bool
AddressSpace::IsInitialized()
{
    return initialized;
}