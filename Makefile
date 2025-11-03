# based on makefile in:
# https://github.com/suloku/gcmm/

default: all

# dolphin path or executable
emupath := flatpak run org.DolphinEmu.dolphin-emu

# wiiload via physical USB Gecko
export WIILOAD := /dev/ttyUSB0

# wiiload via network
#export WIILOAD := tcp:10.200.200.150

# this can be any of the run targets
# note that the wii binary won't run on gamecube, and vice versa
# set this to whatever your testing environment is
run: runwii

# this can be debuglog (prints info in a configurable way), or debuggdb (actual remote debugger)
debug: debuglog

clean: gc-clean wii-clean

all: gc wii

wii:
	$(MAKE) -f Makefile.wii

gc:
	$(MAKE) -f Makefile.gc 

runwii:
	$(MAKE) -f Makefile.wii run

rungc:
	$(MAKE) -f Makefile.gc run

runemu:
	$(emupath) -b -e GTS_gc.dol

reloadgc:
	$(MAKE) -f Makefile.gc reload

gc-clean:
	$(MAKE) -f Makefile.gc clean

wii-clean:
	$(MAKE) -f Makefile.wii clean

debuglog:
	$(MAKE) -f Makefile.gc DEBUG=1
	$(MAKE) -f Makefile.wii DEBUG=1

debuggdb:
	$(MAKE) -f Makefile.gc DEBUG=2
	$(MAKE) -f Makefile.wii DEBUG=2

bench:
	$(MAKE) -f Makefile.gc BENCH=1
	$(MAKE) -f Makefile.wii BENCH=1

release:
	$(MAKE) -f Makefile.gc version=$(version)
	$(MAKE) -f Makefile.wii version=$(version)