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
    beginSend();
    _uart->println(command);
    _uart->flush();
}

void ModemClass::send(__FlashStringHelper* command){
    beginSend();
    _uart->println(command);
    _uart->flush();
}

void ModemClass::beginSend(){

    if (_lowPowerMode){
        digitalWrite(GSM_LOW_PWR_PIN, HIGH); //turn off low power mode if on
        delay(5);
    }

    /*
    The chain Command -> Response shall always be respected and a new command must not be issued
    before the module has terminated all the sending of its response result code (whatever it may be).
    This applies especially to applications that ?sense? the OK text and therefore may send the next
    command before the complete code <CR><LF>OK<CR><LF> is sent by the module.
    It is advisable anyway to wait for at least 20ms between the end of the reception of the response and
    the issue of the next AT command.
    If the response codes are disabled and therefore the module does not report any response to the
    command, then at least the 20ms pause time shall be respected.
    */

    // compare the time of the last response or URC and ensure
    // at least 20ms have passed before sending a new command

    unsigned long delta = millis() - _lastResponseOrUrcMillis;
    if(delta < MODEM_MIN_RESPONSE_OR_URC_WAIT_TIME_MS){
        delay(MODEM_MIN_RESPONSE_OR_URC_WAIT_TIME_MS - delta);
    }
    _ready = 0;
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
    for (unsigned long start = millis(); (millis() - start) < timeout;){
        if(_uart->available()){
            _buffer += _uart->read();
            if(_buffer.endsWith("\r\n") && _buffer.length() > 2){
                onResponseArrived();
                if(_ready != 0) return _ready;
            }
        }
    }
    //clean up in case timeout occured
    _responseDataStorage = NULL;
    _ready = 1;
    _buffer = ""; //clean buffer in case we got some bytes but didn't complete in time
    return -1;
}

void ModemClass::onResponseArrived()
{
    int responseResultIndex = _buffer.lastIndexOf(GSM_OK);
    if (responseResultIndex != -1){
        _ready = 1;
    }
    else if((responseResultIndex = _buffer.lastIndexOf(GSM_ERROR)) != -1){
        _ready = 2;
    }
    #ifdef GSM_DEBUG
    else if ((responseResultIndex = _buffer.lastIndexOf(GSM_CME_ERROR)) != -1){
        _ready = 3;
    }
    else if ((responseResultIndex = _buffer.lastIndexOf(GSM_CMS_ERROR)) != -1){
        _ready = 4;
    }
    #endif
    
    if (_ready != 0){ //response actually arrived if != 0, otherwise it's just a new line
        _lastResponseOrUrcMillis = millis();
        if (_lowPowerMode){ //after receiving the response, bring back low power mode if it were on
            digitalWrite(GSM_LOW_PWR_PIN, LOW);
        }

        if (_responseDataStorage != NULL){
            #ifdef GSM_DEBUG
            if (_ready < 3){
                String responseCode = _buffer.substring(responseResultIndex + 2, _buffer.length() - 2);
                _buffer.remove(responseResultIndex); //remove result code along with extra spaces
                _buffer.trim();
                DBG("#DEBUG# response received: \"", _buffer, "\", code: \"", responseCode, "\"");
            }
            else{
                _buffer.trim();
                DBG("#DEBUG# response received: \"", _buffer, "\"");
            }
            #else	
            //when debug mode is off, error codes are sent using GSM_ERROR, not using GSM_CME_ERROR or GSM_CSM_ERROR
            _buffer.remove(responseResultIndex); //remove result code along with extra spaces
            _buffer.trim();
            #endif
            *_responseDataStorage = _buffer;
            _responseDataStorage = NULL;
        }
        #ifdef GSM_DEBUG
        else{
            DBG("#DEBUG# response received");
        }
        #endif
        _buffer = ""; //response has been saved!
    }
}

int ModemClass::waitForResponse(String expected, unsigned long timeout)
{
    for (unsigned long start = millis(); (millis() - start) < timeout;){
        if(_uart->available()){
            if(_buffer == expected){
                _ready = 1;
                _buffer = "";
                return _ready;
            }
        }
    }
    //clean up also in case timeout occured
    _ready = 1;
    _buffer = ""; //clean buffer in case we got some bytes but didn't complete in time
    return -1;
}

uint8_t ModemClass::ready()
{
    //updates _ready only when a standard response (OK or ERROR) is received; 
    //not suited in general programming, only for setting up automatic state machines
    //such as GPRS initialization
    while(_uart->available()){
        _buffer += _uart->read();
        if(_buffer.endsWith("\r\n") && _buffer.length() > 2){
            onResponseArrived();
            break;
        }
    }
    return _ready;
}

void ModemClass::waitForUrc() 
{
    while(_uart->available()){
        char c = _uart->read();
        switch(_state){
        case IDLE:
        default:
            _buffer += c;
            if (_buffer.startsWith("+CIPRCV") && _buffer.endsWith(":")){
                _sock = atoi(_buffer.c_str() + 8);
                _chunkLen = atoi(_buffer.c_str() + 10);
                _state = RECV_SOCK_CHUNK;
                _ready = 0;
                _buffer = "";
            }
            else if(c == '\n'){
                _buffer.trim();
                if (_buffer.length() > 0){
                    DBG("#DEBUG# unhandled URC received: ", _buffer);
                    _buffer = "";
                } 
                break;
            }
            break;
        case RECV_SOCK_CHUNK:
            if(--_chunkLen <= 0){
                //done receiving chunk
                _state = IDLE;
                _ready = 1;
            }
            //send to correct socket!
            _sockets[_sock]->handleUrc((uint8_t*)&c, 1);
            break;
        }
    }
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
