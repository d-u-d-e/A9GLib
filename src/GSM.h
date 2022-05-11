#ifndef _GSM_H_INCLUDED
#define _GSM_H_INCLUDED

#include <Arduino.h>

#include "modem.h"


class GSM {

public:
    GSM();
    NetworkStatus init(const char* pin = NULL, bool restart = false, uint32_t timeout_ms = 60 * 1000);
    bool isAccessAlive();
    bool shutdown();
    uint8_t ready();
    uint32_t getTime();
    uint32_t getLocalTime();
    bool setLocalTime(uint32_t time, uint8_t quarters_from_utc);
    void lowPowerMode();
    void noLowPowerMode();
    int8_t getSignalQuality(uint32_t timeout_ms = 100);
    static const char* signal2String(int8_t signalQuality);
    bool waitForNetwork(uint32_t timeout_ms, int8_t * signal = NULL);
    NetworkStatus status();
private:
    NetworkStatus _state;
    uint8_t _readyState;
    const char* _pin;
    String _response;
    uint32_t _timeout;
};

#endif
