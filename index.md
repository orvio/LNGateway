# LNGateway
LNGateway is an opto isolated DIY interface between Loconet and a serial interface based on an Arduino micro.
It transfers messages from one connection to the other and can be used instead of a LocoBuffer in JMRI.

A PDF of the schematic is available [here](https://orvio.github.io/LNGateway/LNGateway.pdf).
All required parts are THT (through hole technology), so the circuit is easy to solder and can be set up on a bread board for testing. The part list with required amounts and estimated prices is available [here](bom.md).

Programming instructions for the Arduino board are available [here](program.md).

A tutorial video describing the setup of the Raspberry PI is available on Youtube: [https://youtu.be/PdjqzoUwuFU](https://youtu.be/PdjqzoUwuFU).

The main purpose is to provide a gateway for throttle messages between wifi trottles connected to JMRI and a DCC command station.
It understands enough Loconet to block certain message types in order to cut down on traffic on either side of the bridge.

It can use an optional i2c OLED display to show the messages passed through it.

It works in JMRI with the LocoBuffer interface driver.

The TX_PIN must be set to 5 if the circuit is built as documented in the KiCad files. It is compatible with the LocoLinx example sketch from the LocoNet library as long as the TX_PIN is set correctly.

The board design has the mounting holes matching the mounting hole positions of a Raspberry PI B board, so it can easily be stacked on top of a Raspberry PI. However, it can also be used to interface with a notebook or PC. It does not require an additional power supply since the LN side is powered by the LN bus and the Arduino is powered through the USB connection.

Full board:

![Full board](https://orvio.github.io/LNGateway/Images/V1_board_full_800w.JPG)

Board without display and Arduino:

![Board without display and Arduino](https://orvio.github.io/LNGateway/Images/V1_board1_800w.JPG)

Detail view:

![Detail view](https://orvio.github.io/LNGateway/Images/V1_board2_800w.JPG)

Modification to have the V2 circuit on the V1 board:

![Modification to have the V2 circuit on the V1 board](https://orvio.github.io/LNGateway/Images/V1_board3_800w.JPG)
