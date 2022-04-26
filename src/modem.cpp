#include "Modem.h"
#include "socket.h"

ModemClass::ModemClass(Uart& uart, unsigned long baud):
    _uart(&uart),
    _baud(baud),
    _lowPowerMode(false),
    _lastResponseOrUrcMillis(0),
    _init(false),
    _ready(1),
    _responseDataStorage(NULL),
    _initSocks(0),
    _expected(NULL)
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
        send(F("ATV1")); //set verbose mode
        if(waitForResponse() != 1) return false;

        if (!autosense()){
            return false;
        }

        #ifdef GSM_DEBUG
        send(F("AT+CMEE=2"));  // turn on verbose error codes
        #else
        send(F("AT+CMEE=0"));  // turn off error codes
        #endif
        if(waitForResponse() != 1) return false;

        send(F("ATE0")); //set echo off 
        if(waitForResponse() != 1) return false;

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
        send(F("AT+RST=1"));
        return (waitForResponse(1000) == 1);
    }
    else{
        init();
    }
}

bool ModemClass::factoryReset()
{
    send(F("AT&FZ&W"));
    return waitForResponse(1000) == 1;
}

bool ModemClass::powerOff()
{
    if(_init){
        _init = false;
        send(F("AT+CPOF"));
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
    send(F("AT"));
    return (waitForResponse() == 1);
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

void ModemClass::flush(){
    _uart->flush();
}

void ModemClass::send(const char* command)
{
    /* The chain Command -> Response shall always be respected and a new command must not be issued
    before the module has terminated all the sending of its response result code (whatever it may be).
    This applies especially to applications that ?sense? the OK text and therefore may send the next
    command before the complete code <CR><LF>OK<CR><LF> is sent by the module.
    It is advisable anyway to wait for at least 20ms between the end of the reception of the response and
    the issue of the next AT command.
    If the response codes are disabled and therefore the module does not report any response to the
    command, then at least the 20ms pause time shall be respected.
    */

    if (_lowPowerMode){
        digitalWrite(GSM_LOW_PWR_PIN, HIGH); //turn off low power mode if on
        delay(5);
    }

    unsigned long delta = millis() - _lastResponseOrUrcMillis;
    if(delta < MODEM_MIN_RESPONSE_OR_URC_WAIT_TIME_MS){
        delay(MODEM_MIN_RESPONSE_OR_URC_WAIT_TIME_MS - delta);
    }

    _uart->println(command);
    _uart->flush();
}

void ModemClass::send(__FlashStringHelper* command)
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

    _uart->println(command);
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

int ModemClass::waitForResponse(unsigned long timeout, String* responseDataStorage)
{
    _responseDataStorage = responseDataStorage;
    _state = RECV_RESP;
    _ready = 0;
    unsigned long start = millis();
    while ((millis() - start) < timeout){
        uint8_t r = ready();
        if(r != 0) return r;
    }
    //clean up in case timeout occured
    _responseDataStorage = NULL;
    _ready = 1;
    _state = IDLE;
    _buffer = ""; //clean buffer in case we got some bytes but didn't complete in time
    return -1;
}

int ModemClass::waitForResponse(String& expected, unsigned long timeout)
{
    _expected = expected;
    _state = RECV_EXP;
    _ready = 0;
    unsigned long start = millis();
    while ((millis() - start) < timeout){
        uint8_t r = ready();
        if(r != 0) return r;
    }
    //clean up in case timeout occured
    _ready = 1;
    _state = IDLE;
    _buffer = ""; //clean buffer in case we got some bytes but didn't complete in time
    return -1;
}

uint8_t ModemClass::ready()
{
    poll();
    return _ready;
}

void ModemClass::poll()
{
    while(_uart->available()){
        char c = _uart->read();
        switch(_state){
        default:
        //############################################################################
        case IDLE:
            _buffer += c;
            checkUrc();
            break;
        //############################################################################
        case RECV_EXP:
            _buffer += c;
            if (_buffer.endsWith(_expected)){
                _ready = 1;
                _state = IDLE;
                _buffer = "";
                break;
            }
            checkUrc();
            break;
        //############################################################################
        case RECV_RESP:
            _buffer += c;
            int responseResultIndex;
            if (_buffer.endsWith("\r\n")){
                if ((responseResultIndex = _buffer.indexOf(GSM_OK)) != -1){
                    _ready = 1;
                }
                else if ((responseResultIndex = _buffer.indexOf(GSM_ERROR)) != -1){
                    _ready = 2;
                }
                #ifdef GSM_DEBUG
                else if ((responseResultIndex = _buffer.indexOf(GSM_CME_ERROR)) != -1){
                    _ready = 3;
                }
                else if ((responseResultIndex = _buffer.indexOf(GSM_CMS_ERROR)) != -1){
                    _ready = 4;
                }
                #endif
            }
            if (_ready != 0){ 
                _lastResponseOrUrcMillis = millis();
                if (_lowPowerMode){ //after receiving the response, bring back low power mode if it were on
                    digitalWrite(GSM_LOW_PWR_PIN, LOW);
                }
                #ifdef GSM_DEBUG
                String resp = _buffer.substring(responseResultIndex);
                resp.trim();
                DBG("#DEBUG# response received: \"", resp, "\"");
                #endif
                if (_responseDataStorage != NULL){
                    String resp = _buffer.substring(responseResultIndex);
                    resp.trim();
                    *_responseDataStorage = resp; 
                    _responseDataStorage = NULL;
                }
                _buffer = ""; //response has been saved!
                _state = IDLE;
                break;
            }
            checkUrc();
            break;
        //############################################################################
        case RECV_SOCK_CHUNK:
            if(--_chunkLen <= 0){
                //done receiving chunk
                _lastResponseOrUrcMillis = millis();
                _state = _prevState;
                if (_state == IDLE) _ready = 1;
                else _ready = 0;
            }
            //send to correct socket!
            _sockets[_sock]->handleUrc(&c, 1);
            break;
        //############################################################################
        }
    }
}

void ModemClass::checkUrc()
{
    //############################################################################
    if (_buffer.startsWith("+CIPRCV") && _buffer.endsWith(":")){
        _sock = atoi(_buffer.c_str() + 8);
        _chunkLen = atoi(_buffer.c_str() + 10);
        _prevState = _state;
        _state = RECV_SOCK_CHUNK;
        _ready = 0;
        _buffer = "";
    }
    //############################################################################
    else if(_buffer.endsWith("\r\n")){
        _buffer.trim();
        if (_buffer.length() > 0){
            _lastResponseOrUrcMillis = millis();
            DBG("#DEBUG# unhandled URC received: ", _buffer);
            _buffer = "";
        } 
    }
    //############################################################################
}

void ModemClass::setResponseDataStorage(String* responseDataStorage)
{
    _responseDataStorage = responseDataStorage;
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
