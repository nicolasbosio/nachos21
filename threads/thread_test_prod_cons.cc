/// Copyright (c) 1992-1993 The Regents of the University of California.
///               2007-2009 Universidad de Las Palmas de Gran Canaria.
///               2016-2021 Docentes de la Universidad Nacional de Rosario.
/// All rights reserved.  See `copyright.h` for copyright notice and
/// limitation of liability and disclaimer of warranty provisions.


#include "thread_test_prod_cons.hh"
#include "system.hh"
#include "condition.hh"
#include "lib/utility.hh"

#include <stdio.h>
#include <stdlib.h>

int MAX_ACTORES = 5;
int MAX_ITERATION = 50;
int MAX_BUFFER = 50;
const int SEED = 1235;

Lock *lock = new Lock("Inicializacion del Lock");
Condition *isFull = new Condition("Condition Full", lock);
Condition *isEmpty = new Condition("Condition Empty", lock);
List<int> *buffer = new List<int>; 
int index = 0;
unsigned int itemsP = 0;
unsigned int itemsC = 0;

static void
Productor(void *n_)
{
    for(int i = 0 ; i < MAX_ITERATION ; i++) {
        lock->Acquire();
        while(index == MAX_BUFFER) {
            printf("estoy en el wait de productor\n");
            isFull->Wait();   
        }
        printf("Soy el productor %s, estoy produciendo\n", currentThread->GetName());
        index++;
        itemsP++;
        buffer->Append(1);
        lock->Release();
        isEmpty->Broadcast();
    }
}

static void
Consumidor(void *n_)
{
    for(int i = 0 ; i < MAX_ITERATION ; i++) {
        lock->Acquire();
        while(index == 0) {
            printf("estoy en el wait de consumidor\n");
            isEmpty->Wait();
        }
        index--;
        itemsC++;
        int element = buffer->Pop();
        printf("Soy el consumidor %s, estoy consumiendo. Consumi: %d\n", currentThread->GetName(), element);
        lock->Release();
        isFull->Broadcast();
    }
}

void
ThreadTestProdCons()
{
    srand(SEED);
    Thread *array[MAX_ACTORES];
    bool rol;
    int cantP = 0, cantC = 0;
    for (int i = 0; i < MAX_ACTORES; i++)
    {
        rol  = rand() % 2;
        char *name = new char [64];
        if(rol) {
            sprintf(name, "Productor %d", cantP++);
            array[i] = new Thread(name, true);
            array[i]->Fork(Productor, NULL);
        }
        else {
            sprintf(name, "Consumidor %d", cantC++);
            array[i] = new Thread(name, true);
            array[i]->Fork(Consumidor, NULL);
        }
    }
    printf("Cantidad Productores: %d\n Cantidad de Consumidores: %d\n", cantP,cantC);
    for (int i = 0; i < MAX_ACTORES; i++)
        array[i]->Join();

    for (int i = 0; i < MAX_ACTORES; i++) {
        delete array[i]->GetName();
    }
    int resultado = itemsP - itemsC;
    ASSERT(index == resultado);
    printf("All producers and consumers finished.\n");
}