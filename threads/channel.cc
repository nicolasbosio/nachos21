#include "channel.hh"
#include "system.hh"
#include <stdio.h>
#include <string.h>

Channel::Channel(const char *debugName){
    name = debugName;
    buffer = nullptr;
    lock = new Lock("New Channel Lock");
    condR = new Condition("Condition Receive", lock);
    condS = new Condition("Condition Send", lock);
    DEBUG('t',"FIN CREACION DEL CANAL\n");
}

Channel::~Channel(){
    //lock->~Lock();
    //condR->~Condition();
    //condS->~Condition();
    delete lock;
    delete condR;
    delete condS;
}

void
Channel::Send(int message){
    DEBUG('t',"SEND\n");
    lock->Acquire();
    DEBUG('t',"ACQUIRE SEND\n");
    while(buffer == nullptr)
        condR->Wait();
    *buffer = message;
    condS->Signal();
    lock->Release();
    DEBUG('t',"FIN DEL SEND\n");
}

void
Channel::Receive(int *message){
    lock->Acquire();
    DEBUG('t',"ACQUIRE RECEIVE\n");
    while(buffer != nullptr)
        condS->Wait();
    DEBUG('t',"CONTENIDO DEL MESSAGE %d\n", message);
    buffer = message;
    condR->Signal();
    condS->Wait();
    DEBUG('t',"CONTENIDO DEL BUFFER\n ");
    buffer = nullptr;
    lock->Release();
}

const char *
Channel::GetName() const
{
    return name;
}