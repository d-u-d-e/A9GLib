#define _XOPEN_SOURCE
#include <time.h>

#include "modem.h"

#include "GSM.h"

enum {
    READY_STATE_CHECK_SIM,
    READY_STATE_UNLOCK_SIM,
    READY_STATE_SET_PREFERRED_MESSAGE_FORMAT,
    READY_STATE_CHECK_REGISTRATION,
    READY_STATE_IDLE
};

static const char GSM_W_SIGNAL[] PROGMEM = "WEAK";
static const char GSM_F_SIGNAL[] PROGMEM = "FAIR";
static const char GSM_G_SIGNAL[] PROGMEM = "GOOD";
static const char GSM_E_SIGNAL[] PROGMEM = "EXCELLENT";

GSM::GSM():
    _state(GSM_OFF),
    _readyState(0),
    _pin(NULL),
    _timeout(0)
{
}

NetworkStatus GSM::init(const char* pin, bool restart, uint32_t timeout_ms)
{
    if ((restart && !MODEM.restart()) || (!restart && !MODEM.init())) {
        _state = ERROR;
    } else{
        _pin = pin;
        _readyState = READY_STATE_CHECK_SIM;
        _timeout = timeout_ms;

        unsigned long start = millis();
        while (ready() == 0) {
            if (!((millis() - start) < _timeout)) {
                _state = ERROR;
                break;
            }
            delay(100);
        }
    }
    return _state;
}

bool GSM::isAccessAlive()
{
    String response;
    MODEM.send(GF("AT+CREG?"));
    if (MODEM.waitForResponse(100, response) == 1) {
        int status = response.charAt(response.indexOf(',') + 1) - '0';
        if (status == 1 || status == 5) {
            return true;
        }
    }
    return false;
}

bool GSM::shutdown()
{
    if(MODEM.powerOff()){
        _state = GSM_OFF;
        return true;
    }
    return false;
}

uint8_t GSM::ready()
{
    if (_state == ERROR) {
        return 2;
    }

    uint8_t ready = 0;

    switch (_readyState) {
        case READY_STATE_CHECK_SIM: {
            MODEM.send(GF("AT+CPIN?"));
            MODEM.waitForResponse(_timeout, _response); 
            if (_response.indexOf("READY") != -1) {
                _readyState = READY_STATE_SET_PREFERRED_MESSAGE_FORMAT;
                ready = 0;
            } else if (_response.indexOf("SIM PIN") != -1) {
                _readyState = READY_STATE_UNLOCK_SIM;
                ready = 0;
            } else {
                _state = ERROR;
                ready = 2;
            }
        break;
    }

    case READY_STATE_UNLOCK_SIM: {
        if (_pin != NULL) {
            MODEM.sendf("AT+CPIN=\"%s\"", _pin);
            uint8_t res = MODEM.waitForResponse(_timeout, _response); 
            if (res != 1) {
                _state = ERROR;
                ready = 2;
            }
            else{
                _readyState = READY_STATE_SET_PREFERRED_MESSAGE_FORMAT;
                ready = 0;
            }
        } else {
            _state = ERROR;
            ready = 2;
        }
        break;
    }

    case READY_STATE_SET_PREFERRED_MESSAGE_FORMAT: {
        MODEM.send("AT+CMGF=1");
        uint8_t res = MODEM.waitForResponse(_timeout, _response); 
        if (res != 1) {
            _state = ERROR;
            ready = 2;
        }
        else{
            _readyState = READY_STATE_CHECK_REGISTRATION;
            ready = 0;
        }
        break;
    }

    case READY_STATE_CHECK_REGISTRATION: {
        MODEM.send("AT+CREG?");
        uint8_t res = MODEM.waitForResponse(_timeout, _response); 
        if (res != 1) {
            _state = ERROR;
            ready = 2;
        }
        else{
            int status = _response.charAt(_response.indexOf(',') + 1) - '0';
            if (status == 0 || status == 4) {
                _readyState = READY_STATE_CHECK_REGISTRATION;
                ready = 0;
            } else if (status == 1 || status == 5) {
                _readyState = READY_STATE_IDLE;
                _state = GSM_READY;
                ready = 1;
            } else if (status == 2) {
                _readyState = READY_STATE_CHECK_REGISTRATION;
                _state = GSM_CONNECTING;
                ready = 0;
            } else if (status == 3) {
                _state = ERROR;
                ready = 2;
            }
        }
        break;
    }
    case READY_STATE_IDLE:{
        break;
    }
    }
    return ready;
}

/*
unsigned long GSM::getTime() //UTC
{
    String response;

    MODEM.send(F("AT+CCLK?"));
    if (MODEM.waitForResponse(100, &response) != 1) {
        return 0;
    }

    struct tm now;

    if (strptime(response.c_str(), CLOCK_FORMAT, &now) != NULL) {
        // adjust for timezone offset which is +/- in 15 minute increments

        time_t result = mktime(&now);
        time_t delta = ((response.charAt(26) - '0') * 10 + (response.charAt(27) - '0')) * (15 * 60);

        if (response.charAt(25) == '-') {
            result += delta;
        } else if (response.charAt(25) == '+') {
            result -= delta;
        }

        return result;
    }

    return 0;
}


unsigned long GSM::getLocalTime()
{
    String response;

    MODEM.send(F("AT+CCLK?"));
    if (MODEM.waitForResponse(100, &response) != 1) {
        return 0;
    }

    struct tm now;

    if (strptime(response.c_str(), CLOCK_FORMAT, &now) != NULL) {
        time_t result = mktime(&now);
        return result;
    }

    return 0;
}

bool GSM::setLocalTime(time_t time, uint8_t quarters_from_utc){ //time is UTC

    struct tm * now = localtime(&time);
    MODEM.sendf("AT+CCLK=\"%.2d/%.2d/%.2d,%.2d:%.2d:%.2d%+.2d\"",
                (now->tm_year + 1900) % 100, now->tm_mon + 1, now->tm_mday, now->tm_hour, now->tm_min, now->tm_sec % 60, quarters_from_utc);
    return MODEM.waitForResponse() == 1;
}
*/
void GSM::lowPowerMode()
{
    MODEM.lowPowerMode();
}

void GSM::noLowPowerMode()
{
    MODEM.noLowPowerMode();
}

NetworkStatus GSM::status()
{
    return _state;
}

int8_t GSM::getSignalQuality(uint32_t timeout_ms)
{
    MODEM.send(F("AT+CSQ"));
    String response;
    uint8_t result = MODEM.waitForResponse(timeout_ms, response);
    if (result != 1) return 99;
    else{
        return 2*(atoi(response.c_str() + 6)-2) - 109; //result in dBm
    }
}

const char* GSM::signal2String(int8_t signalQuality)
{
    if(signalQuality < -100){
        return GSM_W_SIGNAL;
    }
    else if(signalQuality <= -90){
        return GSM_F_SIGNAL;
    }
    else if(signalQuality <= -60){
        return GSM_G_SIGNAL;
    }
    else
        return GSM_E_SIGNAL;
}

bool GSM::waitForNetwork(uint32_t timeout_ms, int8_t* signal)
{
    for (unsigned long start = millis(); millis() - start < timeout_ms;) {
        if (isAccessAlive()){
            if (signal != NULL){
                *signal = getSignalQuality();
            }
            return true;
        }
        delay(250);
    }
    return false;
}
