RS - LAN translator for Agilent multimeter
==========================================

My project of an RS232 <-> LAN translator for Agilent 34410A digital multimeter.

Designed to work on [Propox MMnetSAM7X](http://www.propox.com/products/t_208.html) (AT91SAM7x256 MCU).
Tested with [Nut/OS 5.0.5](http://www.ethernut.de/) and GCC version 4.6.3 from
[Sourcery CodeBench Lite](http://www.mentor.com/embedded-software/sourcery-tools/sourcery-codebench/overview/).

Basically, it keeps a socket connection with the multimeter and passes the messages between input
UART and the socket connection.
