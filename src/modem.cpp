#include "Modem.h"
#include "socket.h"

ModemClass::ModemClass(Uart& uart, unsigned long baud):
    _uart(&uart),
    _baud(baud),
    _lowPowerMode(false),
    _lastResponseOrUrcMillis(0),
    _init(false),
    _initSocks(0)
{
    _buffer.reserve(64); //reserve 64 chars
}

//Copy this code before calling init()

/*
        pinMode(GSM_PWR_PIN, OUTPUT);
        pinMode(GSM_RST_PIN, OUTPUT);
        pinMode(GSM_LOW_PWR_PIN, OUTPUT);
        digitalWrite(GSM_RST_PIN, LOW);
        digitalWrite(GSM_LOW_PWR_PIN, HIGH);
        digitalWrite(GSM_PWR_PIN, HIGH);

                                //send a pulse to start the module
        digitalWrite(GSM_PWR_PIN, LOW);
        delay(3000);
        digitalWrite(GSM_PWR_PIN, HIGH);

*/

bool ModemClass::init()
{
    if(!_init){
        _uart->begin(_baud > 115200 ? 115200 : _baud);

        send(GF("ATE0"));
        if(waitForResponse() != 1) return false;

        send(GF("ATV1")); //set verbose mode
        if(waitForResponse() != 1) return false;

        if (!autosense()){
            return false;
        }

        #ifdef GSM_DEBUG
        send(GF("AT+CMEE=2"));  // turn on verbose error codes
        #else
        send(GF("AT+CMEE=0"));  // turn off error codes
        #endif
        if(waitForResponse() != 1) return false;

        //TODO
        send(GF("AT+CIPSPRT=0")); //turn off TCP prompt ">" 
        waitForResponse();
        
        //check if baud can be set higher than default 115200

        if (_baud > 115200){
            sendf("AT+IPR=%ld", _baud);
            if (waitForResponse() != 1){
                return false;
            }
            _uart->end();
            delay(100);
            _uart->begin(_baud);

            if (!autosense()){
                return false;
            }
        }
        _init = true;
    }
    return true;
}

bool ModemClass::restart()
{
    if(_init){
        send(GF("AT+RST=1"));
        return (waitForResponse(1000) == 1);
    }
    else{
        init();
    }
}

bool ModemClass::factoryReset()
{
    send(GF("AT&FZ&W"));
    return waitForResponse(1000) == 1;
}

bool ModemClass::powerOff()
{
    if(_init){
        _init = false;
        send(GF("AT+CPOF"));
        uint8_t stat = waitForResponse();
        _uart->end();
        return stat == 1;
    }
    return true;
}

bool ModemClass::autosense(unsigned int timeout)
{
    for (unsigned long start = millis(); (millis() - start) < timeout;){
        if (noop() == 1){
            return true;
        }
        delay(100);
    }
    return false;
}

bool ModemClass::noop()
{
    send(GF("AT"));
    return (waitForResponse() == 1);
}

void ModemClass::flush()
{
    _uart->flush();
}

void ModemClass::lowPowerMode()
{
    _lowPowerMode = true;
    digitalWrite(GSM_LOW_PWR_PIN, LOW);
}

void ModemClass::noLowPowerMode()
{
    _lowPowerMode = false;
    digitalWrite(GSM_LOW_PWR_PIN, HIGH);
}

uint16_t ModemClass::write(uint8_t c)
{
   return _uart->write(c);
}

uint16_t ModemClass::write(const uint8_t* buf, uint16_t size)
{
    return _uart->write(buf, size);
}

void ModemClass::send(GsmConstStr cmd)
{
    if (_lowPowerMode){
        digitalWrite(GSM_LOW_PWR_PIN, HIGH); //turn off low power mode if on
        delay(5);
    }

    // compare the time of the last response or URC and ensure
    // at least 20ms have passed before sending a new command
    unsigned long delta = millis() - _lastResponseOrUrcMillis;
    if(delta < MODEM_MIN_RESPONSE_OR_URC_WAIT_TIME_MS){
        delay(MODEM_MIN_RESPONSE_OR_URC_WAIT_TIME_MS - delta);
    }

    DBG("#DEBUG# command sent: \"", cmd, "\"");
    _uart->println(cmd);
    _uart->flush();
}

void ModemClass::send(const char* cmd)
{
    if (_lowPowerMode){
        digitalWrite(GSM_LOW_PWR_PIN, HIGH); //turn off low power mode if on
        delay(5);
    }

    // compare the time of the last response or URC and ensure
    // at least 20ms have passed before sending a new command
    unsigned long delta = millis() - _lastResponseOrUrcMillis;
    if(delta < MODEM_MIN_RESPONSE_OR_URC_WAIT_TIME_MS){
        delay(MODEM_MIN_RESPONSE_OR_URC_WAIT_TIME_MS - delta);
    }

    DBG("#DEBUG# command sent: \"", cmd, "\"");
    _uart->println(cmd);
    _uart->flush();
}

void ModemClass::sendf(const char *fmt, ...)
{
    char buf[BUFSIZ];
    va_list ap;
    va_start((ap), (fmt));
    vsnprintf(buf, sizeof(buf) - 1, fmt, ap);
    va_end(ap);
    send(buf);
}

uint8_t ModemClass::waitForResponse(uint32_t timeout, String& data, GsmConstStr r1, GsmConstStr r2,
                                   GsmConstStr r3, GsmConstStr r4, GsmConstStr r5)
{
    data.reserve(64);
    unsigned long start = millis();
    uint8_t index = 0;
    do{
        while (_uart->available()) {
            char c = _uart->read();
            data += c;
            if (r1 && data.endsWith(r1)){
                index = 1;
                goto finish;
            }
            else if (r2 && data.endsWith(r2)){
                index = 2;
                goto finish;
            }
            else if (r3 && data.endsWith(r3)){
                #ifdef GSM_DEBUG
                if (r3 == GFP(GSM_CME_ERROR))
                    streamSkipUntil('\n', &data);
                #endif
                index = 3;
                goto finish;
            }
            else if (r4 && data.endsWith(r4)){
                index = 4; 
                goto finish;
            }
            else if (r5 && data.endsWith(r5)){
                index = 5; 
                goto finish;
            }
        }
    } while (millis() - start < timeout);

    finish:
    if (!index) {
        DBG("#DEBUG# response timeout!");
        data.trim();
        if (data.length()) { DBG("#DEBUG# unhandled data: \"", data, "\"");}
        data = "";
    }
    else{
        _lastResponseOrUrcMillis = millis();
        if (_lowPowerMode){ //after receiving the response, bring back low power mode if it were on
            digitalWrite(GSM_LOW_PWR_PIN, LOW);
        }
        data.trim();
        //TODO to print better results, maybe replace all occuring \n, \r with \\n \\r
        DBG("#DEBUG# response received:", "\"", data, "\""); //could contain trash other than our expected response TODO
        //such as URC or other data
    }
    return index;
}

uint8_t ModemClass::waitForResponse(uint32_t timeout, GsmConstStr r1, GsmConstStr r2, 
                                   GsmConstStr r3, GsmConstStr r4, GsmConstStr r5)
{
    //don't care to save response
    String data;
    return waitForResponse(timeout, data, r1, r2, r3, r4, r5);
}

void ModemClass::poll()
{
    //make the modem poll new data, which is not an expected response
    while(_uart->available()){
        char c = _uart->read();
        switch(_urcState){
            defaut:
            case URC_IDLE:{
                _buffer += c;
                checkUrc();
                break;
            }
            case URC_RECV_SOCK_CHUNK:{
                //send to correct socket!
                _sockets[_sock]->handleUrc(&c, 1);
                if(--_chunkLen <= 0){
                    //done receiving chunk
                    _lastResponseOrUrcMillis = millis();
                    _urcState = URC_IDLE;
                    bool skip = streamSkipUntil('\n');
                    #ifdef GSM_DEBUG
                    if (!skip){
                        DBG("#DEBUG# TCP missing END mark!");
                    }
                    #endif
                }
                break;
            }
        } //end switch
    } //end while
}

void ModemClass::checkUrc()
{
    //############################################################################ +CIPRCV
    if (_buffer.endsWith("+CIPRCV,")){
        _sock = streamGetIntBefore(',');
        _chunkLen = streamGetIntBefore(':');
        _urcState = URC_RECV_SOCK_CHUNK;
        _buffer = "";
    }
    //############################################################################ UNHANDLED
    else if(_buffer.endsWith("\r\n") && _buffer.length() > 2){
        _lastResponseOrUrcMillis = millis();
        #ifdef GSM_DEBUG
        //can get URC not starting with \r\n+ but only with +
        if (_buffer.startsWith("+") || _buffer.startsWith("\r\n+")){
            //DBG("#DEBUG# sent: ", _sent);
            _buffer.trim();
            DBG("#DEBUG# unhandled URC received: \"", _buffer, "\"");
        }
        else {
           _buffer.trim();
           if (_buffer.length())
               DBG("#DEBUG# unhandled data: \"", _buffer, "\"");
        }
        #endif
        _buffer = "";
    }
    //############################################################################
}

inline int16_t ModemClass::streamGetIntBefore(const char& lastChar)
{
    char buf[7];
    int16_t bytesRead = _uart->readBytesUntil(lastChar, buf, 7);
    if (bytesRead && bytesRead < 7) {
        buf[bytesRead] = '\0';
        int16_t res = atoi(buf);
        return res;
    }
    return -999;
}

inline bool ModemClass::streamSkipUntil(const char& c, String* save, const uint32_t timeout_ms)
{
    uint32_t startMillis = millis();
    while (millis() - startMillis < timeout_ms){
        while (_uart->available()){
            char r = _uart->read();
            //DBG("#DEBUG#", r);
            if (save != NULL) *save += r; 
            if (r == c) return true;
        }
    }
    return false;
}

void ModemClass::setBaudRate(unsigned long baud)
{
    _baud = baud;
}

void ModemClass::addUrcHandler(ModemUrcHandler* handler)
{
    for (int i = 0; i < MAX_URC_HANDLERS; i++) {
        if (_urcHandlers[i] == NULL) {
            _urcHandlers[i] = handler;
            break;
        }
    }
}

void ModemClass::removeUrcHandler(ModemUrcHandler* handler)
{
    for (int i = 0; i < MAX_URC_HANDLERS; i++) {
        if (_urcHandlers[i] == handler) {
            _urcHandlers[i] = NULL;
            break;
        }
    }
}

ModemClass MODEM(Serial1, 115200);

uint8_t GSM_PWR_PIN = 9;
uint8_t GSM_RST_PIN = 6;
uint8_t GSM_LOW_PWR_PIN = 5;
