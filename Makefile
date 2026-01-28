# combined makefile for gc and wii targets
# modified from the libogc2 examples

# make sure there aren't any previous rules
.SUFFIXES:

# check for devkitppc
ifeq ($(strip $(DEVKITPPC)),)
	$(error "\$DEVKITPPC environment variable not set.")
endif


# base project name, appended with _GC or _WII, depending on which is being built
TARGET_BASENAME :=	GTS

# folder to put final builds into, if you want
# this will _need_ a trailing forward slash if set
TARGET_DIR :=

# final binary names
TARGET_GC :=		$(TARGET_DIR)$(TARGET_BASENAME)_GC
TARGET_WII :=		$(TARGET_DIR)$(TARGET_BASENAME)_WII

# top level build directory
# subdirectories will be made inside for gc and wii build data
BUILD_BASE :=		build

# project headers
INCLUDES :=			include include/submenu include/util

# project source files
SOURCES :=			source source/submenu source/util

# not used in GTS
DATA :=				data

# textures folder
TEXTURES :=			textures


# the actual variables are built on the second run, once IS_BUILD=1 is set
# some vars use system-specific values, so this seemed to be the most elegant way to do this
# _PRE and _POST are provided, since order often matters for some of these...

# c compiler options
CFLAGS_PRE :=		-g -O2 -Wall -std=c23
CFLAGS_POST =

# utc timestamp for use in code
# defines BUILD_DATE
# format: YYYY-MM-DD_HH:MM:SS
BUILD_DATE =		`date -u '+%F_%H:%M:%S'`
CFLAGS_POST +=		-DBUILD_DATE=\"$(BUILD_DATE)\"

# 8 character git commit id
# defines COMMIT_ID
CFLAGS_POST +=		-DCOMMIT_ID=\"`git log --format=format:%h | head -n 1`\"

# arbitrary version string
# defines VERSION_NUMBER
ifdef VERSION
	CFLAGS_POST += -DVERSION_NUMBER=\"$(VERSION)\"
endif

# flag to enable debug logging
# defines DEBUGLOG
ifdef DEBUGLOGGING
	CFLAGS_POST += -DDEBUGLOG
endif

# flag to enable remote gdb debugging
# defines DEBUGGDB
ifdef REMOTEGDB
	CFLAGS_POST += -DDEBUGGDB
endif

# flag to enable rudimentary performance test (how long from start to finish render)
# defines BENCH
ifdef BENCHMARK
	CFLAGS_POST += -DBENCH
endif

# c++ flags, if any need to be different
CXXFLAGS_PRE :=		$(CFLAGS)
CXXFLAGS_POST :=

# link flags
LDFLAGS_PRE :=		-g
LDFLAGS_POST :=		-Wl,-Map,$(notdir $@).map

# external libraries that we might link
LIBS_PRE =			-lfat
LIBS_POST =			-lm

# remote gdb debugging
# we need to link -ldb for this target
ifdef REMOTEGDB
	LIBS_PRE +=		-ldb
endif

# external libraries folder (self-compiled stuff)
# stuff specific to devkitpro (portlibs) should be specified in LIBDIRS_DKP or LIBDIRS_PKGCONF
# "must be the top level containing include and lib"
# not used in GTS
LIBDIRS_LOCAL :=


# wiiload target
# set based on what you use

# physical USB Gecko
export WIILOAD :=	/dev/ttyUSB0

# network connection
#export WIILOAD :=	tcp:192.168.1.2


#-----------------------------------------------------------------------------------------------------------------------
# OK THIS GETS SLIGHTLY COMPLICATED...
# to handle building in a slightly generic way, two flags are set in the second run of make: IS_BUILD and TARGET_SYSTEM:
# IS_BUILD=1 is where the actual build targets are. this is similar to the ifneq() build dir check in the examples,
#   but more explicit in what's going on. this is also where compile vars such as CFLAGS are properly set.
#   this is needed because some include values from gamecube_rules or wii_rules, which may or may not be the same.
# TARGET_SYSTEM specifies what libogc platform rules to import, and sets any platform-specific varaibles.
#   some systems require a library not present in another, so those can be specified there.

# IS_BUILD=1 - second run of make
# we should be in the build folder, and we provide the actual compile targets for the project
ifeq ($(IS_BUILD),1)

#----------------------------------------
# check the target system:

# we are targeting gamecube
ifeq ($(TARGET_SYSTEM),GC)
	include $(DEVKITPRO)/libogc2/gamecube_rules
	# full path to the final binary
	OUTPUT :=		$(PROJDIR)/$(TARGET_GC)
	# specific build directory for the platform
	# these should be separate unless you _know_ what you're doing
	BUILD_DIR :=	$(BUILD_BASE)/gc
	# any system specific compiler flags
	SYSTEM_LIBS :=	-logc

	# gamecube specific check: if we want to enable debug logging, we need to link -lbba for network sockets to work
	ifdef DEBUGLOGGING
		SYSTEM_LIBS := -lbba $(SYSTEM_LIBS)
	endif


# we are targeting wii
else ifeq ($(TARGET_SYSTEM),WII)
	include $(DEVKITPRO)/libogc2/wii_rules
	# full path to the final binary
	OUTPUT :=		$(PROJDIR)/$(TARGET_WII)
	# specific build directory for the platform
	# these should be separate unless you _know_ what you're doing
	BUILD_DIR :=	$(BUILD_BASE)/wii
	# any system specific compiler flags
	# TODO: see if -lwiiuse and -lbte provide anything useful for gts...
	SYSTEM_LIBS =	-logc


# this shouldn't happen
else
exit_error:
	$(error "Target system is invalid or not present.")
endif

#----------------------------------------

# pkg-config link flags
# example:
# LIBS_PRE += 		$(shell powerpc-eabi-pkg-config --libs freetype2)
# appends:
# -> -L/opt/devkitpro/portlibs/ppc/lib -lfreetype -lbz2 -lpng16 -lz -lbrotlidec -lbrotlicommon -lm
LIBS_PRE +=

# external libraries folder (stuff from devkitpro/extremscorner repos)
# "must be the top level containing include and lib"
# basically, portlibs
#LIBDIRS_DKP := 	$(PORTLIBS)
LIBDIRS_DKP :=

# pkg-config includes
# some packages will have a specific subfolder that needs to be included
# as an example, freetype2 includes $(PORTLIBS)/include/freetype2 and $(PORTLIBS)/include/libpng16
# these would not be included by just including $(PORTLIBS)
# example:
# LIBDIRS_PKGCONF :=		$(shell powerpc-eabi-pkg-config --cflags-only-I freetype2)
# gives:
# -> -I/opt/devkitpro/portlibs/ppc/include/freetype2 -I/opt/devkitpro/portlibs/ppc/include -I/opt/devkitpro/portlibs/ppc/include/libpng16
LIBDIRS_PKGCONF :=

# pkg-config other flags
# stuff like -DSOMEVAR that should be passed
# example:
# CFLAGS_POST +=		$(shell powerpc-eabi-pkg-config --cflags-only-other freetype2)
# appends:
# -> -DWITH_GZFILEOP
CFLAGS_POST +=


export DEPSDIR :=	$(PROJDIR)/$(BUILD_DIR)

# pkgconfig is not in the foreach loop because pkgconfig alraedy links to the needed /include
INCLUDE	+= -I$(PROJDIR)/$(BUILD_DIR) -I$(LIBOGC_INC) $(foreach dir,$(LIBDIRS_DKP),-I$(dir)/include) $(LIBDIRS_PKGCONF)

# these shouldn't be modified
# use _PRE, _POST, and SYSTEM_LIBS above
CFLAGS :=			$(CFLAGS_PRE) $(MACHDEP) $(INCLUDE) $(CFLAGS_POST)
CXXFLAGS :=			$(CFLAGS)
LDFLAGS :=			$(LDFLAGS_PRE) $(MACHDEP) $(LDFLAGS_POST)
LIBS :=				$(LIBS_PRE) $(SYSTEM_LIBS) $(LIBS_POST)

# use CXX for linking C++ projects, CC for standard C
# needs to be in here since we don't include the devkitpro stuff on first run
ifeq ($(strip $(CPPFILES)),)
	export LD	:=	$(CC)
else
	export LD	:=	$(CXX)
endif

# just combine all libdirs for use below
# pkgconfig stuff is not included here because pkgconfig ones might already have directly linked to the needed /lib
LIBDIRS := $(LIBDIRS_LOCAL) $(LIBDIRS_DKP)

# uses LIBOGC_LIB, so needs to be in here
export LIBPATHS	:=	-L$(LIBOGC_LIB) $(foreach dir,$(LIBDIRS),-L$(dir)/lib)

DEPENDS	:=	$(OFILES:.o=.d)

# build targets with dependency chain
# .dol is the thing we want at the end
$(OUTPUT).dol: $(OUTPUT).elf
$(OUTPUT).elf: $(OFILES)

$(OFILES_SOURCES) : $(HFILES)

-include $(DEPENDS)

# textures dependency, stuff in textures/ as defined by textures/*.scf
# .scf is interpreted as .tpl.o due to list conversions below (see SCFFILES TPLFILES and OFILES)
# order of operations is:
# textures.scf -> textures.tpl (SCFFILES->TPLFILES) -> textures.tpl.o (TPLFILES->OFILES_BIN->OFILES)
# OFILES is a requirement for the .elf, which is a requirement for the .dol
# bin2o is defined in base_tools, it calls gxtexconv, bin2s, and cc
%.tpl.o	%_tpl.h :	%.tpl
	@echo $(notdir $<)
	@$(bin2o)

# debug target, just prints variables to confirm correctness
# just set it as a dependency of OFILES to have it print first
#$(OFILES) : echo_vars
echo_vars:
	@echo "-----"
	@echo "Target files/directories:"
	@echo "OFILES: $(OFILES)"
	@echo "HFILES: $(HFILES)"
	@echo "VPATH: $(VPATH)"
	@echo
	@echo "Compiler vars:"
	@echo "CFLAGS: $(CFLAGS)"
	@echo "CXXFLAGS: $(CXXFLAGS)"
	@echo "INCLUDE: $(INCLUDE)"
	@echo "LIBS: $(LIBS)"
	@echo "LIBPATHS: $(LIBPATHS)"
	@echo "LDFLAGS: $(LDFLAGS)"
	@echo "-----"


#-----------------------------------------------------------------------------------------------------------------------
# IS_BUILD not set - first run of make
# build base list of files needed for compilation, and provide (non-compiling) targets
else

export VPATH	:=	$(foreach dir,$(SOURCES),$(CURDIR)/$(dir)) \
			$(foreach dir,$(DATA),$(CURDIR)/$(dir)) \
			$(foreach dir,$(TEXTURES),$(CURDIR)/$(dir))

# automatically build a list of object files for our project
CFILES		:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.c)))
CPPFILES	:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.cpp)))
sFILES		:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.s)))
SFILES		:=	$(foreach dir,$(SOURCES),$(notdir $(wildcard $(dir)/*.S)))
BINFILES	:=	$(foreach dir,$(DATA),$(notdir $(wildcard $(dir)/*.*)))
SCFFILES	:=	$(foreach dir,$(TEXTURES),$(notdir $(wildcard $(dir)/*.scf)))
TPLFILES	:=	$(SCFFILES:.scf=.tpl)

# make .o dependency list
export OFILES_BIN	:=	$(addsuffix .o,$(BINFILES)) $(addsuffix .o,$(TPLFILES))
export OFILES_SOURCES := $(CPPFILES:.cpp=.o) $(CFILES:.c=.o) $(sFILES:.s=.o) $(SFILES:.S=.o)
export OFILES := $(OFILES_BIN) $(OFILES_SOURCES)

export HFILES := $(addsuffix .h,$(subst .,_,$(BINFILES)))

# build a list of include paths
export INCLUDE	=	$(foreach dir,$(INCLUDES),-I$(CURDIR)/$(dir)) \
			$(foreach dir,$(LIBDIRS_LOCAL),-I$(dir)/include)

# pass the root folder of the project to the next run
# needed since we go to another directory (the build directory), but still need to reference stuff here
export PROJDIR := $(CURDIR)

# targets:
.PHONY: clean build_gc build_wii run rungc runwii

# default target
# builds both gc and wii
all: build_gc build_wii

gc: build_gc
build_gc:
	@echo "Building GameCube target."
	@[ -d $@ ] || mkdir -p $(BUILD_BASE)/gc
	@$(MAKE) --no-print-directory -C $(BUILD_BASE)/gc -f $(CURDIR)/Makefile TARGET_SYSTEM=GC IS_BUILD=1
	@echo "Done."

wii: build_wii
build_wii:
	@echo "Building Wii target."
	@[ -d $@ ] || mkdir -p $(BUILD_BASE)/wii
	@$(MAKE) --no-print-directory -C $(BUILD_BASE)/wii -f $(CURDIR)/Makefile TARGET_SYSTEM=WII IS_BUILD=1
	@echo "Done."

# default run target
# specify which you want to be default, it can be either rungc or runwii
run: runwii

# gamecube run target
rungc:
	wiiload $(TARGET_GC).dol

# gamecube run target
runwii:
	wiiload $(TARGET_WII).dol

# clean target
# remove parent build folder and any target binaries in present directory
clean:
	@echo clean ...
	@rm -rf $(BUILD_BASE) $(TARGET_GC).elf $(TARGET_GC).dol $(TARGET_WII).elf $(TARGET_WII).dol


# specific targets
# usually, these set some flag to enable a feature

# gdb target
# enables remote gdb debugging
gdb:
	@echo "Building with remote gdb debugging."
	@$(MAKE) --no-print-directory -f $(CURDIR)/Makefile all REMOTEGDB=1

# bench target
# enables rudimentary performance test (how long from start to finish render)
bench:
	@echo "Building with benchmark enabled."
	@$(MAKE) --no-print-directory -f $(CURDIR)/Makefile all BENCHMARK=1

# debug logging target
# enables debug text output via logging.h to usb gecko, network socket, or file
logging:
	@echo "Building with debug logging enabled."
	@$(MAKE) --no-print-directory -f $(CURDIR)/Makefile all DEBUGLOGGING=1

# there used to be a 'release' target, but that doesn't really make sense in this context
# since you need to provide the string in the original call anyway.
# this is just done manually now with `make VERSION="version string"

# for convenience
log: logging
debug: logging

endif
