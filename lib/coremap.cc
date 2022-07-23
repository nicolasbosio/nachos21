/// Copyright (c) 2018-2021 Docentes de la Universidad Nacional de Rosario.
/// All rights reserved.  See `copyright.h` for copyright notice and
/// limitation of liability and disclaimer of warranty provisions.


#include "coremap.hh"
#include "machine/mmu.hh"

#include <stdlib.h>
#include <stdio.h>

CoreMap::CoreMap(unsigned size)
{
    table = new CoreItem[size];
    map = new Bitmap(size);
    memLock = new Lock("Lock Coremap");
    for(unsigned i = 0; i < size; i++)
    {
        Clear(i);
    }
#ifdef PRPOLICY_FIFO
    fifoList = new List<unsigned>;
#endif
}

CoreMap::~CoreMap()
{
    delete table;
    delete map;
    delete memLock;
#ifdef PRPOLICY_FIFO
    delete fifoList;
#endif /* PRPOLICY_FIFO */
}

///
void
CoreMap::Clear(unsigned which)
{
    map->Clear(which);
    table[which].valid = false;
    table[which].pid = -1;
    table[which].spaceId = nullptr;
    table[which].physicalPage = -1;
    table[which].virtualPage = -1;
    table[which].inTlb = -1;
    //table[which].busy = false;
}

///
int
CoreMap::Add(AddressSpace *space, unsigned vPage, unsigned pid)
{
    int index = map->Find();
    if (index >= 0 && index < (int)NUM_PHYS_PAGES) {
        table[index].valid = true;
        table[index].spaceId = space;
        table[index].virtualPage = vPage;
        table[index].physicalPage = index;
        table[index].pid = pid;
        table[index].inTlb = -1;
        table[index].busy = true;
#ifdef PRPOLICY_FIFO
        fifoList->Append(index);
#endif
    }
    return index;
}

///
int
CoreMap::AddVictim(AddressSpace *space, unsigned vPage, unsigned pid, unsigned victim)
{
    // ASSERT(!map->Test(victim));
    // map->Mark(victim);
    if (victim >= 0 && victim < (int)NUM_PHYS_PAGES) {
        table[victim].valid = true;
        table[victim].spaceId = space;
        table[victim].virtualPage = vPage;
        table[victim].physicalPage = victim;
        table[victim].pid = pid;
        table[victim].inTlb = -1;
        table[victim].busy = true;
#ifdef PRPOLICY_FIFO
        fifoList->Append(victim);
#endif
    }
    return victim;
}


///
unsigned
CoreMap::CountClear()
{
    return map->CountClear();
}

///
CoreItem*
CoreMap::GetItem(unsigned index)
{
    return &table[index];
}

///
unsigned 
CoreMap::PickVictim()
{
    unsigned phyPage;
#ifdef PRPOLICY_FIFO
    phyPage = fifoList->Pop();
#elif defined(PRPOLICY_LRU)
    phyPage = 0;
#else
    do
    {
        phyPage = (unsigned)SystemDep::Random() % NUM_PHYS_PAGES;
    } while (table[phyPage].busy);
    table[phyPage].busy = true;
#endif
    return phyPage;
} 

void
CoreMap::SetItemTlb(unsigned index, unsigned tlbPos)
{
    table[index].inTlb = tlbPos;
}

void
CoreMap::ClearItemTlb(unsigned index)
{
    table[index].inTlb = -1;
}

Lock*
CoreMap::GetLock()
{
    return memLock;
}

void 
CoreMap::unlockPage(unsigned index)
{
    if (table[index].busy == true) {
        table[index].busy = false;
    }
    else
        ASSERT(false);
}

/// COMENTAR
void
CoreMap::PrintCoreMap()
{
    CoreItem *item;
    printf("\nCOREMAP content (%d entries)\n", NUM_PHYS_PAGES);
    for(unsigned page = 0 ; page < NUM_PHYS_PAGES ; page++) {
        item = GetItem(page);
        printf("\t(%d) -> Valid: %d pid: %d vPage: %d phyPage: %d Space: %p\n", 
            page, item->valid, item->pid, item->virtualPage, item->physicalPage, item->spaceId);
    }
}