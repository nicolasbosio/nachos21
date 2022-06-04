/// Copyright (c) 2019-2021 Docentes de la Universidad Nacional de Rosario.
/// All rights reserved.  See `copyright.h` for copyright notice and
/// limitation of liability and disclaimer of warranty provisions.


#include "transfer.hh"
#include "lib/utility.hh"
#include "threads/system.hh"

void ReadBufferFromUser(int userAddress, char *outBuffer,
                        unsigned byteCount) 
{
    ASSERT(userAddress != 0);
    ASSERT(outBuffer != nullptr);
    ASSERT(byteCount > 0);

    unsigned count = 0;
    while (count < byteCount) {
        int temp;

        bool valid = false;
        for(int i = 0 ; i < 5 && !valid; i++){
            valid = machine->ReadMem(userAddress, 1, &temp);
        }
        ASSERT(valid);
        userAddress++;
        *(outBuffer + count++) = (unsigned char) temp;
    }
}

bool ReadStringFromUser(int userAddress, char *outString,
                        unsigned maxByteCount) 
{
    ASSERT(userAddress != 0);
    ASSERT(outString != nullptr);
    ASSERT(maxByteCount != 0);

    unsigned count = 0;
    do {
        int temp;
        count++;

        bool valid = false;
        // TODO: porque menor a 5???????
        for(int i = 0 ; i < 5 && !valid ; i++){
            valid = machine->ReadMem(userAddress, 1, &temp);
        }
        ASSERT(valid);
        userAddress++;

        *outString = (unsigned char) temp;
    } while (*outString++ != '\0' && count < maxByteCount);

    return *(outString - 1) == '\0';
}

void WriteBufferToUser(const char *buffer, int userAddress,
                       unsigned byteCount) 
{
    ASSERT(userAddress != 0);
    ASSERT(buffer != nullptr);

    if(byteCount != 0) {
        unsigned count = 0;
        do {
            count++;
            bool valid = false;
            for(int i = 0 ; i < 5 && !valid ; i++){
                valid = machine->WriteMem(userAddress, 1, (int) *buffer);
            }
            ASSERT(valid);
            userAddress++;
            buffer++;
        } while (count < byteCount);
    }
}

void WriteStringToUser(const char *string, int userAddress)
{
    ASSERT(userAddress != 0);
    ASSERT(string != nullptr);
    do {
        bool valid = false;
        for(int i = 0 ; i < 5 && !valid ; i++){
            valid = machine->WriteMem(userAddress, 1, (int) *string);
        }
        ASSERT(valid);
        userAddress++;
    } while (*string++ != '\0');
}
