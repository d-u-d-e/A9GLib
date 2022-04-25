#include "GSMLocation.h"

#define GSM_LOCATION_UPDATE_INTERVAL_MIN 1000*60
#define GSM_LOCATION_UPDATE_INTERVAL_HOUR 1000*3600

GSMLocation::GSMLocation() :
    _commandSent(false),
    _locationAvailable(false),
    _latitude(0),
    _longitude(0),
    _altitude(0),
    _on(false),
    _uncertainty(0)
{
    MODEM.addUrcHandler(this);
}

GSMLocation::~GSMLocation()
{
    MODEM.removeUrcHandler(this);
}

bool GSMLocation::set(bool on)
{
    if(!_on && on){
        MODEM.send("AT+AGPS=1"); //TODO THIS ACTUALLY RESPONDS WITH THREE REPLIES
        if(MODEM.waitForResponse() == 1){
            _on = true;
            return true;
        }
    }
    else if(_on && !on){
        MODEM.send("AT+AGPS=0");
        if(MODEM.waitForResponse() == 1){
            _on = false;
            return true;
        }
    }
    else if( (_on && on) || (!_on && !on) )
        return true;
    return false;
}

bool GSMLocation::available()
{
    if (!_commandSent) {
        _commandSent = true;
        _locationAvailable = false;

        MODEM.send("AT+LOCATION=2");
        MODEM.waitForResponse();
    }

    MODEM.poll();

    if (_locationAvailable) {
        _commandSent = false;
        _locationAvailable = false;
        return true;
    }

    return false;
}

float GSMLocation::latitude()
{
    return _latitude;
}

float GSMLocation::longitude()
{
    return _longitude;
}

long GSMLocation::altitude()
{
    return _altitude;
}

long GSMLocation::accuracy()
{
    return _uncertainty;
}

void GSMLocation::handleUrc(const String& urc)
{
    if (urc.startsWith("????")) { //TODO
        

        _locationAvailable = true;


    }
}
