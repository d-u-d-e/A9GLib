#ifndef _GSM_H_INCLUDED
#define _GSM_H_INCLUDED

#include <Arduino.h>

#include "modem.h"

enum NetworkStatus {ERROR, CONNECTING, GSM_READY, GSM_OFF, GPRS_READY, GPRS_OFF};

class GSM {

public:
    GSM();
    NetworkStatus init(const char* pin = 0, bool restart = false, bool synchronous = true);
    bool isAccessAlive();
    bool shutdown();
    uint8_t ready();
    unsigned long getTime();
    unsigned long getLocalTime();
    bool setLocalTime(time_t time, uint8_t quarters_from_utc);
    void lowPowerMode();
    void noLowPowerMode();
    int8_t getSignalQuality(unsigned long timeout = 100);
    static const char * signal2String(int8_t signalQuality);
    bool waitForNetwork(unsigned long timeout, int8_t * signal = NULL);
    NetworkStatus status();
private:
    NetworkStatus _state;
    uint8_t _readyState;
    const char* _pin;
    String _response;
    unsigned long _timeout;
};

#endif
