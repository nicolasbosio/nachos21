#include "thread_test_channel.hh"
#include "thread.hh"
#include "system.hh"
#include <stdio.h>

Channel *channel;
bool send, receive;


static void
enviar(void *mensaje){
    int *msg = (int *) mensaje;
    channel->Send(*msg);
    send = true;
}

static void
recibir(void *buffer){
    int *bf = (int *) buffer;
    channel->Receive(bf);
    receive = true;
}

void ThreadTestChannel() {
    send = false; 
    receive = false;
    channel = new Channel("Canal 1");
    int buffer;
    int mensaje = 5;
    Thread *s = new Thread("SEND", true);
    Thread *r = new Thread("RECEIVE", true);
    
    r->Fork(recibir, (void**) &buffer);
    s->Fork(enviar, (void *) &mensaje);

    r->Join();
    s->Join();
    
    printf("Lo que recibi es: %d\n", buffer);
}


