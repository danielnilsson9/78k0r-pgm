# 78k0r-pgm
Arduino based Renesas 78K0R microcontroller programmer.

**NOTE:**  
This programmer has only been tested on NEC D79F9211 which I suspect is equivalent to Renesas UPD78F1000. I don't know if I have an original NEC MCU or if it's a chinese clone. A few commands such as baudrate change and silicone signature does not return the expected response. Silicon signature has a few extra bytes compared to specification document and the baudrate change command does not seem to work at all causing us to be stuck at 9600 buad.

I do not know if this is compatible with the modern Renesas 78K0R series MCU:s, if not, then minor adjustments may be needed.


## Usage
You will need an Arduino board with > 1 Serial port to run this flashing firmware.
I'm using Arduino Micro which has one physical UART and another virtual one on the USB connection.

The physical port (Serial1) is used for communication with the MCU to flash.
The virtual port (Serial) is used for communication with the computer (flashing tool).

You need to make three connections to the Renesas MCU.

TOOL0 -> RX & TX of Serial (Connect to both joinig them together)
RESET -> D9
FLMD  -> D10

You can change the pins in the firmware in Config.h if needed.
