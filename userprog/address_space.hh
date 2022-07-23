/// Data structures to keep track of executing user programs (address
/// spaces).
///
/// For now, we do not keep any information about address spaces.  The user
/// level CPU state is saved and restored in the thread executing the user
/// program (see `thread.hh`).
///
/// Copyright (c) 1992-1993 The Regents of the University of California.
///               2016-2021 Docentes de la Universidad Nacional de Rosario.
/// All rights reserved.  See `copyright.h` for copyright notice and
/// limitation of liability and disclaimer of warranty provisions.

#ifndef NACHOS_USERPROG_ADDRESSSPACE__HH
#define NACHOS_USERPROG_ADDRESSSPACE__HH

#include "filesys/file_system.hh"
#include "machine/translation_entry.hh"

const unsigned USER_STACK_SIZE = 1024;  ///< Increase this as necessary!

class CoreItem;
class Thread;

class AddressSpace {
public:

    /// Create an address space to run a user program.
    ///
    /// The address space is initialized from an already opened file.
    /// The program contained in the file is loaded into memory and
    /// everything is set up so that user instructions can start to be
    /// executed.
    ///
    /// Parameters:
    /// * `executable_file` is the open file that corresponds to the
    ///   program; it contains the object code to load into memory.
    AddressSpace(OpenFile *executable_file);

    /// De-allocate an address space.
    ~AddressSpace();

#ifdef DEMAND_LOADING
    /// COMENTAR
    TranslationEntry *LoadPage(unsigned vAddr);
#endif

    /// Initialize user-level CPU registers, before jumping to user code.
    void InitRegisters();

    /// COMENTAR
    void InvalidateTlb(Thread* exitThread);

    /// COMENTAR
    TranslationEntry * GetTranslationEntry(unsigned vPage);
    
    /// Save/restore address space-specific info on a context switch.
    void SaveState();

    /// COMENTAR
    void SavePageFromTLB(unsigned page);

    /// COMENTAR
    void RestoreState(Thread* exitThread);
    
    /// COMENTAR
    bool IsInitialized();
    
    /// COMENTAR
    unsigned GetSize();
    
    /// COMENTAR
    bool SetTlbPage(TranslationEntry *pageTranslation);
    
    /// COMENTAR
    unsigned GetNumPages();

#ifdef SWAP
    /// COMENTAR
    int WritePagetoSwap();

    /// COMENTAR
    bool LoadPageFromSwap(TranslationEntry *pageTranslation);

    /// COMENTAR
    void PrintPageTable(); //BORRAR
    
#endif /* SWAP */

private:

    /// Assume linear page table translation for now!
    TranslationEntry *pageTable;

    /// Number of pages in the virtual address space.
    unsigned numPages;

#if USE_TLB
    /// COMENTAR
    unsigned tlbIndex;
#endif

    /// COMENTAR
    bool initialized;

#ifdef DEMAND_LOADING
    /// COMENTAR
    unsigned int codeSize;
    
    /// COMENTAR
    unsigned int initDataSize;
    
    /// COMENTAR
    OpenFile *executableFile;
#endif
};
#endif
