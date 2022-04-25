#ifndef _SOCKET_H_INCLUDED
#define _SOCKET_H_INCLUDED

#include "modem.h"

#define BUFFER_MAX 4096 

class GSM_Socket{

public:
    friend class GPRS;
private:
    GSM_Socket(uint8_t mux);
    bool close(unsigned long timeout = 1000L);
    uint8_t read(char * buffer, uint8_t len = 1, unsigned long timeout = 1000L);
    uint16_t send(const void * buff, size_t len);
    void handleUrc(const String& urc);
    uint8_t _mux;
    char _buffer[BUFFER_MAX];
    uint8_t _freeIndex;
    uint8_t _available;
};

#endif

