#include "socket.h"

GSM_Socket::GSM_Socket(uint8_t mux):
    _mux(mux),
    _freeIndex(0),
    _free(BUFFER_MAX)
{
}

void GSM_Socket::handleUrc(const uint8_t * urc, uint16_t len)
{		
    if (_free < len){
        DBG("#DEBUG# TCP buffer %d overflow! Discarding new bytes, sock ", _mux);
        for(int i = 0; i < _free; i++){
            _buffer[_freeIndex] = urc[i];
            _freeIndex = (_freeIndex + 1) % BUFFER_MAX;
        }
        _free = 0;
    }
    else{
        for(int i = 0; i < len; i++){
            _buffer[_freeIndex] = urc[i];
            _freeIndex = (_freeIndex + 1) % BUFFER_MAX;
        }
        _free -= len;
    }
}

uint16_t GSM_Socket::read(uint8_t * buf, uint16_t len, unsigned long timeout) //TODO implement read that returns -1 when other end closes the connection
{
    uint16_t readIndex = (_freeIndex + _free) % BUFFER_MAX;
    if (BUFFER_MAX - _free >= len){
        for(int i = 0; i < len; i++){
            buf[i] = _buffer[readIndex];
            readIndex = (readIndex + 1) % BUFFER_MAX;
        }
        _free += len;
        return len;
    }
    else{
        uint16_t len_r = len;
        for (unsigned long start = millis(); (millis() - start) < timeout && len_r > 0;){
            uint16_t readNow = min(len_r, BUFFER_MAX - _free);
            for(int i = 0; i < readNow; i++){
                buf[i] = _buffer[readIndex];
                readIndex = (readIndex + 1) % BUFFER_MAX;
            }
            _free += readNow;
            len_r -= readNow;
            delay(100);
            MODEM.waitForUrc(); //let the modem read other expected data from the stream
        }
        return len - len_r;
    }
}

uint16_t GSM_Socket::send(const void * buff, uint16_t len) 
{
    MODEM.sendf("AT+CIPSEND=%d,%d", _mux, (uint16_t) len); //change this if mux is enabled
    int resp = MODEM.waitForResponse(String(PROMPT), 2000L); //wait for the > prompt issued by the modem
    if (resp != 1) return 0;
    MODEM.write(reinterpret_cast<const uint8_t*>(buff), len);
    MODEM.write(0x1A); //tell modem to send
    MODEM.flush();
    if(MODEM.waitForResponse(10000L) != 1) return 0; //wait at least 10 secs, but maybe add a timeout
    return len;
}
