##########################################################
#
# $Id:$
#
# $Log:$
#
#
CROSS_COMPILE=arm-linux-gnueabi-


CC=$(CROSS_COMPILE)gcc  # gcc or g++
CFLAGS=-ggdb3 -O2 -Wall -DNORMALUNIX -DLINUX -I../../alsa-lib/build/install/usr/include/ -L../../alsa-lib/build/install/usr/arm-linux-gnueabi/lib/
LDFLAGS=-Wl,--gc-sections
LIBS=-lm -lasound

O=linux

all:	 $(O)/sndserver

clean:
	rm -f *.o *~ *.flc
	rm -f linux/*

# Target
$(O)/sndserver: \
	$(O)/soundsrv.o \
	$(O)/sounds.o \
	$(O)/wadread.o \
	$(O)/linux.o
	$(CC) $(CFLAGS) $(LDFLAGS) \
	$(O)/soundsrv.o \
	$(O)/sounds.o \
	$(O)/wadread.o \
	$(O)/linux.o -o $(O)/sndserver $(LIBS)
	echo make complete.

# Rule
$(O)/%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@


