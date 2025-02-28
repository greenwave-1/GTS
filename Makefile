# based on makefile in:
# https://github.com/suloku/gcmm/

default: all

# dolphin path or executable
emupath := flatpak run org.DolphinEmu.dolphin-emu

# wiiload device for wii
# this can be the physical usb gecko, or over the network
runwii: export WIILOAD=/dev/ttyUSB0

# wiiload device for gamecube
# does swiss not support the physical usb gecko for wiiload?
rungc: export WIILOAD=tcp:10.20.204.108

# this can be any of the run targets
run: runwii

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
	$(emupath) -b -e fossScope_gc.dol

reloadgc:
	$(MAKE) -f Makefile.gc reload

gc-clean:
	$(MAKE) -f Makefile.gc clean

wii-clean:
	$(MAKE) -f Makefile.wii clean

debug:
	$(MAKE) -f Makefile.gc DEBUG=1
	$(MAKE) -f Makefile.wii DEBUG=1