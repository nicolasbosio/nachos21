/// Copyright (c) 2018-2021 Docentes de la Universidad Nacional de Rosario.
/// All rights reserved.  See `copyright.h` for copyright notice and
/// limitation of liability and disclaimer of warranty provisions.


#include "coremap.hh"
#include "machine/mmu.hh"

#include <stdlib.h>

CoreMap::CoreMap(unsigned size)
{
    table = new CoreItem[size];
    map = new Bitmap(size);
    memLock = new Lock("Lock Coremap");
    for(unsigned i = 0; i < size; i++)
    {
        Clear(i);
    }
#if PRPOLICY_FIFO
    fifoList = new List<unsigned>();
#endif
}

CoreMap::~CoreMap()
{
    delete table;
    delete map;
    delete memLock;
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
#if PRPOLICY_FIFO
        fifoList.Append(index);
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
#if PRPOLICY_FIFO
    phyPage = fifoList.Pop();
#elif defined(PRPOLICY_LRU)
    phyPage = 0;
#else
    do
    {
        phyPage = (unsigned)SystemDep::Random() % NUM_PHYS_PAGES;
        DEBUG('s', "PICK\n"); //BORRAR
    } while (table[phyPage].busy);
    ASSERT(!table[phyPage].busy); // BORRAR
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