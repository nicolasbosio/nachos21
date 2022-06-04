/// Miscellaneous useful definitions, including debugging utilities.
///
/// See also `lib/debug.hh`.
///
/// Copyright (c) 1992-1993 The Regents of the University of California.
///               2016-2021 Docentes de la Universidad Nacional de Rosario.
/// All rights reserved.  See `copyright.h` for copyright notice and
/// limitation of liability and disclaimer of warranty provisions.

#include "userprog/address_space.hh"
#include "lib/bitmap.hh"
#include "threads/semaphore.hh"
#ifndef NACHOS_COREMAP__HH
#define NACHOS_COREMAP__HH

class CoreItem 
{
public:

    /// Valid state of entry
    bool valid;

    /// The SpaceId owner
    AddressSpace *spaceId;

    ///
    int pid;

    /// The page number in virtual memory.
    int virtualPage;

    /// The page number in real memory (relative to the start of
    /// `mainMemory`).
    int physicalPage;

    ///
    int inTlb;

private:
};

class CoreMap
{
public:

    ///
    CoreMap(unsigned size);

    ///
    ~CoreMap();

    ///
    void Clear(unsigned which);

    ///
    int Add(AddressSpace *space, unsigned vPage, unsigned pid);

    ///
    unsigned CountClear();

    ///
    CoreItem* GetItem(unsigned index);
    
    ///
    unsigned PickVictim();

    ///
    void SetItemTlb(unsigned index, unsigned tlbPos);

    ///
    void ClearItemTlb(unsigned index);

    ///
    Semaphore* GetSemaphore();

private:
    CoreItem *table;
    Bitmap *map;
    Semaphore *sem;
};

#endif
