#include "GPRS.h"

enum {
    GPRS_STATE_IDLE,
    GPRS_STATE_ATTACH,
    GPRS_STATE_SET_PDP_CONTEXT,
    GPRS_STATE_SET_USERNAME_PASSWORD,
    GPRS_STATE_ACTIVATE_IP,
    GPRS_STATE_DEACTIVATE_IP,
    GPRS_STATE_DEATTACH,
};

//this should be a singleton!!!
GPRS::GPRS():
    _apn(NULL),
    _state(GPRS_OFF),
    _username(NULL),
    _password(NULL),
    _timeout(0)
{
}

NetworkStatus GPRS::attachGPRS(const char* apn, const char* user_name, const char* password, uint32_t timeout)
{
    _apn = apn;
    _username = user_name;
    _password = password;
    _timeout = timeout;
    _readyState = GPRS_STATE_ATTACH;
    unsigned long start = millis();
    while (ready() == 0) {
        if (!((millis() - start) < _timeout)) {
            return ERROR;
        }
        delay(100);
    }
    return _state;
}

void GPRS::detachGPRS(bool synchronous)
{
    _readyState = GPRS_STATE_DEACTIVATE_IP;
    if (synchronous) {
        while (ready() == 0) {
            delay(100);
        }
    } else {
        ready();
    }
}

uint8_t GPRS::ready()
{
    if (_state == ERROR) {
        return 2;
    }

    uint8_t ready = 0;
    switch (_readyState) {
    default: {
        ready = 1;
        break;
    }

    case GPRS_STATE_ATTACH: {
        MODEM.send("AT+CGATT=1");
        uint8_t res = MODEM.waitForResponse(_timeout);
        if (res != 1) {
            _state = ERROR;
        } else {
            _readyState = GPRS_STATE_SET_PDP_CONTEXT;
        }
        break;
    }

    case GPRS_STATE_SET_PDP_CONTEXT: {
        MODEM.sendf("AT+CIPMUX=1"); //enable 8 sockets or simultaneous connections
        uint8_t res = MODEM.waitForResponse(_timeout);
        if (res != 1) {
            _state = ERROR;
        } else {
            _readyState = GPRS_STATE_SET_USERNAME_PASSWORD;
        }
        break;
    }

    case GPRS_STATE_SET_USERNAME_PASSWORD: {
        MODEM.sendf("AT+CSTT=\"%s\",\"%s\",\"%s\"", _apn, _username, _password);
        uint8_t res = MODEM.waitForResponse(_timeout);
        if (res != 1) {
            _state = ERROR;
        } else {
            _readyState = GPRS_STATE_ACTIVATE_IP;
        }
        break;
    }

    case GPRS_STATE_ACTIVATE_IP: {
        MODEM.send("AT+CIICR");
        uint8_t res = MODEM.waitForResponse(_timeout);
        if (res != 1) {
            _state = ERROR;
        } else {
            ready = 1;
            _state = GPRS_ON;
            _readyState = GPRS_STATE_IDLE;
        }
        break;
    }

    case GPRS_STATE_DEACTIVATE_IP: {
        MODEM.send("AT+CIPSHUT");
        uint8_t res = MODEM.waitForResponse(_timeout);
        if (res != 1) {
            _state = ERROR;
        } else {
            _readyState = GPRS_STATE_DEATTACH;
        }
        break;
    }

    case GPRS_STATE_DEATTACH: {
        MODEM.send("AT+CGATT=0");
        uint8_t res = MODEM.waitForResponse(_timeout);
        if (res != 1) {
            _state = ERROR;
        } else {
            _readyState = GPRS_STATE_IDLE;
            _state = GPRS_OFF;
            ready = 1;
        }
        break;
    }
    }
    return ready;
}

IPAddress GPRS::getIPAddress()
{
    String response;
    MODEM.send("AT+CIFSR?");
    if (MODEM.waitForResponse(100, response) == 1) {
        response = response.substring(0, response.indexOf("\r")); //remove response code OK
        IPAddress ip;
        if (ip.fromString(response)) {
            return ip;
        }
    }
    return IPAddress(0, 0, 0, 0);
}

NetworkStatus GPRS::status()
{
    return _state;
}

bool GPRS::connect(const char* host, uint16_t port, uint8_t* mux, uint16_t timeout_s, ConnectionStatus* status) 
{
    if(MODEM._initSocks >= MAX_SOCKETS){
        if(status != NULL)
            *status = ConnectionStatus::ERROR;
        return false;
    }

    unsigned long start = millis();
    unsigned long timeout_ms = timeout_s * 1000;
    
    String response;
    MODEM.sendf("AT+CIPSTART=\"TCP\",\"%s\",%s", host, String(port).c_str());
    int result = MODEM.waitForResponse(timeout_ms, response);
    //this response should contain either "CONNECT OK", "CONNECT FAIL", or "ALREADY CONNECT"

    if (result == -1){
        if(status != NULL)
            *status = ConnectionStatus::TIMEOUT;
        return false;
    }
    else if (result != 1){
        if(status != NULL)
            *status = ConnectionStatus::ERROR;
        return false;
    }

    if(response.indexOf(CONNECT_OK) != -1){
        if(status != NULL)
            *status = ConnectionStatus::CONNECT_OK;
        uint8_t newMux = atoi(response.c_str() + 8);
        *mux = newMux;
        MODEM._sockets[newMux] = new GSM_Socket(newMux);
        MODEM._initSocks++;
        return true;
    }
    else if(response.indexOf(CONNECT_FAIL) != -1){
        if(status != NULL)
            *status = ConnectionStatus::CONNECT_FAIL;
        return false;
    }
    else if(response.indexOf(CONNECT_ALREADY) != -1){
        if(status != NULL)
            *status = ConnectionStatus::CONNECT_ALREADY;
        return true;
    }
    else{
        if(status != NULL)
            *status = ConnectionStatus::ERROR;
        return false;
    }
}

bool GPRS::close(uint8_t mux, uint32_t timeout_ms) //just closes the TCP connection
{	
    MODEM.sendf("AT+CIPCLOSE=%d", mux);
    int result = MODEM.waitForResponse(timeout_ms);
    if (result == 1){
        delete MODEM._sockets[mux];
        MODEM._initSocks--;
        return true;
    }
    return false;
}

uint16_t GPRS::send(uint8_t mux, const void* buff, uint16_t len)
{
    return MODEM._sockets[mux]->send(buff, len);
}

uint16_t GPRS::read(uint8_t mux, void* buf, uint16_t len, uint32_t timeout_ms)
{
    return MODEM._sockets[mux]->read(buf, len, timeout_ms);
}
