#ifndef _GSM_LOCATION_H_INCLUDED
#define _GSM_LOCATION_H_INCLUDED

#include <Arduino.h>

#include "Modem.h"

class GSMLocation : public ModemUrcHandler {

public:
    GSMLocation();
    virtual ~GSMLocation();

    bool set(bool on = true);
    bool available();
    float latitude();
    float longitude();
    long altitude();
    long accuracy();

    void handleUrc(const String& urc);

private:
    bool _commandSent;
    bool _locationAvailable;

    float _latitude;
    float _longitude;
    long _altitude;
    bool _on;
    long _uncertainty;
};

#endif
