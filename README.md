# LocoBridge
LocoBridge is an opto isolated DIY interface between Loconet and a serial interface based on an Arduino micro.

The main purpose is to provide a gateway for throttle messages between wifi trottles connected to JMRI and a DCC command station.
It understands enough Loconet to block certain message types in order to cut down on traffic on either side of the bridge.

It can use a i2c OLED display to show the messages passed through it.

It works in JMRI with the LocoBuffer interface driver.

The TX_PIN must be set to 5 if the circuit is built as documented in the KiCad files. It is compatible with the LocoLinx example sketch from the LocoNet library as long as the TX_PIN is set correctly.
