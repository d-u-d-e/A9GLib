#include "GSM_TCP.h"


GSM_TCP::GSM_TCP():
    _freeIndex(0),
    _available(TCP_BUFFER_MAX)
{

}

//blocking connect
bool GSM_TCP::connect(const char * host, uint16_t port/*, uint8_t* mux*/, unsigned long timeout_s, TCP_NetworkStatus * status)
{
    unsigned long start = millis();
    unsigned long timeout_ms = timeout_s * 1000;

    MODEM.sendf("AT+CIPSTART=\"TCP\",\"%s\",%s", host, String(port).c_str());
    /*
        TODO: if multiple connections are enabled we receive +CIPNUM
        indicating the new channel that was opened

        int index = response.indexOf("+CIPNUM:");
        if (index == -1) return ERROR;
        uint8_t newMux = atoi(response.c_str() + 8);
        *mux = newMux;*/

    uint8_t result = MODEM.waitForResponse(timeout_ms, &_response);
    //this response should contain either "CONNECT OK", "CONNECT FAIL", or "ALREADY CONNECT"

    if (result == -1){
        if(status != NULL)
            *status = TCP_NetworkStatus::TIMEOUT;
        return false;
    }
    else if (result != 1){
        if(status != NULL)
            *status = TCP_NetworkStatus::ERROR;
        return false;
    }

    if(_response.indexOf(CONNECT_OK) != -1){
        if(status != NULL)
            *status = TCP_NetworkStatus::CONNECT_OK;
        MODEM.addUrcHandler(this);
        return true;
    }
    else if(_response.indexOf(CONNECT_FAIL) != -1){
        if(status != NULL)
            *status = TCP_NetworkStatus::CONNECT_FAIL;
        return false;
    }
    else if(_response.indexOf(CONNECT_ALREADY) != -1){
        if(status != NULL)
            *status = TCP_NetworkStatus::CONNECT_ALREADY;
        return true;
    }
    else{
        if(status != NULL)
            *status = TCP_NetworkStatus::ERROR;
        return false;
    }
}

void GSM_TCP::handleUrc(const String & urc)
{		
    //if I read \r\nCLOSED\r\nOK I should notify the client! TODO
    //actually this is a URC which gets trimmed according to the logic implemented, so
    //the only piece retrivied is CLOSED (to be checked)
    if(urc.startsWith("+CIPRCV,")){
        int start = urc.indexOf(":") + 1;
        uint16_t len = atoi(urc.c_str() + 8);
        if (_available < len){
            DBG("#DEBUG# TCP buffer overflow! Discarding new bytes...");
            for(int i = 0; i < _available; i++){
                _buffer[_freeIndex] = urc[start + i];
                _freeIndex = (_freeIndex + 1) % TCP_BUFFER_MAX;
            }
            _available = 0;
        }
        else{
            for(int i = 0; i < len; i++){
                _buffer[_freeIndex] = urc[start + i];
                _freeIndex = (_freeIndex + 1) % TCP_BUFFER_MAX;
            }
            _available -= len;
        }
    }
}

uint8_t GSM_TCP::read(char * buf, uint8_t len, unsigned long timeout) //implement read that returns -1 when other end closes the connection
{
    uint8_t readIndex = (_freeIndex + _available) % TCP_BUFFER_MAX;
    if (TCP_BUFFER_MAX - _available >= len){
        for(int i = 0; i < len; i++){
            buf[i] = _buffer[readIndex];
            readIndex = (readIndex + 1) % TCP_BUFFER_MAX;
        }
        _available += len;
        return len;
    }
    else{
        uint8_t len_r = len;
        for (unsigned long start = millis(); (millis() - start) < timeout && len_r > 0;){
            uint16_t readNow = min(len_r, TCP_BUFFER_MAX - _available);
            for(int i = 0; i < readNow; i++){
                buf[i] = _buffer[readIndex];
                readIndex = (readIndex + 1) % TCP_BUFFER_MAX;
            }
            _available += readNow;
            len_r -= readNow;
            MODEM.poll();
        }
        return len - len_r;
    }
}

bool GSM_TCP::close(unsigned long timeout /*, uint8_t mux*/) //just closes the TCP connection, IP still set
{		
    MODEM.removeUrcHandler(this);
    _available = TCP_BUFFER_MAX;
    _freeIndex = 0;
    MODEM.send(F("AT+CIPCLOSE"));
    return MODEM.waitForResponse(timeout) == 1;
}

uint16_t GSM_TCP::send(const void * buff, size_t len /*, uint8_t mux*/)
{
	String prompt = String(TCP_PROMPT);	
    MODEM.sendf("AT+CIPSEND=%d", (uint16_t) len); //change this if mux is enabled
    int resp = MODEM.waitForResponse(2000L, NULL, &prompt); //wait for the > prompt issued by the modem
    if (resp != 1) return 0;
    MODEM.write(reinterpret_cast<const uint8_t*>(buff), len);
    MODEM.write(0x1A); //tell modem to send
    MODEM.flush();
    if(MODEM.waitForResponse(10000L) != 1) return 0; //wait at least 10 secs, but maybe add a timeout
    return len;
}

TCP_NetworkStatus GSM_TCP::status()
{
    //TODO check connection status +CIPSTATUS
    // check for "CONNECT OK" or for "IP CLOSE", other values are just ignored, not supported
    return TCP_NetworkStatus::ERROR;
}
