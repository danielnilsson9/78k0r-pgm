# 78k0r-pgm
Arduino based Renesas 78K0R microcontroller programmer.

**NOTE:**  
This programmer has only been tested on NEC D79F9211 which I suspect is equivalent to Renesas UPD78F1000. I don't know if I have an original NEC MCU or if it's a chinese clone. A few commands such as baudrate change and silicone signature does not return the expected response. Silicon signature has a few extra bytes compared to specification document and the baudrate change command does not seem to work at all causing us to be stuck at 9600 buad.

I do not know if this is compatible with the modern Renesas 78K0R series MCU:s, if not, then minor adjustments may be needed.


## Usage
You will need an Arduino board with > 1 Serial port to run this flashing firmware.
I'm using Arduino Micro which has one physical UART and another virtual port on the USB connection.

The physical port (Serial1) is used for communication with the MCU.
The virtual port (Serial) is used for communication with the computer (flashing tool).

You need to make the following connections to the Renesas MCU.

TOOL0 -> RX & TX of Serial (Connect to both joining them together)  
RESET -> D9  
FLMD  -> D10
GND -> GND
5V -> 5V (Only if Renesas MCU is not powered by other circuit)

You can change the pins in firmware/Config.h if needed.

Initiate flash operation using:
```
78k0r-pgm.exe --com COM7 --input program.hex
```
