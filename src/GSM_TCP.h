#ifndef _TCP_H_INCLUDED
#define _TCP_H_INCLUDED

#include "modem.h"

#define TCP_BUFFER_MAX 1024

//TODO: enable multiple TCP connections; see GPRS.cpp in order to enable them with AT+CIPMUX=1

enum class TCP_NetworkStatus {ERROR, CONNECT_OK, CONNECT_FAIL, CONNECT_ALREADY, TIMEOUT};

static const char CONNECT_OK[] PROGMEM = "CONNECT OK";
static const char CONNECT_FAIL[] PROGMEM = "CONNECT FAIL";
static const char CONNECT_ALREADY[] PROGMEM = "CONNECT ALREADY";

//this shall be a singleton for the moment
class GSM_TCP: public ModemUrcHandler{

public:
    GSM_TCP();

    TCP_NetworkStatus status();
    bool connect(const char * host, uint16_t port/*, uint8_t* mux*/, unsigned long timeout_s = 75, TCP_NetworkStatus * status = NULL);
    bool close(unsigned long timeout = 1000L);
    uint8_t read(char * buffer, uint8_t len = 1, unsigned long timeout = 1000L);
    uint16_t send(const void * buff, size_t len /*, uint8_t mux*/);
    void handleUrc(const String& urc);

private:

    String _response;
    char _buffer[TCP_BUFFER_MAX];
    uint8_t _freeIndex;
    uint8_t _available;
};

#endif

