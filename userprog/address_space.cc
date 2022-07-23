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
#include <stdio.h>

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
    
    //
    unsigned freePages;

#if USE_TLB
    ///
    tlbIndex = 0;
#endif
    // How big is address space?
    unsigned size = exe.GetSize() + USER_STACK_SIZE;
    // We need to increase the size to leave room for the stack.

#ifndef SWAP
    freePages = memoryMap->CountClear();
#else
    freePages = memoryCoreMap->CountClear();
#endif

    DEBUG('p', "Cantidad de paginas libres %d\n", freePages);
    numPages = DivRoundUp(size, PAGE_SIZE);
    DEBUG('p', "Cantidad de paginas requeridas por el proceso %d\n", numPages);
    
    size = numPages * PAGE_SIZE;

    // Check we are not trying to run anything too big -- at least until we
    // have virtual memory.
    if (numPages > freePages)
    {
#ifndef SWAP
        DEBUG('p', "La cantidad de paginas requeridas no alcanza!\n");
        return;
#endif
    }

    DEBUG('p', "Initializing address space, num pages %u, size %u\n",
          numPages, size);

    // First, set up the translation.
    pageTable = new TranslationEntry[numPages];
    for (unsigned i = 0; i < numPages; i++) {
        pageTable[i].virtualPage  = i;
        // TODO: CREO esta condicion esta mal si DEMAND no esta pero si SWAP en memset linea 100 estamos pisando memoria
#if defined(DEMAND_LOADING) || defined(SWAP)
        pageTable[i].physicalPage = 0;
        pageTable[i].valid = false;
#else
        pageTable[i].physicalPage = memoryMap->Find();
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
    DEBUG('p', "Inicializacion de demand loading correcta\n");
#else
    char *mainMemory = machine->GetMMU()->mainMemory;

    // Zero out the entire address space, to zero the uninitialized data
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
        if(pageTable[i].valid && pageTable[i].virtualPage < numPages) {
#ifndef SWAP
            memoryMap->Clear(pageTable[i].physicalPage);
#else
            memoryCoreMap->Clear(pageTable[i].physicalPage);
#endif
        }
    }
    delete [] pageTable;
#ifdef DEMAND_LOADING
    delete executableFile;
#endif
}

#ifdef DEMAND_LOADING
///COMENTAR
TranslationEntry *
AddressSpace::LoadPage(unsigned vAddr)
{
    Executable exe (executableFile);
    unsigned pageNum = vAddr / PAGE_SIZE;
    vAddr = pageNum * PAGE_SIZE;
    DEBUG('p', "'Load Page Required' -> vPage: %d -- vAddr: %d\n",
          pageNum, vAddr);
    char *mainMemory = machine->GetMMU()->mainMemory;

    memoryCoreMap->GetLock()->Acquire();
#ifndef SWAP
    int physPage = memoryMap->Find();
#else
    int physPage = memoryCoreMap->Add(this, pageNum, currentThread->GetPid());
    memoryCoreMap->GetLock()->Release();
    if(physPage == -1) {
        DEBUG('p', "No space in memory to load page from binary file\n"); //BORRAR
        int victim = WritePagetoSwap();
        memoryCoreMap->GetLock()->Acquire();
        physPage = memoryCoreMap->AddVictim(this, pageNum, currentThread->GetPid(), victim);
        memoryCoreMap->GetLock()->Release();
        ASSERT(victim == physPage);
    }
#endif
   
    
    ASSERT(physPage != -1);
    pageTable[pageNum].virtualPage = pageNum;
    pageTable[pageNum].physicalPage = physPage;
    pageTable[pageNum].valid = true;
    //memoryCoreMap->ClearItemTlb(physPage); // Ojo con esto ya se hace en WritePagetoSwap
    
    if (vAddr >= (codeSize + initDataSize))
    {
        DEBUG('p', "'Load Stack block' -> vPage: %d -- phyPage %d\n", 
            pageTable[pageNum].virtualPage, pageTable[pageNum].physicalPage);
        memset(&mainMemory[pageTable[pageNum].physicalPage * PAGE_SIZE], 0, PAGE_SIZE);
    }
    else if (vAddr < codeSize)
    {
        uint32_t readSize = min(codeSize - vAddr, PAGE_SIZE);
        uint32_t offset = pageNum * PAGE_SIZE;

        DEBUG('p', "'Load Code block' -> vPage: %d -- phyPage %d -- readSize %d -- vAddr %d\n", 
            pageTable[pageNum].virtualPage, pageTable[pageNum].physicalPage, readSize, vAddr);

        pageTable[pageNum].readOnly = true;
        exe.ReadCodeBlock(&mainMemory[pageTable[pageNum].physicalPage * PAGE_SIZE], PAGE_SIZE, offset);
    }
    else
    {
        uint32_t readSize = min(initDataSize - (vAddr - codeSize), PAGE_SIZE);
        uint32_t offset = pageNum * PAGE_SIZE;

        DEBUG('p', "'Load initData block' -> vPage: %d -- phyPage %d -- readSize %d -- vAddr %d\n",
              pageTable[pageNum].virtualPage, pageTable[pageNum].physicalPage, PAGE_SIZE, vAddr);

        exe.ReadDataBlock(&mainMemory[pageTable[pageNum].physicalPage * PAGE_SIZE], readSize, (offset - codeSize));
    }
#ifdef SWAP
    memoryCoreMap->unlockPage(physPage);
#endif
    return &pageTable[pageNum];
}
#endif




#ifdef SWAP
/// Devuelve un entero que indica la pagina fisica respecto al comienzo de memoria que se libero
int
AddressSpace::WritePagetoSwap()
{
    MMU *mmu = machine->GetMMU();
    char *mainMemory = mmu->mainMemory;
    memoryCoreMap->GetLock()->Acquire();
    unsigned victimIndex = memoryCoreMap->PickVictim();
    memoryCoreMap->GetLock()->Release();
    CoreItem *victimData = memoryCoreMap->GetItem(victimIndex);
    unsigned int virtualPage = victimData->virtualPage;

    DEBUG('x',"'SWAP-Write Required' VictimData index: %d - VictimPid: %d - IsDirty: %d\n", 
        victimIndex, victimData->pid, victimData->spaceId->pageTable[virtualPage].dirty); //BORRAR

    bool isDirty = victimData->pid == (int)currentThread->GetPid() && victimData->inTlb != -1
        ? mmu->tlb[victimData->inTlb].dirty
        : victimData->spaceId->pageTable[virtualPage].dirty;
    
    if(isDirty) {
        char swapFileName[FILENAME_MAX];
        sprintf(swapFileName, "SWAP.%d", victimData->pid);
        OpenFile *swapFile = fileSystem->Open(swapFileName);
        int writeSize = swapFile->WriteAt(&mainMemory[victimData->physicalPage * PAGE_SIZE], PAGE_SIZE, virtualPage * PAGE_SIZE);
        if(writeSize != PAGE_SIZE) return -1;
        DEBUG('x', "Write page to swap: phyPage -> %d -- swapFile -> %s -- virtPage %d\n"
                                    ,victimIndex, swapFileName, virtualPage);
        victimData->spaceId->pageTable[virtualPage].virtualPage += victimData->spaceId->GetNumPages();
        victimData->spaceId->pageTable[virtualPage].physicalPage = -1;
        delete swapFile;
    }
    else
    {
        DEBUG('x', "No SWAP-Write needed, page in binary file\n"); //BORRAR
        victimData->spaceId->pageTable[virtualPage].valid = false;
    }    
    
#ifdef USE_TLB
    int tlbVictimIndex = victimData->inTlb;
    if(tlbVictimIndex != -1)
    {
        victimData->spaceId->pageTable[virtualPage].dirty = mmu->tlb[tlbVictimIndex].dirty;
        victimData->spaceId->pageTable[virtualPage].use   = mmu->tlb[tlbVictimIndex].use;
        mmu->tlb[tlbVictimIndex].valid = false;
    }
#endif
    
    // memoryCoreMap->Clear(victimIndex);
    return victimIndex;
}
 
bool
AddressSpace::LoadPageFromSwap(TranslationEntry *pageTranslation)
{
    unsigned int pid = currentThread->GetPid();
    char swapFile[FILENAME_MAX];
    sprintf(swapFile, "SWAP.%d", pid);
    DEBUG('x', "Requested Load Page from SWAP: %s | Virtual page: %d | threadName: %s\n",
          swapFile, (pageTranslation->virtualPage - numPages), currentThread->GetName());
    char *mainMemory = machine->GetMMU()->mainMemory;
    OpenFile *swap = fileSystem->Open(swapFile);
    if(swap == nullptr) return false;

    memoryCoreMap->GetLock()->Acquire();
    int physPage = memoryCoreMap->Add(currentThread->space, (pageTranslation->virtualPage - numPages), pid);
    memoryCoreMap->GetLock()->Release();

    if(physPage == -1) {
        DEBUG('x', "No space in memory to load page from SWAP\n"); //BORRAR
        int victim = WritePagetoSwap();
        memoryCoreMap->GetLock()->Acquire();
        physPage = memoryCoreMap->AddVictim(currentThread->space, (pageTranslation->virtualPage - numPages), pid, victim);
        memoryCoreMap->GetLock()->Release();
        ASSERT(victim == physPage);
    }
    
    DEBUG('x', "LOADING Page from SWAP - physical: %d | virtual: %d | threadName: %s\n",
          physPage, (pageTranslation->virtualPage - numPages), currentThread->GetName());
    int sizeRead = swap->ReadAt(&mainMemory[physPage * PAGE_SIZE], PAGE_SIZE, (pageTranslation->virtualPage - numPages) * PAGE_SIZE);
    if(sizeRead != PAGE_SIZE) return false;
    pageTranslation->physicalPage = physPage;
    delete swap;
    pageTranslation->virtualPage -= numPages;
    memoryCoreMap->unlockPage(physPage);
    return true;
}

/// COMENTAR
void
AddressSpace::PrintPageTable() 
{
    printf("\nPAGE TABLE (%s) content (%d entries)\n",currentThread->GetName(), numPages);
    TranslationEntry entry;
    for(unsigned page = 0 ; page < numPages ; page++) {
        entry = pageTable[page];
        bool inswap = entry.virtualPage >= numPages;
        CoreItem *item = memoryCoreMap->GetItem(entry.physicalPage);
        int tlbPos = item->inTlb;
        printf("\t(%d) -> vPage: %d phPage: %d Valid: %d inSwap: %d inTLB: %d\n", 
            page, entry.virtualPage, entry.physicalPage, entry.valid, inswap, tlbPos);
    }
}

#endif /* SWAP */



/// COMENTAR
///
///
void
AddressSpace::InvalidateTlb(Thread *exitThread)
{
    MMU *mmu = machine->GetMMU();
    for (unsigned i = 0; i < TLB_SIZE; i++)
    {
        if(mmu->tlb[i].valid == true) {
            mmu->tlb[i].valid = false;
#ifdef SWAP
            if(exitThread != nullptr)
            {
                memoryCoreMap->ClearItemTlb(mmu->tlb[i].physicalPage);
            }
#endif
        }
    }
}

/// COMENTAR
void
AddressSpace::SavePageFromTLB(unsigned page)
{
    TranslationEntry *tlb = machine->GetMMU()->tlb;
    if (tlb[page].valid)
    {
        pageTable[tlb[page].virtualPage].dirty = tlb[page].dirty;
        pageTable[tlb[page].virtualPage].use   = tlb[page].use;
        tlb[page].valid = false;
    }
}

/// COMENTAR
bool 
AddressSpace::SetTlbPage(TranslationEntry *pageTranslation)
{
#ifdef USE_TLB
    //COMPLETAR CHEQUEOS
    if(pageTranslation == nullptr) {
        DEBUG('p', "Page translation invalid\n");
        return false;
    }
    TranslationEntry *tlb = machine->GetMMU()->tlb;
    // DEBUG('p', "Set Tlb in page: %d \n", tlbIndex);

    if(tlb[tlbIndex].valid)
    {
        SavePageFromTLB(tlbIndex);
        memoryCoreMap->ClearItemTlb(tlb[tlbIndex].physicalPage);
    }
    tlb[tlbIndex] = *pageTranslation;
    memoryCoreMap->SetItemTlb(tlb[tlbIndex].physicalPage, tlbIndex);
    tlbIndex = (tlbIndex + 1) % TLB_SIZE;
    return true;
#else
    DEBUG('p', "TLB not present in the machine.\n");
    return false;
#endif
}

/// On a context switch, save any machine state, specific to this address
/// space, that needs saving.
///
/// For now, nothing!
void
AddressSpace::SaveState()
{
    for(unsigned page = 0; page < TLB_SIZE; page++)
    {
        SavePageFromTLB(page);
#ifdef SWAP
        TranslationEntry *tlb = machine->GetMMU()->tlb;
        memoryCoreMap->ClearItemTlb(tlb[page].physicalPage);
#endif
    }
}

/// On a context switch, restore the machine state so that this address space
/// can run.
///
/// For now, tell the machine where to find the page table.
void
AddressSpace::RestoreState(Thread *exitThread)
{
#ifdef USE_TLB
    InvalidateTlb(exitThread);
#else
    machine->GetMMU()->pageTable = pageTable;
    machine->GetMMU()->pageTableSize = numPages;
#endif
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
    DEBUG('p', "Initializing stack register to %u\n",
          numPages * PAGE_SIZE - 16);
}

/// COMENTAR
///
///
TranslationEntry * 
AddressSpace::GetTranslationEntry(unsigned vPage)
{
    return &pageTable[vPage];
}

/// COMENTAR
bool
AddressSpace::IsInitialized()
{
    return initialized;
}

/// COMENTAR
unsigned 
AddressSpace::GetSize()
{
    return numPages * PAGE_SIZE;
}

unsigned 
AddressSpace::GetNumPages()
{
    return numPages;
}

