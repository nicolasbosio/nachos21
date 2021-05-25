/// Routines to manage address spaces (memory for executing user programs).
///
/// Copyright (c) 1992-1993 The Regents of the University of California.
///               2016-2021 Docentes de la Universidad Nacional de Rosario.
/// All rights reserved.  See `copyright.h` for copyright notice and
/// limitation of liability and disclaimer of warranty provisions.


#include "address_space.hh"
#include "threads/system.hh"
#include "executable.hh"
#include <string.h>

/// First, set up the translation from program memory to physical memory.
/// For now, this is really simple (1:1), since we are only uniprogramming,
/// and we have a single unsegmented page table.
AddressSpace::AddressSpace(OpenFile *executable_file)
{
    initialized = false;
    if(executable_file == nullptr) {
        DEBUG('p', "No executable to run\n");
        return;
    }

    Executable exe (executable_file);
    if (!exe.CheckMagic()) {
        DEBUG('p', "Not Nachos executable file\n");
        return;
    }

    // How big is address space?
    unsigned size = exe.GetSize() + USER_STACK_SIZE;
    // We need to increase the size to leave room for the stack.
    DEBUG('p', "Cantidad de paginas libres %d\n", bitmap->CountClear());
    numPages = DivRoundUp(size, PAGE_SIZE);
    DEBUG('p', "Cantidad de paginas requeridas por el proceso %d\n", numPages);
    
    size = numPages * PAGE_SIZE;

    // Check we are not trying to run anything too big -- at least until we
    // have virtual memory.
    if(numPages > bitmap->CountClear()) {
        DEBUG('p', "La cantidad de paginas requeridas no alcanza\n");
        return;
    }

    DEBUG('p', "Initializing address space, num pages %u, size %u\n",
          numPages, size);

    // First, set up the translation.
    pageTable = new TranslationEntry[numPages];
    for (unsigned i = 0; i < numPages; i++) {
        pageTable[i].virtualPage  = i;
#ifdef DEMAND_LOADING
        pageTable[i].physicalPage = 0;
        pageTable[i].valid = false;
#else
        pageTable[i].physicalPage = bitmap->Find();
        pageTable[i].valid        = true;
#endif
        pageTable[i].use          = false;
        pageTable[i].dirty        = false;
        pageTable[i].readOnly     = false;
          // If the code segment was entirely on a separate page, we could
          // set its pages to be read-only.
    }

#ifdef DEMAND_LOADING
    codeSize = exe.GetCodeSize();
    initDataSize = exe.GetInitDataSize();
    executableFile = executable_file;
    DEBUG('p', "Initializing Demand Loading Address space for thread: %s\n", currentThread->GetName());
    DEBUG('p', "Inicializacion de demand loading correcta\n");
#else
    char *mainMemory = machine->GetMMU()->mainMemory;

    // Zero out the entire address space, to zero the unitialized data
    // segment and the stack segment.
    for(unsigned i = 0; i < numPages ; i++)
        memset(&mainMemory[pageTable[i].physicalPage * PAGE_SIZE], 0, PAGE_SIZE);

    // Then, copy in the code and data segments into memory.
    uint32_t codeSize = exe.GetCodeSize();
    uint32_t initDataSize = exe.GetInitDataSize();

    if(codeSize > 0)
    {
        uint32_t virtualAddr = exe.GetCodeAddr();
        DEBUG('p', "Initializing code segment, at 0x%X, size %u\n", virtualAddr, codeSize);
        for (uint32_t bytesRead = 0; bytesRead < codeSize; bytesRead += PAGE_SIZE)
        {
            uint32_t page = (virtualAddr + bytesRead) / PAGE_SIZE;
            uint32_t offset = (virtualAddr + bytesRead) % PAGE_SIZE;
            uint32_t readSize = min(codeSize - bytesRead, PAGE_SIZE);
            pageTable[page].readOnly = true;
            exe.ReadCodeBlock(&mainMemory[(pageTable[page].physicalPage * PAGE_SIZE) + offset], readSize, bytesRead);
        }
    }

    if(initDataSize > 0)
    {
        uint32_t virtualAddr = exe.GetInitDataAddr();
        DEBUG('p', "Initializing data segment, at 0x%X, size %u\n", virtualAddr, initDataSize);
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
    DEBUG('p', "Inicializacion de address space correcta\n");
#endif
    initialized = true;
}

/// Deallocate an address space.
///
/// Nothing for now!
AddressSpace::~AddressSpace()
{
    for(unsigned i = 0 ; i < numPages ; i++) {
        if(pageTable[i].valid) {
            DEBUG('p', "Limpiando el marco fisico n° %d -- Valid? %d\n", pageTable[i].physicalPage, pageTable[i].valid);
            bitmap->Clear(pageTable[i].physicalPage);
        }
    }
    delete [] pageTable;
#ifdef DEMAND_LOADING
    delete executableFile;
#endif
}


#ifdef DEMAND_LOADING
///
TranslationEntry
AddressSpace::LoadPage(unsigned vAddr)
{
    Executable exe (executableFile);
    unsigned pageNum = vAddr / PAGE_SIZE;
    vAddr = pageNum * PAGE_SIZE;
    DEBUG('p', "'Load Page Required' -> vPage: %d -- vAddr: %d\n",
          pageNum, vAddr);
    bool init = false;
    char *mainMemory = machine->GetMMU()->mainMemory;
    int physPage = bitmap->Find();

    ASSERT(physPage != -1);
    
    pageTable[pageNum].virtualPage = pageNum;
    pageTable[pageNum].physicalPage = physPage;
    pageTable[pageNum].valid = true;
    
    if (vAddr >= (codeSize + initDataSize))
    {
        DEBUG('p', "'Load Stack block' -> vPage: %d -- phyPage %d\n", 
            pageTable[pageNum].virtualPage, pageTable[pageNum].physicalPage);
        memset(&mainMemory[pageTable[pageNum].physicalPage * PAGE_SIZE], 0, PAGE_SIZE);
        //memset(&mainMemory[pageTable[i].physicalPage * PAGE_SIZE], 0, PAGE_SIZE);
        init = true;
    }
    else if (vAddr < codeSize)
    {
        uint32_t readSize = min(codeSize - vAddr, PAGE_SIZE);
        uint32_t offset = pageNum * PAGE_SIZE;

        DEBUG('p', "'Load Code block' -> vPage: %d -- phyPage %d -- readSize %d -- vAddr %d\n", 
            pageTable[pageNum].virtualPage, pageTable[pageNum].physicalPage, readSize, vAddr);

        pageTable[pageNum].readOnly = true;
        exe.ReadCodeBlock(&mainMemory[pageTable[pageNum].physicalPage * PAGE_SIZE], PAGE_SIZE, offset);
        //exe.ReadCodeBlock(&mainMemory[(pageTable[page].physicalPage * PAGE_SIZE) + offset], readSize, bytesRead);
        init = true;
    }
    else
    {
        uint32_t readSize = min(initDataSize - (vAddr - codeSize), PAGE_SIZE);
        uint32_t offset = pageNum * PAGE_SIZE;

        DEBUG('p', "'Load initData block' -> vPage: %d -- phyPage %d -- readSize %d -- vAddr %d\n",
              pageTable[pageNum].virtualPage, pageTable[pageNum].physicalPage, PAGE_SIZE, vAddr);

        exe.ReadDataBlock(&mainMemory[pageTable[pageNum].physicalPage * PAGE_SIZE], readSize, (offset - codeSize));
        //exe.ReadDataBlock(&mainMemory[(pageTable[page].physicalPage * PAGE_SIZE) + offset], readSize, bytesRead);
        init = true;
    }
    ASSERT(init);

    return pageTable[pageNum];
}
#endif

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
    DEBUG('p', "Initializing stack register to %u\n",
          numPages * PAGE_SIZE - 16);
}

///
///
///
void
AddressSpace::InvalidateTlb()
{
    MMU *mmu = machine->GetMMU();
    for (unsigned i = 0; i < TLB_SIZE; i++) 
        mmu->tlb[i].valid = false;
}

///
///
///
TranslationEntry 
AddressSpace::GetTranslationEntry(unsigned vPage)
{
    return pageTable[vPage];
}

/// On a context switch, save any machine state, specific to this address
/// space, that needs saving.
///
/// For now, nothing!
void
AddressSpace::SaveState()
{
}

/// On a context switch, restore the machine state so that this address space
/// can run.
///
/// For now, tell the machine where to find the page table.
void
AddressSpace::RestoreState()
{
#ifdef USE_TLB
    InvalidateTlb();
#else
    machine->GetMMU()->pageTable = pageTable;
    machine->GetMMU()->pageTableSize = numPages;
#endif
}

bool
AddressSpace::IsInitialized()
{
    return initialized;
}