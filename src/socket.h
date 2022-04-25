#ifndef _SOCKET_H_INCLUDED
#define _SOCKET_H_INCLUDED

#include "modem.h"

#define BUFFER_MAX 128 

class GSM_Socket: public ModemUrcHandler{

public:
    friend class ModemClass;
    friend class GPRS;
private:
    GSM_Socket(uint8_t mux);
    bool close(unsigned long timeout = 1000L);
    uint16_t read(uint8_t * buffer, uint16_t len = 1, unsigned long timeout = 1000L);
    uint16_t send(const void * buff, uint16_t len);
    void handleUrc(const uint8_t * urc, uint16_t len);
    uint8_t _mux;
    uint8_t _buffer[BUFFER_MAX];
    uint8_t _freeIndex;
    uint8_t _free;
};

#endif

