#
# Makefile for spin compiler
# Copyright (c) 2011-2016 Total Spectrum Software Inc.
# Distributed under the MIT License (see COPYING for details)
#
# if CROSS is defined, we are building a cross compiler
# possible targets are: win32, rpi
# Note that you may have to adjust your compiler names depending on
# which Linux distribution you are using (e.g. ubuntu uses
# "i586-mingw32msvc-gcc" for mingw, whereas Debian uses
# "i686-w64-mingw32-gcc"
#

ifeq ($(CROSS),win32)
#  CC=i586-mingw32msvc-gcc
  CC=i686-w64-mingw32-gcc
  EXT=.exe
  BUILD=./build-win32
else ifeq ($(CROSS),rpi)
  CC=arm-linux-gnueabihf-gcc
  EXT=
  BUILD=./build-rpi
else ifeq ($(CROSS),linux32)
  CC=gcc -m32
  EXT=
  BUILD=./build-linux32
else
  CC=gcc
  EXT=
  BUILD=./build
endif

INC=-I. -I$(BUILD)

#
# WARNING: byacc probably will not work!
#
#YACC = byacc
YACC = bison
CFLAGS = -g -Wall $(INC)
#CFLAGS = -g -Og -Wall -Wc++-compat -Werror $(INC)
LIBS = -lm
RM = rm -f

VPATH=.:util:backends:backends/asm:backends/cpp:backends/dat

HEADERS = $(BUILD)/spin.tab.h

PROGS = $(BUILD)/testlex$(EXT) $(BUILD)/spin2cpp$(EXT)

UTIL = dofmt.c flexbuf.c lltoa_prec.c strupr.c strrev.c

# FIXME lexer should not need cppexpr.c (it belongs in CPPBACK)
LEXSRCS = lexer.c symbol.c ast.c expr.c $(UTIL) preprocess.c cppexpr.c
PASMBACK = outasm.c p1ir.c optimize_ir.c inlineasm.c
CPPBACK = outcpp.c cppfunc.c outgas.c # cppexpr.c
SPINSRCS = $(LEXSRCS) functions.c pasm.c outdat.c $(PASMBACK) $(CPPBACK)

LEXOBJS = $(LEXSRCS:%.c=$(BUILD)/%.o)
SPINOBJS = $(SPINSRCS:%.c=$(BUILD)/%.o)
OBJS = $(SPINOBJS) $(BUILD)/spin.tab.o

all: $(BUILD) $(PROGS)

$(BUILD)/testlex$(EXT): testlex.c $(LEXOBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)

$(BUILD)/spin.tab.c $(BUILD)/spin.tab.h: spin.y
	$(YACC) -t -b $(BUILD)/spin -d spin.y

clean:
	$(RM) $(PROGS) $(BUILD)/*

test: lextest asmtest cpptest errtest runtest

lextest: $(PROGS)
	$(BUILD)/testlex

asmtest: $(PROGS)
	(cd Test; ./asmtests.sh)

cpptest: $(PROGS)
	(cd Test; ./cpptests.sh)

errtest: $(PROGS)
	(cd Test; ./errtests.sh)

runtest: $(PROGS)
	(cd Test; ./runtests.sh)

$(BUILD)/spin2cpp$(EXT): spin2cpp.c $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)

$(BUILD):
	mkdir -p $(BUILD)

$(BUILD)/spin.tab.o: $(BUILD)/spin.tab.c $(HEADERS)
	$(CC) $(CFLAGS) -o $@ -c $<

$(BUILD)/%.o: %.c $(HEADERS)
	$(CC) -MMD -MP $(CFLAGS) -o $@ -c $<

#
# automatic dependencies
# these .d files are generated by the -MMD -MP flags to $(CC) above
# and give us the dependencies
# the "-" sign in front of include says not to give any error or warning
# if a file is not found
#
-include $(SPINOBJS:.o=.d)

#
# targets to build a .zip file for a release
#
spin2cpp.exe: .PHONY
	$(MAKE) CROSS=win32
	cp build-win32/spin2cpp.exe .

spin2cpp.linux: .PHONY
	$(MAKE) CROSS=linux32
	cp build-linux32/spin2cpp ./spin2cpp.linux

zip: spin2cpp.exe spin2cpp.linux
	zip -r spin2cpp_v3.0.1.zip README.md COPYING Changelog.txt docs spin2cpp.exe spin2cpp.linux

#
# target to build a windows spincvt GUI
#
FREEWRAP=/opt/freewrap/linux64/freewrap
FREEWRAPEXE=/opt/freewrap/win32/freewrap.exe

spincvt.zip: .PHONY
	rm -f spincvt.zip
	rm -rf spincvt
	$(MAKE) CROSS=win32
	mkdir -p spincvt/bin
	cp build-win32/spin2cpp.exe spincvt/bin
	cp spinconvert/spinconvert.tcl spincvt
	mkdir -p spincvt/examples
	cp -rp spinconvert/examples/*.spin spincvt/examples
	cp -rp spinconvert/examples/*.def spincvt/examples
	cp -rp spinconvert/README.txt COPYING spincvt
	cp -rp docs spincvt
	(cd spincvt; $(FREEWRAP) spinconvert.tcl -w $(FREEWRAPEXE))
	rm spincvt/spinconvert.tcl
	zip -r spincvt.zip spincvt

.PHONY:
