# GCC Test Suite (GTS)

GCC Test Suite (GTS for short) is an open source GameCube controller tester, and an alternative to SmashScope.

Currently not feature-complete, but in a workable state.

(This project was originally developed under the name FossScope)
## Current features:
- Polls at ~1400 hz in the oscilloscope menus, ~120 hz otherwise.
- Current menu options:
  - Controller Test - Shows the overall state of a controller visually. Also shows origin information.
  - Stick Oscilloscope - Shows short recordings of a stick over time. Includes Melee-specific tests.
  - Continuous Stick Oscilloscope - Continuously records stick inputs over time. Can be "Frozen", and zoomed.
  - Trigger Oscilloscope - Shows analog and digital state of either trigger over time.
  - Coordinate Viewer - Shows stick coordinates on a circle, with Melee-specific coordinate overlays. 
  - 2D Plot - Shows a recording of a stick on a 2d plot of its axis. Shows buttons pressed and frame intervals.
  Includes Melee-specific stickmaps. 
  - Button Timing Viewer - View state of sticks and buttons over time. Analog values have adjustable thresholds.
  Shows timing information in frames.
  - Gate Visualizer - Shows the rough state of a given stick gate. Measured by moving the stick slowly around the gate.
  - Export Data - Exports a recording from certain above menus to csv format. 
- Recordings from some menus can be viewed in other menus. An asterisk will appear next to a menu entry to indicate
this. 
- Works on GameCube and Wii, at 480i and 480p.

## Current issues:
- Polling rate is not perfectly uniform, and can be pushed higher.
- Specific tests need confirmation for correctness.
- Everything is drawn directly on the framebuffer with the CPU, need to move to using GX_ 
functions at some point.
- A lot of logic that shouldn't be in menu.c is, needs to be moved to other files.
- Other things I can't think of right now...

## Usage:
Grab the latest release in [Releases](https://github.com/greenwave-1/GTS/releases).

The two files that are important are `GTS_GC.dol` and `wii.zip`.

`GTS_GC.dol` is for GameCube. Place the file in a location that you can launch it from. 
Having a way to load [Swiss](https://github.com/emukidid/swiss-gc) is preferred.
This file should also be used for Dolphin. 

`wii.zip` is for Wii. Extract the zip to the Wii's SD card, and launch from the Homebrew Channel. 

## Building:
- Install devkitpro
  - [Follow the instructions for your device from here](https://devkitpro.org/wiki/devkitPro_pacman)
- Install libogc2
  - [Add the extremscorner repo](https://github.com/extremscorner/pacman-packages#readme)
- Run ```sudo (dkp-)pacman -S libogc2 libogc2-libdvm-git```
  - The ```(dkp-)``` is for systems that don't normally have pacman. On those systems, any command should use
```dkp-pacman```
  - Systems that normally use pacman should run ```pacman```
- Run ```make``` in the root of the project
- A numbered release can be made with ```make release <version string>```, or using the bash script to also make
a distributable zip file for wii

## Why?
Originally, I wanted a test program that worked on Gamecube, since SmashScope was for Wii only. I got motivation to
pick up the project again after the website for SmashScope went down, and made significant progress since then. 

## Thanks to:
- The PhobGCC team (especially for PhobVision, this wouldn't have been done without referencing
it repeatedly). Also thanks for putting up with me in the Discord :)
- SmashScope and its creators for giving me a baseline to strive for
- The DevkitPro team and Extrems for making stuff like this a lot more accessible
- Z. B. Wells for being my "Archaic Language Consultant"
