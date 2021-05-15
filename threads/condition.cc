/// Routines for synchronizing threads.
///
/// The implementation for this primitive does not come with base Nachos.
/// It is left to the student.
///
/// When implementing this module, keep in mind that any implementation of a
/// synchronization routine needs some primitive atomic operation.  The
/// semaphore implementation, for example, disables interrupts in order to
/// achieve this; another way could be leveraging an already existing
/// primitive.
///
/// Copyright (c) 1992-1993 The Regents of the University of California.
///               2016-2021 Docentes de la Universidad Nacional de Rosario.
/// All rights reserved.  See `copyright.h` for copyright notice and
/// limitation of liability and disclaimer of warranty provisions.


#include "condition.hh"

/// Note -- without a correct implementation of `Condition::Wait`, the test
/// case in the network assignment will not work!

Condition::Condition(const char *debugName, Lock *conditionLock)
{
    name = debugName;
    lock = conditionLock;
    list = new List<Semaphore* >();

}

Condition::~Condition()
{
    list->~List();
    delete list;
}

const char *
Condition::GetName() const
{
    return name;
}

void
Condition::Wait()
{
    Semaphore *wait = new Semaphore(name, 0);
    list->Append(wait);
    lock->Release();
    wait->P();

    //////////// CORTE DE EJECUCION

    delete wait;
    lock->Acquire();
}

void
Condition::Signal()
{
    if(!list->IsEmpty()) {
        Semaphore* aux = list->Pop();
        aux->V();
    }
}

void
Condition::Broadcast()
{
    while(!list->IsEmpty()) {
        Signal();
    }
}
