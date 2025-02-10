# FossScope (working title)

An open source alternative to SmashScope.

Currently not feature-complete, but in a workable state.

## Current features:
- Polls at ~1000hz
- Draws waveform over ~6 seconds or when stick stops moving (whichever comes first, depending on mode)
- Input viewer/button tester with melee coordinates
- 2D stick plot with stickplot maps
- Basic Coordinate Viewer
- Works on GameCube/Wii, and with 480p

## Current issues:
- Polling rate is not uniform, there's a double-poll that seems to occur with vblank
- Measuring the stick requires the user to press A first, investigate how to change this without losing current polling 
method
- Waveform doesn't show C-Stick (yet)
- No way to currently have a continuous waveform, though I have some ideas
- No way to scale the waveform (yet)
- Dashback and pivot logic are implemented, but I am unsure of their correctness
- Coordinate viewer is incomplete, I'd like to have a list of common coordinates that can be tested (firefox angles, shield drop, etc...)
- All drawing is done with printf and modifying the framebuffer directly, investigate if that can be changed
- A lot of logic that shouldn't be in menu.c is, needs to be moved to other files, maybe one header per-screen?
- Other things I can't think of right now...

## Why?
When I started making this I didn't realize that an updated SmashScope was supposedly in the works, so I guess this was
more of an experiment for myself. I'll keep updating it if interest is there but if a second SmashScope is incoming
this is probably useless.

## Thanks to:
- The PhobGCC team (especially for phobconfigtool and PhobVision, this wouldn't have been done without referencing
it repeatedly)
- SmashScope and its creators for giving me a baseline to strive for
- Z. B. Wells for being my "Archaic Language Consultant"