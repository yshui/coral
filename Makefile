all: dm

ifeq ($(Release), 1)
EXTRA_CFLAGS?=-O3 -march=native -mtune=native
else
EXTRA_CFLAGS?=-g3 -Og
endif

CFLAGS=-I/usr/include/libdrm -Wall -Wextra -Wno-unused-parameter
LDFLAGS=-ludev -linput -lev -ldrm

dm: main.c Makefile
	gcc $(CFLAGS) $(EXTRA_CFLAGS) $(LDFLAGS) -o $@ $<

clean:
	-rm dm
