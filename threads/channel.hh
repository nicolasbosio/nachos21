#ifndef NACHOS_THREADS_CHANNEL__HH
#define NACHOS_THREADS_CHANNEL__HH

#include "condition.hh"

class Channel {
public:

    /// Constructor: set up the channel
    Channel(const char *debugName);

    ~Channel();

    /// For debugging.
    const char *GetName() const;

    void Send(int message);
    void Receive(int *message);

private:

    /// For debugging.
    const char *name;
    
    // Add other needed fields here.
    int *buffer;
    Lock *lock;
    Condition *condR, *condS;

};


#endif