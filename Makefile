# based on makefile in:
# https://github.com/suloku/gcmm/

default: all

# dolphin path or executable
emupath := flatpak run org.DolphinEmu.dolphin-emu

# this can be runwii, or runemu
run: runwii

clean: gc-clean wii-clean

all: gc wii

wii:
	$(MAKE) -f Makefile.wii 

gc:
	$(MAKE) -f Makefile.gc 

runwii:
	$(MAKE) -f Makefile.wii run

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