/// Copyright (c) 1992-1993 The Regents of the University of California.
///               2007-2009 Universidad de Las Palmas de Gran Canaria.
///               2016-2021 Docentes de la Universidad Nacional de Rosario.
/// All rights reserved.  See `copyright.h` for copyright notice and
/// limitation of liability and disclaimer of warranty provisions.


#include "thread_test_damian.hh"
#include "system.hh"

#include <stdio.h>
#include <string.h>

void Hello(void* args) {
    puts("Hola mundo!");
    scheduler->Print();
}

void ThreadTestDamian() {
    Thread *hilos[5];
    for(int i = 0; i < 5; i++) {
      char *name = new char [64];
      sprintf(name, "%d", i);
      Thread *newThread = new Thread(name, true, 5);
      hilos[i] = newThread;
      newThread->Fork(Hello, (void *) name);
    }
    scheduler->Print();

    ////////////////
    
    for(int i = 0; i < 5; i++) {
      hilos[i]->Join();
    }
    
    ///////////////////////
    //currentThread->Yield();
    puts("Fin");
}
