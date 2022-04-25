#include "socket.h"

GSM_Socket::GSM_Socket(uint8_t mux):
    _mux(mux),
    _freeIndex(0),
    _available(BUFFER_MAX)
{

}

void GSM_Socket::handleUrc(const String& urc)
{		
    //if I read \r\nCLOSED\r\nOK I should notify the client! TODO
    //actually this is a URC which gets trimmed according to the logic implemented, so
    //the only piece retrivied is CLOSED (to be checked)
    //here we receive a urc of the form: "+CIPRCV,mux,len:" so at position 10 we retrieve the len
    //NOTE THAT if we receive a urc of len > BUFFER_MAX we discard some bytes! make sure not to by making it a bit large
    //a few experiments showed data segment is not so large (not more than 4096 bytes)
    uint16_t len = atoi(urc.c_str() + 10);
    uint8_t start = urc.indexOf(":") + 1;
    if (_available < len){
        DBG("#DEBUG# TCP buffer %d overflow! Discarding new bytes...", _mux);
        for(int i = 0; i < _available; i++){
            _buffer[_freeIndex] = urc[start + i];
            _freeIndex = (_freeIndex + 1) % BUFFER_MAX;
        }
        _available = 0;
    }
    else{
        for(int i = 0; i < len; i++){
            _buffer[_freeIndex] = urc[start + i];
            _freeIndex = (_freeIndex + 1) % BUFFER_MAX;
        }
        _available -= len;
    }
}

uint8_t GSM_Socket::read(char * buf, uint8_t len, unsigned long timeout) //implement read that returns -1 when other end closes the connection
{
    uint8_t readIndex = (_freeIndex + _available) % BUFFER_MAX;
    if (BUFFER_MAX - _available >= len){
        for(int i = 0; i < len; i++){
            buf[i] = _buffer[readIndex];
            readIndex = (readIndex + 1) % BUFFER_MAX;
        }
        _available += len;
        return len;
    }
    else{
        uint8_t len_r = len;
        for (unsigned long start = millis(); (millis() - start) < timeout && len_r > 0;){
            uint16_t readNow = min(len_r, BUFFER_MAX - _available);
            for(int i = 0; i < readNow; i++){
                buf[i] = _buffer[readIndex];
                readIndex = (readIndex + 1) % BUFFER_MAX;
            }
            _available += readNow;
            len_r -= readNow;
            delay(100);
            MODEM.poll();
        }
        return len - len_r;
    }
}

uint16_t GSM_Socket::send(const void * buff, size_t len) 
{
    String prompt = String(TCP_PROMPT);
    MODEM.sendf("AT+CIPSEND=%d,%d", _mux, (uint16_t) len); //change this if mux is enabled
    int resp = MODEM.waitForResponse(2000L, NULL, &prompt); //wait for the > prompt issued by the modem
    if (resp != 1) return 0;
    MODEM.write(reinterpret_cast<const uint8_t*>(buff), len);
    MODEM.write(0x1A); //tell modem to send
    MODEM.flush();
    if(MODEM.waitForResponse(10000L) != 1) return 0; //wait at least 10 secs, but maybe add a timeout
    //should probably change the logic, make echo OFF because at the moment waitForResponse will read
    //also the send buffer (modem echoes everything); another mode of AT+CIPSEND maybe doesn't echo back the buffer! TODO
    return len;
}
