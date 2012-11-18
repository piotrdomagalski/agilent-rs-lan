RS - LAN translator for Agilent multimeter
==========================================

My project of an RS232 <-> LAN translator for Agilent 34410A digital multimeter.

Designed to work on Propox MMnetSAM7X (AT91SAM7x256 MCU). Tested with Nut/OS 5.0.5 and CodeSourcery
GCC version 4.6.3 (now Mentor Graphics Sourcery Tools).

Basically, it keeps a socket connection with the multimeter and passes the messages between input
UART and the socket connection.
