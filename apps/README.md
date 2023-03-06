# Dazzler Application Collection

## What's on the disk?

This is a collection of working Dazzler software running under Cromemco CDOS. Why CDOS and not CPM? Well because Cromemco released a lot of Dazzler software under CDOS which is not 100% compatible with CPM, especially the Altair versions of CPM. Running the applications under CDOS provides the most reliable experience and they work as originally intended.

## Highlights
* Working versions of all the Cromemco games and applications
* Music system
* Graphics extensions for 16K Basic!
* Test programs
* XMODEM for CDOS

## Configuring the Altair Simulator

To boot CDOS, you need to configure the ALtair Simulator / Altair-Duino as follows:
*Configure the serial card*
```
(E) Configure serial cards
(1) Configure SIO
SIO board re(v)ision       : Cromemco
```
*Configure the bootable disk*
```
(C) Configure cromemco drive
Disable boot ROM after (b)oot : yes (Required as this bootable disk is configured to use all 64K RAM)
Enable (a)uto-boot            : yes
Drive (0) mounted disk image  : CDISK04.DSK (Or whatever you have named the disk image)
```
*Set processor to Z80*
```
Pro(c)essor                 : Zilog Z80
```
Optionally save the configuration and set the following front-panel switches in last 2 octets (sw 5 - sw 0)
`010 001`
Toggle the AUX1 switch down.<br>

The simulator should then boot into CDOS.
```
[Running Cromemco RDOS 1.0 ROM]

CDOS version 02.58
Cromemco Disk Operating System
Copyright (C) 1977, 1983 Cromemco, Inc.

A.
```
CDOS is similar enough to CPM that you should be able to find your way around. CDOS and various other manuals are included in this repository for your reference.

## Troubleshooting
### Hangs at [Running Cromemco RDOS 1.0 ROM]
1) If you have the Dazzler configured, it must be connected. CDOS initializes the Dazzler at startup if present.
2) Make sure you have a valid CDOS boot disk mounted at Disk 0

### You have a ; prompt
You didn't configure auto-boot. Press B `<Enter>` to boot.




