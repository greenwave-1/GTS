# GCC Test Suite (GTS)

GCC Test Suite (GTS for short) is an open source GameCube controller tester, and an alternative to SmashScope.

Currently not feature-complete, but in a workable state.

(This project was originally developed under the name FossScope)
## Current features:
- Polls at ~1400 hz in the oscilloscope menus, ~120 hz otherwise
- Two oscilloscope menus, one for testing specific inputs, and one for continuously measuring
- Input viewer and button tester
- Melee coordinate viewer with coordinate overlays
- 2D stick plot with stickplot maps
- Works on GameCube/Wii, and with 480p

## Current issues:
- Polling rate is not perfectly uniform. The min time for polls at ~1400hz is around 600 microseconds,
where as the normal and max are around 700 microseconds
- No way to scale the waveform (yet)
- Tests in the oscilloscope menu are not confirmed to be correct
- Everything on screen is drawn directly on the framebuffer with the CPU, need to move to using GX_ 
functions at some point
- A lot of logic that shouldn't be in menu.c is, needs to be moved to other files (mostly drawing functions)
- Other things I can't think of right now...

## Building:
- Install devkitpro
  - [Follow the instructions for your device from here](https://devkitpro.org/wiki/devkitPro_pacman)
- Install libogc2
  - [Add the extremscorner repo](https://github.com/extremscorner/pacman-packages#readme)
  - NOTE: A recent change to libogc2 [exposed a bug in dolphin](https://github.com/dolphin-emu/dolphin/pull/13650) that 
will cause incorrect behavior. Until this fix is in a release version, dolphin will be
considered an unsupported platform
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
- The PhobGCC team (especially for phobconfigtool and PhobVision, this wouldn't have been done without referencing
it repeatedly). Also thanks for putting up with me in the Discord :)
- SmashScope and its creators for giving me a baseline to strive for
- The DevkitPro team and Extrems for making stuff like this a lot more accessible
- Z. B. Wells for being my "Archaic Language Consultant"
