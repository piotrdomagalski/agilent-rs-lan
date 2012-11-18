RS - LAN translator for Agilent multimeter
==========================================

Project of an RS232 <-> LAN translator for [Agilent
34410A](http://www.home.agilent.com/en/pd-692834-pn-34410A/) digital multimeter. There is actually
not much code specific to this product, so it could potentially be used with other SCPI compliant
devices.

Designed to work on [Propox MMnetSAM7X](http://www.propox.com/products/t_208.html) (AT91SAM7x256
MCU) with a custom design baseboard. Tested with [Nut/OS 5.0.5](http://www.ethernut.de/) and GCC
version 4.6.3 from [Sourcery CodeBench
Lite](http://www.mentor.com/embedded-software/sourcery-tools/sourcery-codebench/overview/).
