#include "Drv78K0R.h"
#include <Arduino.h>

#define ETX             0x03
#define SOH             0x01
#define STX             0x02
#define ETB             0x17

#define BLOCK_SIZE      1024

#define TWT_DEFAULT     3000    // Default timeout if nothing else specified

#define TWT1_MAX_BASE   1112    // Chip erase fixed timeout
#define TWT1_MAX_BLOCK  141     // Chip erase timeout per block

#define TWT4_MAX        100     // Write timeout, 47.2 according to doc but doesn't work using slow baudrate
#define TWT5_MAX        860     // Verify timeout per block

#define TFD3            145     // microseconds

static uint8_t checksum(const uint8_t* buf, uint16_t length)
{
	uint8_t value = 0;
	for (uint16_t i = 0; i < length; i++)
	{
		value += buf[i];  
	}  

	value += (uint8_t)length;  
	value = ~value + 1;

	return value;
}


Drv78K0R::Drv78K0R(HardwareSerial& serial, int pinRESET, int pinFLMD)
    : _serial(serial)
    , _pinReset(pinRESET)
    , _pinFlmd(pinFLMD)
{
    memset(_deviceId, 0, 10);

    pinMode(_pinFlmd, OUTPUT);
    pinMode(_pinReset, OUTPUT);

    digitalWrite(_pinFlmd, HIGH);
    digitalWrite(_pinReset, HIGH);
}


Drv78K0R::Result Drv78K0R::begin()
{  
    _serial.end();

    digitalWrite(_pinFlmd, LOW);
	digitalWrite(_pinReset, LOW);  

	delay(10);  
	digitalWrite(_pinFlmd, HIGH);

    _serial.begin(9600);

	delay(10);
	digitalWrite(_pinReset, HIGH);  

	delay(10);  
	_serial.write(0);

	delay(2);  
	_serial.write(0);

    delay(10);

    // reset
    {
        uint8_t cmd[] = { 0x00 };
        sendCmd(cmd, sizeof(cmd));
        readStatusResponse(TWT_DEFAULT);
    }

    delay(13);

    // set baudrate (does not work for some reason?!)
    /*{
        // baudrate 115200
        uint8_t buffer[] = { 0x9a, 0x00, 0x00, 0x0a, 0x01 };
        sendCmd(buffer, sizeof(buffer));

        UART_TOOL0.begin(115200);
        rxClear();

        uint8_t resetCmd[] = { 0x00 };

        uint8_t retry = 16;
        while(retry > 0)
        {
            delayMicroseconds(66);
                
            sendCmd(resetCmd, sizeof(resetCmd));
            if (readStatusResponse(TWT_DEFAULT) != ERROR_TIMEOUT)
            {
                break;
            }

            retry--;
        }

        if (retry == 0)
        {
            return ERROR_TIMEOUT;
        }
    }*/

    delay(10);

    // get signature
    {
        uint8_t cmd[] = { 0xc0 };

        sendCmd(cmd, sizeof(cmd));
        
        return readSiliconSignatureResponse();
    }
}

void Drv78K0R::end()
{
    _serial.end();

    digitalWrite(_pinReset, LOW);
    digitalWrite(_pinFlmd, LOW);

    digitalWrite(_pinReset, HIGH);

    memset(_deviceId, 0, 10);
    _flashSize = 0;
}

uint32_t Drv78K0R::getFlashSize() const
{
    return _flashSize;
}

const char* Drv78K0R::getDeviceId() const
{
    return _deviceId;
}

Drv78K0R::Result Drv78K0R::eraseFlash()
{
    uint8_t cmd[] = { 0x20 };
    sendCmd(cmd, sizeof(cmd));
    return readStatusResponse(TWT1_MAX_BASE + TWT1_MAX_BLOCK * (_flashSize / BLOCK_SIZE));
}

Drv78K0R::Result Drv78K0R::beginWrite(uint32_t startAddress, uint32_t endAddress)
{
    _bytesLeftToWrite = endAddress - startAddress;
    _blockToWrite = _bytesLeftToWrite / 1024;

    uint8_t cmd[] = { 
        0x40, 
        (uint8_t)(startAddress >> 16), 
        (uint8_t)(startAddress >> 8),
        (uint8_t)(startAddress),
        (uint8_t)(endAddress >> 16),
        (uint8_t)(endAddress >> 8),
        (uint8_t)(endAddress)
    };

    sendCmd(cmd, sizeof(cmd));

    return readStatusResponse(TWT_DEFAULT);
}

Drv78K0R::Result Drv78K0R::writeFlash(uint8_t* data, uint16_t len)
{
    if (len > 256)
    {
        return ERROR_INVALID_PARAMETER;
    }

    _bytesLeftToWrite -= len;

    bool isLast = _bytesLeftToWrite <= 0;

    _serial.write(STX);
    _serial.write((uint8_t)len);
    _serial.write(data, len);
	_serial.write(checksum(data, len));
    _serial.write(isLast ? ETX : ETB);

    flush();

    return readStatusResponse2(TWT4_MAX);
}

Drv78K0R::Result Drv78K0R::endWrite()
{
    // read verify data frame
    return readStatusResponse(_blockToWrite * TWT5_MAX);
}


void Drv78K0R::sendCmd(const uint8_t* buffer, uint8_t length)
{
    _serial.write(SOH);
    _serial.write(length);
    _serial.write(buffer, length);
	_serial.write(checksum(buffer, length));
    _serial.write(ETX);

    flush();
}


Drv78K0R::Result Drv78K0R::readStatusResponse(uint32_t timeout_ms)
{
    if (readResponse(timeout_ms) == 5)
    {
        return (Result)_msgbuf[2];
    }

    return ERROR_TIMEOUT;
}

Drv78K0R::Result Drv78K0R::readStatusResponse2(uint32_t timeout_ms)
{
    if (readResponse(timeout_ms) == 6)
    {
        if (_msgbuf[2] != ERROR_NONE)
        {
            return (Result)_msgbuf[2];
        }
        else if (_msgbuf[3] != ERROR_NONE)
        {
            return (Result)_msgbuf[3];
        }

        return ERROR_NONE;
    }

    return ERROR_TIMEOUT;
}

Drv78K0R::Result Drv78K0R::readSiliconSignatureResponse()
{
    Result res = readStatusResponse(TWT_DEFAULT);
    if (res == ERROR_NONE)
    {
        uint8_t len = readResponse(TWT_DEFAULT);

        if (len == 28)
        {
            _flashSize = (((uint32_t)_msgbuf[9]) << 16 | ((uint32_t)_msgbuf[8]) << 8 | _msgbuf[7]) + 1;
            memcpy(_deviceId, &_msgbuf[10], 10);

            return ERROR_NONE;
        }
        else if (len == 31) // 79F9211 response, should have been 28 according to documentation ?!
        {
            _flashSize = (((uint32_t)_msgbuf[10]) << 16 | ((uint32_t)_msgbuf[9]) << 8 | _msgbuf[8]) + 1; 
            memcpy(_deviceId, &_msgbuf[11], 10);

            return ERROR_NONE;
        }
        
        return ERROR_TIMEOUT;
    }
    else
    {
        return res;
    }
}


uint8_t Drv78K0R::readResponse(uint32_t timeout_ms)
{
    uint32_t now = millis();

    uint8_t i = 0;
    uint8_t len = sizeof(_msgbuf);
    bool err = false;
    while(i < (len + 4) && millis() - now < timeout_ms)
    {
        int b = _serial.read();
        
        if (!err && b != -1)
        {
            if (i == 0 && b != STX)
            {
                err = true;
            }
            else if (i == 1)
            {
                len = b;
            }
            else if (i == (len + 2))
            {
                uint8_t ch = checksum(_msgbuf + 2, len);
                if (ch != (uint8_t)b)
                {
                    err = true;
                }
            }
            else if (i == (len + 3))
            {
                if (b != ETX)
                {
                    err = true;
                }
            }

            _msgbuf[i++] = b;
        }
    }

    delayMicroseconds(TFD3);

    if (err || i < (len + 4))
    {
        return 0;
    }

    return i;
}

void Drv78K0R::flush()
{
    // flush tx and discard any data in rx buf, 
    // expecting response from MCU to arrive
    _serial.flush(); 
    while (_serial.available()) _serial.read();
}
