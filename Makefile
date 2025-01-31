# EMU86 main makefile

CFLAGS = -g -Wall
PREFIX = /usr/local

# EMU86 main program

EMU86_PROG = emu86

EMU86_HDRS = \
	op-common.h \
	op-id-name.h \
	op-class.h \
	emu-mem-io.h \
	emu-proc.h \
	emu-int.h \
	emu-timer.h \
	emu-serial.h \
	op-exec.h \
	# end of list

EMU86_OBJS = \
	op-common.o \
	op-id-name.o \
	op-class.o \
	emu-mem-io.o \
	emu-proc.o \
	emu-int.o \
	emu-serial.o \
	op-exec.o \
	emu-main.o \
	# end of list

# Disassembly style
# att = AT&T syntax (GNU default)
# intel = Intel syntax

STYLE=att
#STYLE=intel

EMU86_OBJS += op-print-$(STYLE).o

# Serial emulation
# console = connected to EMU86 stdin & stdout
# pty = connected to PTY (created by EMU86 as master)

SERIAL=console
#SERIAL=pty

EMU86_OBJS += serial-$(SERIAL).o

# Target selection
# elks = minimal PC to run ELKS
# advtech = Advantech SNMP-1000 SBC

TARGET=elks
#TARGET=advtech
ifeq ($(TARGET), elks)
CFLAGS += -DELKS
endif

EMU86_OBJS += \
	io-$(TARGET).o \
	int-$(TARGET).o \
	timer-$(TARGET).o \
	serial-$(TARGET).o \
	# end of list

# PCAT utility for EMU86 serial port

PCAT_PROG = pcat

PCAT_OBJS = pcat-main.o

# Rules

.PHONY: all install clean

all: $(EMU86_PROG) $(PCAT_PROG)

install: $(EMU86_PROG) $(PCAT_PROG)
	install -m 755 -s $(EMU86_PROG) $(PREFIX)/bin/$(EMU86_PROG)
	install -m 755 -s $(PCAT_PROG) $(PREFIX)/bin/$(PCAT_PROG)

clean:
	rm -f $(EMU86_OBJS) $(EMU86_PROG) $(PCAT_OBJS) $(PCAT_PROG)

$(EMU86_OBJS): $(EMU86_HDRS)

$(EMU86_PROG): $(EMU86_OBJS)
	$(CC) -o $(EMU86_PROG) $(EMU86_OBJS)

$(PCAT_PROG): $(PCAT_OBJS)
	$(CC) -o $(PCAT_PROG) $(PCAT_OBJS)
