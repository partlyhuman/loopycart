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

By default, Zadig should show options to update the driver (most likely from "libusb0") to "WinUSB". Ensure "WinUSB" is selected on the right side, and click "Replace Driver." Be patient, installation can take a few seconds.

Once it's done running, Chrome will detect the Floopy Drive right away, no need to unplug the device.

## After Initial Setup

Chrome will remember your pairing, so in the future, all you have to do is plug in the Floopy Drive and tap the "RP2040 Zero detected" notification. That's it!

## Disconnecting

It should be perfectly safe to pull out the USB after you are done using the Floopy Drive. As long as it's idle, you don't have to close the browser window or hit the disconnect button, but you certainly can.

## Battery

The Floopy Drive uses a CR2032 battery to keep your saves alive. If you are losing saves, please open the Floopy Drive and replace the battery.

(No data are available yet regarding the lifespan of the battery.)

# Using the Floopy Drive

**NOTE** I'll be adding a user-friendly mode to the web interface in the coming days. This will be a drag-and-drop interface with nothing to configure. At that time, the existing interface will be tucked away as the "Advanced mode."

# Advanced Mode

An understanding of the Floopy Drive's internals will help. The Floopy Drive has:

* Flash - this stores the game / ROM. There are 4MB available.
  * It *must* be erased between writes.
  * You can erase the first 2MB, or all 4 MB. So if you're flashing a 2MB game, you don't really need to erase all 4MB.
* SRAM - this stores save games. There are 128KB available. It can be overwritten without erasing.
  * It's a good idea to erase the SRAM when flashing a new game. A few games might get confused if the contents of SRAM are not what it expect.
* The Pico filesystem - the RP2040 has onboard flash that can be used to backup and restore your saves when swapping between games

## Functions

* Inspect - dumps a bunch of very useful information to the log, including excerpts of the RAM and ROM
* ROM / Dump - dumps the entire ROM for verification purposes
* ROM / Erase 2MB - erases the first 2MB of the flash storage. Use this before flashing a 2MB game.
* ROM / Full Erase - erases the first 2MB, then the second 2MB of the flash storage. Use this before flashing a 3MB game.
* ROM / Choose File - either choose a Loopy ROM here, or drag and drop a ROM onto it. See below for more on ROMs. Flashing will begin as soon as you choose a file.
* SRAM / Download - dumps the contents of SRAM, or in other words, downloads your save. Useful for copying saves between carts and/or emulators!
* SRAM / Format - clears every byte of the SRAM. Do this if this is the first time you're playing a game. Not necessary if you're going to be restoring or uploading a save.
* SRAM / Backup - backs up the current game's save file to the Pico flash. Again this is stored on your cart. Do this *before* switching from your existing game to a new game.
* SRAM / Restore - if it can find a backed up save for the current game, loads it from the Pico flash. Do this *after* switching to a new game that you've previously played on your Floopy Drive.
* SRAM / Choose File - either choose a Loopy save file here, or drag and drop a save onto it. See below for more on Saves. Writing will begin as soon as you choose a file.

## Typical Operation

Say you want to change the game on your Floopy Drive from OldGame to NewGame. The typical steps would be:

1. SRAM / Backup
2. Erase 2MB (assuming NewGame is 2MB) or Full Erase (if NewGame is 3MB)
3. ROM / Choose File and select NewGame.bin
4. SRAM / Restore

## ROMS

The preferred ROM set for the Floopy Drive is called "CASIO LOOPY ROMS COMPLETE SET [verified 2023]". These are all confirmed working with the Floopy Drive.

**NOTE** ROMs in big-endian are expected by the Floopy Drive, however, if you use a little-endian ROM, we will try to detect this and swap the bytes for you transparently. So other ROM sets should work too.


## Saves

Saves are tested to be compatible with [Loopy My Seal Emulator](https://github.com/PSI-Rockin/LoopyMSE) and MAME.
