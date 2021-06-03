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
    //
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
        DEBUG('p', "La cantidad de paginas requeridas no alcanza\n");
        //return;
    }

    DEBUG('p', "Initializing address space, num pages %u, size %u\n",
          numPages, size);

    // First, set up the translation.
    pageTable = new TranslationEntry[numPages];
    for (unsigned i = 0; i < numPages; i++) {
        pageTable[i].virtualPage  = i;
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
        pageTable[i].inSwap       = false;
        pageTable[i].inTLB        = -1;
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

#ifndef SWAP
    int physPage = memoryMap->Find();
#else

    unsigned pid = 0;
    for( ; pid < MAX_SPACE ; pid++) {
        //DEBUG('p', "FOR RANCIO: pid -> %d\n", pid); //BORRAR
        if((AddressSpace *)tableThread[pid].space == currentThread->space) {
            break;   
        }
    }

    int physPage = memoryCoreMap->Add(this, pageNum, pid);
    // si no hay mas espacio el add devuelve -1 y no agrega nada al core map
    // pero mas abajo estamos escribiendo en el marco fisico que nos devuelve
    // writetoswap pero nunca volvemos a hacer un add
    //DEBUG('p', "Physical page devuelta por coremap: %d\n", physPage); //BORRAR

    if(physPage == -1) {
        DEBUG('p', "nos quedamos sin espacio\n"); //BORRAR
        //physPage = WritePagetoSwap();
        int victim = WritePagetoSwap();
        //Si ESTO NO ES ATOMICO ACA SE PUEDE ROMPER ALGO
        //SI EN EL MEDIO DEL PROCESO SE CEDE EL CONTROL
        //Y EL OTRO HILO TAMB SWAPEA PAGINAS
        //VER DE PONER SEMAFOROS PARA EVITAR PROBLEMAS
        physPage = memoryCoreMap->Add(this, pageNum, pid);
        ASSERT(victim == physPage);
    }

#endif

    ASSERT(physPage != -1);
    pageTable[pageNum].virtualPage = pageNum;
    pageTable[pageNum].physicalPage = physPage;
    pageTable[pageNum].valid = true;
    pageTable[pageNum].inSwap = false;
    pageTable[pageNum].inTLB = -1;
    
    if (vAddr >= (codeSize + initDataSize))
    {
        DEBUG('p', "'Load Stack block' -> vPage: %d -- phyPage %d\n", 
            pageTable[pageNum].virtualPage, pageTable[pageNum].physicalPage);
        memset(&mainMemory[pageTable[pageNum].physicalPage * PAGE_SIZE], 0, PAGE_SIZE);
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
        init = true;
    }
    else
    {
        uint32_t readSize = min(initDataSize - (vAddr - codeSize), PAGE_SIZE);
        uint32_t offset = pageNum * PAGE_SIZE;

        DEBUG('p', "'Load initData block' -> vPage: %d -- phyPage %d -- readSize %d -- vAddr %d\n",
              pageTable[pageNum].virtualPage, pageTable[pageNum].physicalPage, PAGE_SIZE, vAddr);

        exe.ReadDataBlock(&mainMemory[pageTable[pageNum].physicalPage * PAGE_SIZE], readSize, (offset - codeSize));
        init = true;
    }
    ASSERT(init);

    return pageTable[pageNum];
}
#endif




#ifdef SWAP


/// Devuelve un entero que indica la pagina fisica respecto al comienzo de memoria que se libero
int
AddressSpace::WritePagetoSwap()
{
    MMU *mmu = machine->GetMMU();
    char *mainMemory = mmu->mainMemory;
    unsigned victimIndex = memoryCoreMap->PickVictim();
    DEBUG('x', "Victim index: %d\n", victimIndex); //BORRAR
    CoreItem *victimData = memoryCoreMap->GetItem(victimIndex);
    DEBUG('x', "VictimData pid: %d\n", victimData->pid); //BORRAR
    const char *swapFileName = tableThread[victimData->pid].thread->GetSwapFileName();
    DEBUG('x', "Swap filename: %s\n", swapFileName); //BORRAR
    OpenFile *swapFile = fileSystem->Open(swapFileName);
    DEBUG('x', "Write page to swap: phyPage -> %d -- swapFile -> %s -- virtPage %d\n", victimIndex, swapFileName, victimData->virtualPage);
    int writeSize = swapFile->WriteAt(&mainMemory[victimData->physicalPage * PAGE_SIZE], PAGE_SIZE, victimData->virtualPage * PAGE_SIZE);
    if(writeSize != PAGE_SIZE)
    {
        return -1;
    }
    
#ifdef USE_TLB
    //ESTO NO ES tlbIndex!! esto tiene que ser la posicion donde estaba en la tlb si es que estaba en la tlb
    
    int tlbVictimIndex = victimData->spaceId->pageTable[victimData->virtualPage].inTLB;
    if(tlbVictimIndex != -1)
    {
       victimData->spaceId->SavePageFromTLB(tlbVictimIndex);
       mmu->tlb[tlbVictimIndex].valid = false;
    }

#endif
    delete swapFile;

    victimData->spaceId->pageTable[victimData->virtualPage].inSwap = true;
    victimData->spaceId->pageTable[victimData->virtualPage].inTLB = -1;
    victimData->spaceId->pageTable[victimData->virtualPage].physicalPage = 0;
    memoryCoreMap->Clear(victimIndex);
    
    return victimIndex;
}

bool
AddressSpace::LoadPageFromSwap(TranslationEntry *pageTranslation)
{
    ///SI ESTOY CARGANDO UNA PAGINA DESDE SWAP SOY EL DUEÑO, NO???
    const char *swapFile = currentThread->GetSwapFileName();
    if(swapFile == nullptr) return false;
    DEBUG('x', "Requested Load Page from SWAP: %s | virtual: %d | threadName: %s\n",
          currentThread->GetSwapFileName(), pageTranslation->virtualPage, currentThread->GetName());
    //VER DONDE PUEDE FALLAR
    char *mainMemory = machine->GetMMU()->mainMemory;
    OpenFile *swap = fileSystem->Open(swapFile);
    if(swap == nullptr) return false;

    ///IMPLEMENTAR DE MANERA MAS FACIL EL REVERSEMAPING
    /// donde le pasemos el marco fisico
    unsigned pid = 0;
    for( ; pid < MAX_SPACE ; pid++) {
        DEBUG('x', "FOR RANCIO LOAD PAGE SWAP: pid -> %d\n", pid); //BORRAR
        if((AddressSpace *)tableThread[pid].space == currentThread->space) {
            break;   
        }
    }

    // si la pagina victima no esta dirty creo que se podria evitar mandarla a swap
    // y solamente marcarla como invalida para que desp cuando se necesite se traiga desde
    // demand loading directamente, nos evitariamos una operacion en disco
    int physPage = memoryCoreMap->Add(currentThread->space, pageTranslation->virtualPage, pid);
    if(physPage == -1) {
        DEBUG('x', "No space in memory to load page from SWAP\n"); //BORRAR
        //physPage = WritePagetoSwap();
        int victim = WritePagetoSwap();
        //Si ESTO NO ES ATOMICO ACA SE PUEDE ROMPER ALGO
        //SI EN EL MEDIO DEL PROCESO SE CEDE EL CONTROL
        //Y EL OTRO HILO TAMB SWAPEA PAGINAS
        //VER DE PONER SEMAFOROS PARA EVITAR PROBLEMAS
        physPage = memoryCoreMap->Add(currentThread->space, pageTranslation->virtualPage, pid);
        ASSERT(victim == physPage);
    }
    
    
    DEBUG('x', "LOADING Page from SWAP - physical: %d | virtual: %d | threadName: %s\n",
          physPage, pageTranslation->virtualPage, currentThread->GetName());
    int sizeRead = swap->ReadAt(&mainMemory[physPage * PAGE_SIZE], PAGE_SIZE, pageTranslation->virtualPage * PAGE_SIZE);
    if(sizeRead != PAGE_SIZE) return false;
    pageTable[pageTranslation->virtualPage].physicalPage = physPage;
    pageTranslation->physicalPage = physPage;
    delete swap;
    pageTable[pageTranslation->virtualPage].inSwap = false;
    pageTranslation->inSwap = false;
    pageTranslation->inTLB = -1;
    //estoy modificando el page translation del hilo pero no el que en exeptio devuelve el gettranslation
    return true;
}

/// COMENTAR
void
AddressSpace::PrintCoreMap()
{
    CoreItem *item;
    printf("\nCOREMAP content (%d entries)\n", NUM_PHYS_PAGES);
    for(unsigned page = 0 ; page < NUM_PHYS_PAGES ; page++) {
        item = memoryCoreMap->GetItem(page);
        printf("\t(%d) -> Valid: %d pid: %d vPage: %d phyPage: %d Space: %p\n", 
            page, item->valid, item->pid, item->virtualPage, item->physicalPage, item->spaceId);
    }
}

/// COMENTAR
void
AddressSpace::PrintPageTable() 
{
    printf("\nPAGE TABLE (%s) content (%d entries)\n",currentThread->GetName(), numPages);
    TranslationEntry entry;
    for(unsigned page = 0 ; page < numPages ; page++) {
        entry = pageTable[page];
        printf("\t(%d) -> vPage: %d phPage: %d Valid: %d inSwap: %d inTLB: %d\n", 
            page, entry.virtualPage, entry.physicalPage, entry.valid, entry.inSwap, entry.inTLB);
    }
}
#endif

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
            if(exitThread != nullptr)
            {
                exitThread->space->pageTable[mmu->tlb[i].virtualPage].inTLB = -1;
            }
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
AddressSpace::SetTlbPage(TranslationEntry pageTranslation)
{
#ifdef USE_TLB
    /* COMPLETAR CHEQUEOS
    if(pageTranslation == nullptr) {
        DEBUG('a', "");
        return false;
    }
    */
    TranslationEntry *tlb = machine->GetMMU()->tlb;
    DEBUG('p', "Set Tlb in page: %d\n", tlbIndex);

    if(tlb[tlbIndex].valid)
    {
        SavePageFromTLB(tlbIndex);
        pageTable[tlb[tlbIndex].virtualPage].inTLB = -1;
    }
    tlb[tlbIndex] = pageTranslation;
    pageTable[pageTranslation.virtualPage].inTLB = tlbIndex;
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
    TranslationEntry *tlb = machine->GetMMU()->tlb;
    for(unsigned page = 0; page < TLB_SIZE; page++)
    {
        SavePageFromTLB(page);
        pageTable[tlb[page].virtualPage].inTLB = -1;
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
TranslationEntry 
AddressSpace::GetTranslationEntry(unsigned vPage)
{
    return pageTable[vPage];
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

