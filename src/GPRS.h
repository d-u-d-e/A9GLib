#ifndef _GPRS_H_INCLUDED
#define _GPRS_H_INCLUDED

#include <IPAddress.h>

#include "GSM.h"
#include "modem.h"
#include "socket.h"

static const char CONNECT_OK[] PROGMEM = "CONNECT OK";
static const char CONNECT_FAIL[] PROGMEM = "CONNECT FAIL";
static const char CONNECT_ALREADY[] PROGMEM = "CONNECT ALREADY";

class GPRS{

public:

    enum class ConnectionStatus {ERROR, CONNECT_OK, CONNECT_FAIL, CONNECT_ALREADY, TIMEOUT};
    GPRS();
    NetworkStatus attachGPRS(const char* apn, const char* user_name, const char* password, bool synchronous = true);
    NetworkStatus detachGPRS(bool synchronous = true);

    bool connect(const char* host, uint16_t port, uint8_t* mux, unsigned long timeout_s, ConnectionStatus* status);
    bool close(uint8_t mux, unsigned long timeout); 
    uint16_t send(uint8_t mux, const void* buff, uint16_t len);
    uint16_t read(uint8_t mux, void * buf, uint16_t len = 1, unsigned long timeout = 1000L);

    uint8_t ready();
    IPAddress getIPAddress();
    void setTimeout(unsigned long timeout);
    NetworkStatus status();
    
private:
    const char* _apn;
    const char* _username;
    const char* _password;
    NetworkStatus _state;
    uint8_t _readyState;
    String _response;
    unsigned long _timeout;
};

#endif
