#include "Drv78K0R.h"

#include <Arduino.h>


#define PIN_RX          0
#define PIN_TX          1

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

#ifndef cbi
#define cbi(sfr, bit) (_SFR_BYTE(sfr) &= ~_BV(bit))
#endif
#ifndef sbi
#define sbi(sfr, bit) (_SFR_BYTE(sfr) |= _BV(bit))
#endif


static void txEnable(bool enable)
{
    if (enable)
    {
        pinMode(PIN_TX, OUTPUT);
        sbi(UCSR1B, TXEN1);
    }
    else
    {
        Serial1.flush();
        cbi(UCSR1B, TXEN1);
        pinMode(PIN_TX, INPUT_PULLUP);
    }
}

static void rxClear()
{
     while (Serial1.available()) Serial1.read();
}

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



Drv78K0R::Drv78K0R(int pinFLMD, int pinReset)
    : _pinFLMD(pinFLMD)
    , _pinReset(pinReset)
{
    memset(_deviceId, 0, 10);

    pinMode(_pinFLMD, OUTPUT);
    pinMode(_pinReset, OUTPUT);
    pinMode(PIN_RX, INPUT);
    pinMode(PIN_TX, INPUT_PULLUP);

    digitalWrite(_pinFLMD, HIGH);
    digitalWrite(_pinReset, HIGH);
}


Drv78K0R::Result Drv78K0R::begin()
{  
    Serial1.end();

    digitalWrite(_pinFLMD, LOW);
	digitalWrite(_pinReset, LOW);  

	delay(10);  
	digitalWrite(_pinFLMD, HIGH);

    Serial1.begin(9600);

	delay(10);
	digitalWrite(_pinReset, HIGH);  

	delay(10);  
	Serial1.write(0);

	delay(2);  
	Serial1.write(0);

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
        uint8_t buffer[] = { 0x9a, 0x00, 0x00, 0x0a, 0x00 };
        sendCmd(buffer, sizeof(buffer));

        delayMicroseconds(TFD3);

        Serial1.begin(115200);
        rxClear();
        
        // reset
        {
            uint8_t buffer1[] = { 0x00 };
            sendCmd(buffer1, sizeof(buffer1));
            readResponse(TWT_DEFAULT);
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
    Serial1.end();

    digitalWrite(_pinReset, LOW);
    digitalWrite(_pinFLMD, LOW);

    digitalWrite(_pinReset, HIGH);
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

    txEnable(true);

    Serial1.write(STX);
    Serial1.write((uint8_t)len);
    Serial1.write(data, len);
	Serial1.write(checksum(data, len));
    Serial1.write(isLast ? ETX : ETB);

    txEnable(false); // forces tx flush 
    rxClear();

    Result2 res = readStatusResponse2(TWT4_MAX);

    if (res.a != ERROR_NONE)
    {
        return res.a;
    }
    else if (res.b != ERROR_NONE)
    {
        return res.b;
    }

    return ERROR_NONE;
}

Drv78K0R::Result Drv78K0R::endWrite()
{
    // read verify data frame
    return readStatusResponse(_blockToWrite * TWT5_MAX);
}


void Drv78K0R::sendCmd(const uint8_t* buffer, uint8_t length)
{
    txEnable(true);

    Serial1.write(SOH);
    Serial1.write(length);
    Serial1.write(buffer, length);
	Serial1.write(checksum(buffer, length));
    Serial1.write(ETX);

    txEnable(false); // forces tx flush 
    rxClear();
}


Drv78K0R::Result Drv78K0R::readStatusResponse(uint32_t timeout_ms)
{
    if (readResponse(timeout_ms) == 5)
    {
        return (Result)_msgbuf[2];
    }

    return ERROR_TIMEOUT;
}

Drv78K0R::Result2 Drv78K0R::readStatusResponse2(uint32_t timeout_ms)
{
    if (readResponse(timeout_ms) == 6)
    {
        return { (Result)_msgbuf[2], (Result)_msgbuf[3] };
    }

    return { ERROR_TIMEOUT, ERROR_TIMEOUT };
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
        int b = Serial1.read();
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