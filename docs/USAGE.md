# Floopy Drive User Guide

The Floopy Drive doesn't have an SD card or a menu on the Loopy itself, you'll manage your ROMs using a computer.

# Getting Started

You'll need:

* Floopy Drive
* A computer with a USB port
* A USB-C cable (C-to-C or A-to-C, whatever works with your computer)
* Chrome or another browser that [supports WebUSB](https://caniuse.com/webusb)

Plug the Floopy Drive into a free port on your computer. If Chrome is running, you should get a notification that says "RP2040 Zero detected." Click this and you'll get sent to the web interface.

![Notification](notification.png)

This will navigate you to the web interface. You can also get there by navigating to [f.loopy.land](https://f.loopy.land). 

Tap "Connect."

You'll get another Chrome dialog asking to confirm the pairing:

![f.loopy.land wants to connect](wants-to-connect.png)

Select the "RP2040 Zero" and tap "Connect."

The Floopy Drive should now be connected!

## Windows Users

Unfortunately, the default drivers on Windows aren't suitable for WebUSB. If Chrome does not pop up a notification when plugging in the drive, an additional first-time setup step is required.

Please download the latest [Zadig](https://zadig.akeo.ie/).

Connect the Floopy Drive and run Zadig.

![Zadig](zadig.png)

Click "Options > List All Devices" and the device selection pulldown should have a few options. Click the "RP2040 Zero" in this list to select the Floopy Drive.

By default, Zadig should show options to update the driver (most likely from "libusb0") to "WinUSB". Ensure "WinUSB" is selected, and click "Replace Driver." Be patient, installation can take a few seconds.

Once it's done running, Chrome will detect the Floopy Drive right away, no need to unplug the device.

## After Initial Setup

Chrome will remember your pairing, so in the future, all you have to do is plug in the Floopy Drive and tap the "RP2040 Zero detected" notification. That's it!

