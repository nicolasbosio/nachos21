#include "thread_test_scheduler.hh"
#include "thread.hh"
#include "system.hh"

#include <stdio.h>
#include <stdlib.h>

int MAX_THREAD = 50;
const int SEED = 1235;


void printPriority(void *) {
    printf("Mi prioridad es: %u\n", currentThread->GetPriority());
}


void ThreadTestScheduler() {
    srand(SEED);
    unsigned int prioridad;
    Thread *array[MAX_THREAD];
    for (int i = 0; i < MAX_THREAD; i++)
    {
        prioridad  = rand() % MAX_PRIORITY;
        char *name = new char [64];
        sprintf(name, "%d", i);
        array[i] = new Thread(name, true, prioridad);
        array[i]->Fork(printPriority, NULL);

    }

    for (int i = 0; i < MAX_THREAD; i++)
        array[i]->Join();

    for (int i = 0; i < MAX_THREAD; i++)
    {
        delete array[i]->GetName();
    }


    

}