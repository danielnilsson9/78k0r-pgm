# 78k0r-pgm
Arduino based Renesas 78K0R microcontroller programmer.

**NOTE:**  
This programmer has only been tested on NEC D79F9211 which I suspect is equivalent to Renesas UPD78F1000. I don't know if I have an original NEC MCU or if it's a chinese clone. A few commands such as changing baudrate and silicone signature does not return the expected response. Silicon signature has an extra byte compared to specification document and the baudrate change command does not seem to work at all causing us to be stuck at 9600 buad.

I do not know if this is compatible with the modern Renesas 78K0R series MCU:s, if not, then minor adjustments may be needed.
