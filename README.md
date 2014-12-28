iMacG5
======

This is an Arduino (Leonardo) project that implements a System Management Controller on a Apple iMac that has been converted to a Hackintosh replacing the original G5 motherboard with an Intel Haswell NUC D54250. See the following for full details of the project:

http://www.tonymacx86.com/completed-mods/139633-kiwis-20-imac-i5.html

The software (and hardware) provide System management function including
 * Management of full power management lifecycle.
 * Control of shutdown of original G5 ATX PSU
 * Control of LCD backlight inverter brightness from Mac Menu widget
 * Control of screen Brightnes via Mac Menu application (via USB serial)
 * Fan Speed Control from internal temperature monitoring
 * Front Panel LED and Power Switch control
This repository contains
 * The full Bill of Materials for the Hardware
 * The source code for Arduino microcontroller
 * The source code for the Brightness Menu on the Mac

This application also has code to support capacative touch sensors as used by MacTester in his excellent iMac creations. Thanks for hs contribution to this project.

