#ifndef _GSM_H_INCLUDED
#define _GSM_H_INCLUDED

#include <Arduino.h>

#include "modem.h"

enum NetworkStatus { ERROR, CONNECTING, GSM_READY, GSM_OFF, GPRS_READY, GPRS_OFF};

class GSM {

public:
    /** Constructor
    */
    GSM();

    /** Start the GSM/GPRS modem, attaching to the GSM network
      @param pin         SIM PIN number (4 digits in a string, example: "1234"). If
                         NULL the SIM has no configured PIN.
      @param restart     Restart the modem. Default is FALSE. If it is running, it will restart.
      @param synchronous If TRUE the call only returns after the Start is complete
                         or fails. If FALSE the call will return immediately. You have
                         to call repeatedly ready() until you get a result. Default is TRUE.
      @return If synchronous, GSM3_NetworkStatus_t. If asynchronous, returns 0.
    */
    NetworkStatus init(const char* pin = 0, bool restart = false, bool synchronous = true);

    /** Check network access status
      @return 1 if Alive, 0 if down
   */
    bool isAccessAlive();

    /** Shutdown the modem (power off really)
      @return true if successful
    */
    bool shutdown();

    /** Get last command status
      @return returns 0 if last command is still executing, 1 success, >1 error
    */
    uint8_t ready();

    void setTimeout(unsigned long timeout);

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
