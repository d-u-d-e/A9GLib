#ifndef _GPRS_H_INCLUDED
#define _GPRS_H_INCLUDED

#include <IPAddress.h>
#include "GSM.h"

#include "modem.h"

class GPRS{

public:

    GPRS();

    NetworkStatus networkAttach(char* networkId, char* user, char* pass)
    {
        return attachGPRS(networkId, user, pass);
    };

    NetworkStatus networkDetach(){ return detachGPRS(); };
    NetworkStatus attachGPRS(const char* apn, const char* user_name, const char* password, bool synchronous = true);
    NetworkStatus detachGPRS(bool synchronous = true);

    /** Returns 0 if last command is still executing
      @return 1 if success, >1 if error
   */
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
