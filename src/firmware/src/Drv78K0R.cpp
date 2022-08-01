#include "Drv78K0R.h"

#include <Arduino.h>


#define PIN_RX      0
#define PIN_TX      1

#define ETX         0x03
#define SOH         0x01
#define STX         0x02
#define ETB         0x17


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
     while(Serial1.available()) Serial1.read();
}

static uint8_t checksum(const uint8_t* buf, uint8_t length)
{
	uint8_t value = 0;
	for (int i = 0; i < length; i++)
	{
		value += buf[i];  
	}  

	value += length;  
	value = ~value + 1;

	return value;
}

// static void printHex(uint8_t value)
// {
//      if (value < 16)
//     {
//         SerialUSB.print('0');
//     }
//     SerialUSB.print(value, HEX);
//     SerialUSB.print(' ');
// }



Drv78K0R::Drv78K0R(int pinFLMD, int pinReset)
    : _pinFLMD(pinFLMD)
    , _pinReset(pinReset)
{
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
        readStatusResponse();
    }

    delay(13);

    // set baudrate (does not work for some reason?!)
    /*{
        // baudrate 115200
        uint8_t buffer[] = { 0x9a, 0x00, 0x00, 0x0a, 0x00 };
        sendCmd(buffer, sizeof(buffer));

        delayMicroseconds(100);

        Serial1.begin(115200);
        rxClear();
        
        // reset
        {
            uint8_t buffer1[] = { 0x00 };
            sendCmd(buffer1, sizeof(buffer1));
            readResponse();

            delay(10);

            sendCmd(buffer1, sizeof(buffer1));
            readResponse();

            delay(10);

            sendCmd(buffer1, sizeof(buffer1));
            readResponse();
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

Drv78K0R::Result Drv78K0R::eraseFlash()
{
    // chip erase
    uint8_t cmd[] = { 0x20 };
    sendCmd(cmd, sizeof(cmd));
    return readStatusResponse();
}

Drv78K0R::Result Drv78K0R::beginWrite(uint32_t startAddress, uint32_t len)
{
    _endAddress = startAddress + len;
    _bytesWritten = 0;

    uint8_t cmd[] = { 
        0x40, 
        (uint8_t)(startAddress >> 16), 
        (uint8_t)(startAddress >> 8),
        (uint8_t)startAddress,
        (uint8_t)(_endAddress >> 16),
        (uint8_t)(_endAddress >> 8),
        (uint8_t)_endAddress
    };

    sendCmd(cmd, sizeof(cmd));

    return readStatusResponse();
}

Drv78K0R::Result Drv78K0R::writeFlash(uint8_t* data, uint8_t len)
{
    _bytesWritten += len;

    sendCmd(data, len, _bytesWritten == _endAddress);
    Result2 res = readStatusResponse2();

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
    return readStatusResponse();
}


void Drv78K0R::sendCmd(const uint8_t* buffer, uint8_t length, bool end)
{
    txEnable(true);

    //SerialUSB.print("TX: ");

    Serial1.write(SOH);
    //printHex(SOH);

    Serial1.write(length);
    //printHex(length);

    for (uint8_t i = 0; i < length; i++)
	{
		Serial1.write(buffer[i]);
        //printHex(buffer[i]);
	}
	Serial1.write(checksum(buffer, length));
    //printHex(checksum(buffer, length));

    if (end)
    {
        Serial1.write(ETX);
        //printHex(ETX);
    }
	else
    {
        Serial1.write(ETB);
        //printHex(ETB);
    }

    //SerialUSB.println();

    txEnable(false); // forces tx flush 
    rxClear();
}



Drv78K0R::Result Drv78K0R::readStatusResponse()
{
    if (readResponse() == 5)
    {
        if (_msgbuf[2] != ERROR_NONE)
        {
            //SerialUSB.println("Error: ");
            //printHex(_msgbuf[2]);
            //SerialUSB.println();
        }

        return (Result)_msgbuf[2];
    }

    return ERROR_TIMEOUT;
}

Drv78K0R::Result2 Drv78K0R::readStatusResponse2()
{
    if (readResponse() == 6)
    {
        if (_msgbuf[2] != ERROR_NONE || _msgbuf[3] != ERROR_NONE)
        {
            if (_msgbuf[2] != ERROR_NONE)
            {
                //SerialUSB.println("Error(A): ");
                //printHex(_msgbuf[2]);
            }
            
            if (_msgbuf[3] != ERROR_NONE)
            {
                //SerialUSB.println("Error(B): ");
                //printHex(_msgbuf[3]);
            }
            
            //SerialUSB.println();
        }

        return { (Result)_msgbuf[2], (Result)_msgbuf[3] };
    }

    return { ERROR_TIMEOUT, ERROR_TIMEOUT };
}

Drv78K0R::Result Drv78K0R::readSiliconSignatureResponse()
{
    Result res = readStatusResponse();
    if (res == ERROR_NONE)
    {
        uint8_t len = readResponse();

        if (len == 31)
        {
            // SerialUSB.print("Signature: ");
            // if (_msgbuf[2] == 0x10)
            // {
            //     SerialUSB.print("NEC ");
            // }
            // SerialUSB.write(&_msgbuf[11], 10);

            // uint32_t flashSize = (((uint32_t)_msgbuf[8]) << 16 | ((uint32_t)_msgbuf[9]) << 8 | _msgbuf[10]);
            // SerialUSB.print(" Flash: ");
            // SerialUSB.print(flashSize);
            // SerialUSB.print(" bytes");

            // SerialUSB.println();

            return ERROR_NONE;
        }
        
        return ERROR_TIMEOUT;
    }
    else
    {
        return res;
    }
}


uint8_t Drv78K0R::readResponse()
{
    uint32_t now = millis();

    //SerialUSB.print("RX: ");

    uint8_t i = 0;
    uint8_t len = sizeof(_msgbuf);
    bool err = false;
    while(i < (len + 4) && millis() - now < 3000)
    {
        int b = Serial1.read();
        if (!err && b != -1)
        {
            //printHex(b);

            if (i == 0 && b != STX)
            {
                //SerialUSB.print("Frame does not start with STX");
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
                    //SerialUSB.print("Checksum Error, expected ");
                    //printHex(ch);
                    err = true;
                }
            }
            else if (i == (len + 3))
            {
                if (b != ETX)
                {
                    //SerialUSB.print("Unexpected end of message, expected ETX");
                    err = true;
                }
            }

            _msgbuf[i++] = b;
        }
    }

    //SerialUSB.println();

    delay(1);

    if (err)
    {
        return 0;
    }

    return i;
}