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
    sem = new Semaphore("Semaforo Coremap", 1);
    for(unsigned i = 0; i < size; i++)
    {
        Clear(i);
    }
}

CoreMap::~CoreMap()
{
    delete table;
    delete map;
    delete sem;
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
    }
    return index;
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
    return (unsigned)SystemDep::Random() % NUM_PHYS_PAGES;
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

Semaphore*
CoreMap::GetSemaphore()
{
    return sem;
}