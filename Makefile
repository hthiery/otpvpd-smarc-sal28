CROSS_COMPILE ?=

CC := $(CROSS_COMPILE)gcc
AR := $(CROSS_COMPILE)ar

ifeq ($(shell test -d .git && echo 1),1)
VERSION := $(shell git describe --abbrev=8 --dirty --always --tags --long)
endif

SOURCES := $(wildcard *.c)
OBJECTS := $(SOURCES:.c=.o)


all: otpvpd

%.o: %.c
	$(CC) -c $< $(CLFAGS) $(EXTRA_CFLAGS)

otpvpd: $(OBJECTS)
	$(CC) $(LDFLAGS) -o $@ $^

clean:
	rm -f $(OBJECTS)
