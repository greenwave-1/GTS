# FossScope (working title)

An open source alternative to SmashScope.

Currently not feature-complete, but in a workable state.

## Current features:
- ~1000hz polling (can probably go faster but untested)
- Draws waveform over ~3 seconds or when stick stops moving (whichever comes first)
- Input viewer/button tester with melee coordinates
- 2D stick plot with stickplot maps
- Works on GameCube/Wii, and with 480p

## Current issues I can think of right now:
- Measuring the stick requires the user to press A first, investigate how to change this without losing current polling 
method
- Waveform doesn't show C-Stick (yet)
- No way to currently have a continuous waveform, though I have some ideas
- No way to scale the waveform (yet)
- Dashback and pivot logic are implemented but I am unsure of their correctness
- No coordinate map like SmashScope, but should be easy enough to implement
- All drawing is done with printf and modifying the framebuffer directly, investigate if that can be changed
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