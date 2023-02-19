# pico_dazzler
Dazzler graphics for the Altair Duino / Altair Simulator using Raspberry Pi Pico

# What is Pico Dazzler?
This is an implementation of the Cromemco Dazzler graphics hardware for David Hansel's wonderful Altair 8800 simulator / Altair Duino, 
using a Respberry Pi Pico. 
A large amount of credit for this project belongs to David as it borrows heavily from his work and I encourage you to check out his [PIC32 solution](https://www.hackster.io/david-hansel/dazzler-display-for-altair-simulator-3febc6) 

It provides the following features:
* Low cost Dazzler implementation using off-the shelf, easy to obtain components. No soldering required!
* VGA graphics output
* USB Joystikc support for 1 or 2 joysticks / game controllers.
* Stereo line-out audio
* A small package that fits neatly within the Altair Duino

The joysticks can be "swapped" by holding all 4 buttons on one of the controllers for more than 2 seconds. This allows those with a single controller to
use software like AMBUSH.COM which requires the second joystick to play.

# What hardware do I need?
I chose to develop for the board described in [Hardware Design with RP2040](https://datasheets.raspberrypi.com/rp2040/hardware-design-with-rp2040.pdf), a commercial implementation of this design is made by [PIMORONI](https://shop.pimoroni.com/products/pimoroni-pico-vga-demo-base). It's widely available and relatively low cost at around USD $25, the last time I checked.

In addition to the PIMORONI board, you will need:
1. A Raspberry Pi Pico
2. A VGA monitor capable of displaying at 1024x768 resolution.
3. A USB Micro to Type A "On The Go" (OTG) USB adapter.

Optionally for Joysticks you will need:
1. A USB, HID=compliant Analog Joystick / Controller / Gamepad. (Unfortunately XBOX controllers are not HID-compliant devices, but most other USB controllers are)
2. A USB Type A Hub with at least 3 ports

Optionally for Sound:
The board provides line-out audio. I've found it drives earbud-style headphones OK. 
If you want to hook into speakers, you'll need something that can take a line-in audio and ampflify it.

Pictures of my setup will be provided shortly.

# Building from source
Building from source is not necessary, you can used the provided pico_dazzler.uf2 file. But building from source is quite strait forward.
First you must have a working Pico C/C++ build environment. Please refer to [Getting started with Raspberry Pi Pico](https://datasheets.raspberrypi.com/pico/getting-started-with-pico.pdf) or [Pico C++ development using Windows](https://learn.pimoroni.com/article/pico-development-using-wsl).
After you have a confirmed working environment, I'd suggest loading one of the VGA examples from the pico-playground repository to confirm the VGA board is working correctly.

After you build environment is set up, simply:
1. git clone https://github.com/phatchman/pico_dazzler.git
2. cd pico_dazzler
3. mkdir build
4. cd build
5. cmake ..
6. make

If all goes well you will end up with a pico_dazzler.uf2 file which can be loaded onto the Pico via USB.

# Connecting to the Altair Simulator / Altair Duino.

This is a very straight forward process. [Images to come shortly]
Simply connect the pico via the USB OTG cable either to the optional USB hub or directly to the USB Native Port on the Ardiuno Due. (This is the 2nd USB port on the Due. The one that is not normally used by the Altair Duino).<br>
Optionally connect the USB Controllers / Joysick and line-out audio.<br>
Connect the VGA cable to the monitor.<br>
You are done!

The Pico will be powered by the Arduino Due's USB port, in the same way as the PIC32 version. The 4-port USB-3 hub I used passes through the power 
from the USB device port from the Due to the Pico, but I'm not sure if all Hubs have that feature. 

While you can power the whole system via the USB input to the Altair Duino, and I've not had any problems doing this, I'd suggest using the external
power supply, just to be safe on power limits.


# Performance
[ Stats to come ]
For almost all uses, the PICO will provide native-speed performance. However, I've found 2 issues:
1. The USB Host implementation on the Pico cannot read data at full speed. For fast updates to the video memory, the Pico may cause slowdowns. 
I've not found this to be noticeable in any real-world applications.
2. The SOUNDF.COM application can produce audio faster than the 48kHz sampling rate implemented. You will get some occasional pops at the higest frequencies.

# Known Issues
1. The bottom line of the VGA displays "garbage" data. This is likely a bug in the Pico's scanline video SDK, which I've not investigated yet.

# TODO
1. Support XBOX controllers
2. Change the I2S audio PIO assembly to repeat the current sample, rather than needing a new value supplied at 48kHz. This could allow us to support higher sampling rates.
3. Investigate issue with bottom line of VGA displaying "random" data. This is likely due to a bug in the scanline video sdk when scaling to 1024x768.
