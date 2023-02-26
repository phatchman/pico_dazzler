# pico_dazzler
Dazzler graphics for the Altair Duino / Altair Simulator using Raspberry Pi Pico

# What is Pico Dazzler?
This is an implementation of the Cromemco Dazzler graphics hardware for David Hansel's wonderful Altair 8800 simulator, 
using a Raspberry Pi Pico. 
A large amount of credit for this project belongs to David as it borrows heavily from his work and I encourage you to check out his [PIC32 hardware solution](https://www.hackster.io/david-hansel/dazzler-display-for-altair-simulator-3febc6)

The Pico Dazzler is fully compatible with the [Altair-Duino](https://adwaterandstir.com/product/altair-8800-emulator-kit/) and fits nicely within the case.

It provides the following features:
* Low cost Dazzler implementation using off-the shelf, easy to obtain components. No soldering required!
* VGA graphics output
* USB Joystick support for 1 or 2 joysticks / game controllers.
* Stereo line-out audio
* A small package that fits neatly within the Altair Duino

The joysticks can be "swapped" between controller 1 and controller 2 by holding all 4 buttons on one of 
the controllers for more than 2 seconds. This allows those with a single controller to
use software like AMBUSH.COM which requires the second joystick to play.
*Note:* For XBOX controllers you need to hold all 4 buttons for 2 seconds, then cause another input (e.g. move a stick or press another button) while holding the 4 buttons.

# What's New
1. Support for Dual Buffering, which speeds up AMBUSH and BARPLOT display.
2. Support for XBOX controllers
3. Support for USB keyboards
4. Minor fixes and performance improvements

# What hardware do I need?
I chose to develop for the board described in [Hardware Design with RP2040](https://datasheets.raspberrypi.com/rp2040/hardware-design-with-rp2040.pdf), a commercial implementation of this design is made by [PIMORONI](https://shop.pimoroni.com/products/pimoroni-pico-vga-demo-base). It's widely available and relatively low cost at around USD $25 (as of early 2023).

In addition to the PIMORONI board, you will need:
1. A Raspberry Pi Pico.
2. A VGA monitor capable of displaying at 1024x768 resolution.
3. A USB Micro to Type A "On The Go" (OTG) USB adapter.
4. A USB Micro to Type A USB Cable.

Optionally for Joysticks you will need:
1. One or two USB, HID-compliant Analog Joystick / Controller / Gamepad.
2. A USB Type A Hub with at least 3 ports

Optionally for Sound:<br>
The board provides line-out audio. I've found it drives earbud-style headphones fine. 
If you want to hook into speakers, you'll need something that can take a line-in audio and amplify it appropriately.

![My test setup](https://github.com/phatchman/pico_dazzler/blob/main/img/pico_dazzler.jpg)

# Preparing the Altair 8800 Simulator firmware
The firmware shipped with the Altair-Duino doesn't support the Dazzler out of the box. Please follow David's instructions on his [Project Page](https://www.hackster.io/david-hansel/dazzler-display-for-altair-simulator-3febc6) and the [Altair-Duino instructions](https://adwaterandstir.com/install/) for information on how to rebuild the firmware. Take note of the "config.h" options used in building the firmware to make sure you retain your existing functionality.

Make sure you go into the simulator's configuration menu and configure the Dazzler to use the Native USB Port.

I would highly recommend testing with David Hansel's [Windows Client](https://github.com/dhansel/Dazzler/tree/master/Windows) before proceeding to make sure the
Altair firmware is configured correctly.

I've never managed to get Dazzler output to work on the Due's programming port. Instead you must use the Due's Native USB port (the port not normally used by the Altair Duino's console) to connect to Dazzler client.

# Building from source
Building from source is not necessary, you can use the provided pico_dazzler.uf2 file. But you may want to build from source to enable debugging features 
or add support for other game controllers etc.
First you must have a working Pico C/C++ build environment. Please refer to [Getting started with Raspberry Pi Pico](https://datasheets.raspberrypi.com/pico/getting-started-with-pico.pdf) or [Pico C++ development using Windows](https://learn.pimoroni.com/article/pico-development-using-wsl). There's also a new alternative for windows that doesn't need WSL, the [Raspberry Pi Pico Windows Installer](https://www.raspberrypi.com/news/raspberry-pi-pico-windows-installer/)

After you have a confirmed working environment, I'd suggest loading one of the VGA examples from the pico-playground repository to confirm the VGA board is working correctly.

After you build environment is set up, simply:
1. git clone https://github.com/phatchman/pico_dazzler.git
2. cd pico_dazzler
3. mkdir build
4. cd build
5. cmake ..
6. make

If all goes well you will end up with a pico_dazzler.uf2 file which can be loaded onto the Pico via USB.

# Loading the Firmware
Load the pico_dazzler.uf2 file onto the Pico using the method of your choice. Typically this involves:
1) Holding down the BOOT/SEL button while connecting the USB cable
2) Copying the pico_dazzler.uf2 file to the Mass Storage drive associated with the Pico

# Connecting to the Altair Simulator / Altair Duino.

This is a very straight forward process. [Images to come shortly]
Simply connect the pico via the USB OTG cable either to the optional USB hub or directly to the USB Native Port on the Ardiuno Due. (This is the 2nd USB port on the Due. The one that is not normally used by the Altair Duino).<br>
Optionally connect the USB Controllers / Joysick and line-out audio. Note that the audio uses the DAC line-out<br>
Connect the VGA cable to the monitor.<br>
You are done!

The Pico is powered by the Arduino Due's USB port, in the same way as the PIC32 version. The 4-port USB-3 hub I used passes through the power 
from the USB device port from the Due to the Pico, but I'm not sure if all Hubs have that feature. If your hub doesn't, the USB micro port on the PIMORONI board accepts an external 5V power supply. I suggest you build a USB micro power cable and find a nice place to tap into 5 volts.

While you can power the whole system via the USB input to the Altair Duino, and I've not had any problems doing this, I'd suggest using the external
power supply, just to be safe on power limits.

# Supported Game Controllers

The software has been tested with the following controllers:
* PS3 Controller
* XBOX One Elite
* SNES USB Gamepad

I've included support for other XBOX and Playstation controllers, but this has not been tested. I expect them to work, but you never know until you try.
If you need assistance with getting other controllers working, you will need to connect the serial debugging output and Set DEBUG_JOYSTICK=1 and TRACE_JOYSTICK=1 in the CMakeLists.txt file. Log a bug with the debugging output attached and I'll see what can be done.

# Customizing Game Controller Buttons
Most game controllers come with more than 4 buttons, and by default the first 4 buttons listed in the HID Descriptor will be assigned as buttons 1-4.
In case the buttons automatically selected are not to your liking, you add to the controller_skip_buttons struct in parse_descriptor.c
```
struct hid_input_button_skip controller_skip_buttons[] = {0x0268, 12}; /* For PS3, skip first 12 buttons */
```
The first value is the product id (PID) of the USB device, which can be found either by looking at the debug output produced when connecting
the controller to the Pico, or from Windows Device Manager.<br>
The second value is the number of buttons to skip before assigning the 4 buttons. There is no easy way to determine this value outside of trial and error, 
but typically you will want to try increments of 4.

# Test Software
The folks at S100 computers have made a recreation of the Dazzler board, named the [Dazzler II](http://www.s100computers.com/My%20System%20Pages/Dazzler%20II%20Board/Dazzler_II%20Board.htm) for S-100 bus computers. 
There is a wealth of information on the Dazzler available there. At the end of the page is some software that you can use to test out the board.
Especially useful are:
* SOUND.COM and SOUNDF.COM from the SOUND.ZIP download
* ADCTEST.COM
* COLOR.COM

The last 2 programs are also contained on the Dazzler CPM disk shipped with the Altair-Duino.

# Reporting Issues
Before reporting an issue, please test the program on David's [Windows client](https://github.com/dhansel/Dazzler/tree/master/Windows) if possible. A lot of the Dazzler programs either work in unintuitive ways, or have been crudely ported to CPM and may not work with your particular CPM version or configuration.

# Performance

For almost all uses, the PICO will provide native-speed performance. However, I've found 2 issues:
1. The USB Host implementation on the Pico cannot read data at full speed. For fast updates to the video memory, the Pico may cause slowdowns. 
I've not found this to be noticeable, except in BARPLOT, which changes the video mode and video memory adress on each frame.
2. The SOUNDF.COM application can produce audio faster than the 48kHz sampling rate implemented. You will get some occasional pops at the highest frequencies.

## Performance Testing Results
The results below are from a test program that updates the full 2k of video ram with an alternating pattern in a tight loop, as fast as possible. 
This test represents the absolute worst-case scenario.

| Scenario                                   | Average Time    |
| ------------------------------------------ | --------------- |
| Windows Client                             |             19s |
| Pico only reading USB with no processing   |             45s |
| Pico Dazzler with full processing          |             46s |
| GDEMO.COM (Windows Client)                 |            1:50 |
| GDEMO.COM (Pico Dazzler)                   |            1:50 |

As can be seen above, the bottleneck is the USB host receive rate on the Pico.<br>
There doesn't appear to be any software solution to this. The Pico should be more than capable of handling the data rate, and more investigation is necessary. The rest of the Pico Dazzler firmware adds negligible overhead.

In real-world applications it is rare that this speed difference will make any difference as the application spends more time calculating than updating the video ram.

With the latest release I've not seen any slowdowns in any of the applications I've tried. Notably GDEMO.COM, the Dazzler Demo program, runs in exactly the same amount of time as the Windows client.

# Debug Output

To get debug output, you will need to do some soldering. There is space for a 2x3 pin header on the board marked GP21, GP20, -. <br>
You will need one of a 3.3v compatible TTL USB serial-converter, a Raspberry Pi, a second Pico, or some other device capable of reading 3.3v TTL serial output. <br>
The port is set to 112500 8 N 1. And you should connect GP21 to TX, GP20 to RX and - to Ground.

Debug output should not really be necessary, but will be handy if you run into issues with your USB hub or controller.

# Known Issues
1. The top VGA scanline is not displayed and the bottom VGA scanline displays a blank line. This is likely and issue with the Pico VGA scanline implementation.
2. Hot plugging devices does not always work, and in some cases can crash the Pico. 
It is suggested that you have all USB devices connected when powering on the Pico Dazzler.

# TODO
1. ~~~Support XBOX controllers~~~
2. Change the I2S audio PIO assembly to repeat the current sample, rather than needing a new value supplied at a constant 48kHz. This could allow us to support higher sampling rates and eliminate the audio queue overflows at high frequencies.
3. Investigate issue with Pico scanline library requesting 129 scanlines instead of 128. This is causing issues with the missing top scanline (line 1) and blank bottom scanline (line 768)
4. ~~~Support USB keyboard devices~~~
5. ~~~Implement dual-buffering, which might help programs like BARPLOT, which alternates the graphics mode and vram address on each frame.~~~
6. Investigate if USB serial throughput can be improved.