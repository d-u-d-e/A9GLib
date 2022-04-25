#include "GPRS.h"

enum {
    GPRS_STATE_IDLE,

    GPRS_STATE_ATTACH,
    GPRS_STATE_WAIT_ATTACH_RESPONSE,

    GPRS_STATE_SET_PDP_CONTEXT,
    GPRS_STATE_WAIT_SET_PDP_CONTEXT_RESPONSE,

    GPRS_STATE_SET_USERNAME_PASSWORD,
    GPRS_STATE_WAIT_SET_USERNAME_PASSWORD_RESPONSE,

    GPRS_STATE_ACTIVATE_IP,
    GPRS_STATE_WAIT_ACTIVATE_IP_RESPONSE,

    GPRS_STATE_DEACTIVATE_IP,
    GPRS_STATE_WAIT_DEACTIVATE_IP_RESPONSE,

    GPRS_STATE_DEATTACH,
    GPRS_STATE_WAIT_DEATTACH_RESPONSE
};

GPRS::GPRS() :
    _apn(NULL),
    _username(NULL),
    _password(NULL),
    _state(GPRS_OFF),
    _timeout(0)
{
}

NetworkStatus GPRS::attachGPRS(const char* apn, const char* user_name, const char* password, bool synchronous)
{
    _apn = apn;
    _username = user_name;
    _password = password;

    _readyState = GPRS_STATE_ATTACH;
    _state = CONNECTING;

    if (synchronous) {
        unsigned long start = millis();

        while (ready() == 0) {
            if (_timeout && !((millis() - start) < _timeout)) {
                _state = ERROR;
                break;
            }
            delay(100);
        }
    } else {
        ready();
    }

    return _state;
}

NetworkStatus GPRS::detachGPRS(bool synchronous)
{
    _readyState = GPRS_STATE_DEACTIVATE_IP;

    if (synchronous) {
        while (ready() == 0) {
            delay(100);
        }
    } else {
        ready();
    }

    return _state;
}

uint8_t GPRS::ready()
{
    uint8_t ready = MODEM.ready();

    if (ready == 0) {
        return 0;
    }

    switch (_readyState) {
    case GPRS_STATE_IDLE:
    default: {
        break;
    }

    case GPRS_STATE_ATTACH: {
        MODEM.send("AT+CGATT=1");
        _readyState = GPRS_STATE_WAIT_ATTACH_RESPONSE;
        ready = 0;
        break;
    }

    case GPRS_STATE_WAIT_ATTACH_RESPONSE: {
        if (ready > 1) {
            _readyState = GPRS_STATE_IDLE;
            _state = ERROR;
        } else {
            _readyState = GPRS_STATE_SET_PDP_CONTEXT;
            ready = 0;
        }
        break;
    }
    case GPRS_STATE_SET_PDP_CONTEXT: {
        MODEM.sendf("AT+CGDCONT=1,\"IP\",\"%s\"", _apn);
        _readyState = GPRS_STATE_WAIT_SET_PDP_CONTEXT_RESPONSE;
        ready = 0;
        break;
    }

    case GPRS_STATE_WAIT_SET_PDP_CONTEXT_RESPONSE: {
        if (ready > 1) {
            _readyState = GPRS_STATE_IDLE;
            _state = ERROR;
        } else {
            _readyState = GPRS_STATE_SET_USERNAME_PASSWORD;
            ready = 0;
        }
        break;
    }

    case GPRS_STATE_SET_USERNAME_PASSWORD: {
        MODEM.sendf("AT+CSTT=\"%s\",\"%s\",\"%s\"", _apn, _username, _password);
        _readyState = GPRS_STATE_WAIT_SET_USERNAME_PASSWORD_RESPONSE;
        ready = 0;
        break;
    }

    case GPRS_STATE_WAIT_SET_USERNAME_PASSWORD_RESPONSE: {
        if (ready > 1) {
            _readyState = GPRS_STATE_IDLE;
            _state = ERROR;
        } else {
            _readyState = GPRS_STATE_ACTIVATE_IP;
            ready = 0;
        }
        break;
    }

    case GPRS_STATE_ACTIVATE_IP: {
        MODEM.send("AT+CGACT=1,1");
        _readyState = GPRS_STATE_WAIT_ACTIVATE_IP_RESPONSE;
        ready = 0;
        break;
    }

    case GPRS_STATE_WAIT_ACTIVATE_IP_RESPONSE: {
        _readyState = GPRS_STATE_IDLE;
        if (ready > 1) {
            _state = ERROR;
        } else {
            _state = GPRS_READY;
        }
        break;
    }

    case GPRS_STATE_DEACTIVATE_IP: {
        MODEM.send("AT+CGACT=0,1");
        _readyState = GPRS_STATE_WAIT_DEACTIVATE_IP_RESPONSE;
        ready = 0;
        break;
    }

    case GPRS_STATE_WAIT_DEACTIVATE_IP_RESPONSE: {
        if (ready > 1) {
            _readyState = GPRS_STATE_IDLE;
            _state = ERROR;
        } else {
            _readyState = GPRS_STATE_DEATTACH;
            ready = 0;
        }
        break;
    }

    case GPRS_STATE_DEATTACH: {
        MODEM.send("AT+CGATT=0");
        _readyState = GPRS_STATE_WAIT_DEATTACH_RESPONSE;
        ready = 0;
        break;
    }

    case GPRS_STATE_WAIT_DEATTACH_RESPONSE: {
        _readyState = GPRS_STATE_IDLE;
        if (ready > 1) {
            _state = ERROR;
        } else {
            _state = GPRS_OFF;
        }
        break;
    }
    }

    return ready;
}

IPAddress GPRS::getIPAddress()
{
    String response;
    MODEM.send("AT+CGPADDR=1");
    if (MODEM.waitForResponse(100, &response) == 1) {
        if (response.startsWith("+CGPADDR: 1,\"") && response.endsWith("\"")) {
            response.remove(response.length() - 1);
            response.remove(0, 13);
            IPAddress ip;
            if (ip.fromString(response)) {
                return ip;
            }
        }
    }
    return IPAddress(0, 0, 0, 0);
}

void GPRS::setTimeout(unsigned long timeout)
{
    _timeout = timeout;
}

NetworkStatus GPRS::status()
{
    return _state;
}
