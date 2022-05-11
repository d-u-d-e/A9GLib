#include "socket.h"

GSM_Socket::GSM_Socket(uint8_t mux):
    _mux(mux),
    _freeIndex(0),
    _free(BUFFER_MAX)
{
}

void GSM_Socket::handleUrc(const void* urc, uint16_t len)
{		
    const uint8_t * urcB = reinterpret_cast<const uint8_t*>(urc);
    if (_free < len){
        DBG("#DEBUG# TCP buffer %d overflow! Discarding new bytes, sock ", _mux);
        for(int i = 0; i < _free; i++){
            _buffer[_freeIndex] = urcB[i];
            _freeIndex = (_freeIndex + 1) % BUFFER_MAX;
        }
        _free = 0;
    }
    else{
        for(int i = 0; i < len; i++){
            _buffer[_freeIndex] = urcB[i];
            _freeIndex = (_freeIndex + 1) % BUFFER_MAX;
        }
        _free -= len;
    }
}

uint16_t GSM_Socket::read(void* buf, uint16_t len, unsigned long timeout) //TODO implement read that returns -1 when other end closes the connection
{
    uint8_t* bufB = reinterpret_cast<uint8_t*>(buf);
    uint16_t readIndex = (_freeIndex + _free) % BUFFER_MAX;
    if (BUFFER_MAX - _free >= len){
        for(int i = 0; i < len; i++){
            bufB[i] = _buffer[readIndex];
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
                bufB[i] = _buffer[readIndex];
                readIndex = (readIndex + 1) % BUFFER_MAX;
            }
            _free += readNow;
            len_r -= readNow;
            delay(100);
            MODEM.poll(); //let the modem read other expected data from the stream
        }
        return len - len_r;
    }
}

uint16_t GSM_Socket::send(const void* buff, uint16_t len) 
{
    //String prompt(PROMPT);
    if (!MODEM.turnEcho(false)) return 0;
    MODEM.sendf("AT+CIPSEND=%d,%d", _mux, (uint16_t) len); 
    MODEM._atCommandState = ModemClass::AT_RECV_RESP;
    MODEM.write(reinterpret_cast<const uint8_t*>(buff), len);
    MODEM.write(0x1A); //tell modem to send
    MODEM.flush();
    uint8_t resp = MODEM.waitForResponse(60 * 1000); //OK if successfull; what if fail? TODO
    if (resp != 1) return 0;
    MODEM.turnEcho(true);
    return len;
}
