CC      ?= gcc
CFLAGS  ?= -O3 -Wall -Wextra

.PHONY: all sim strip clean debug

all: strip

wii-weigh: wii-weigh.c
	$(CC) $(CFLAGS) -o $@ $<

sim-weigh: wii-weigh.c
	$(CC) $(CFLAGS) -DSIMULATOR -o $@ $<

sim: sim-weigh

strip: wii-weigh
	strip wii-weigh

debug: CFLAGS = -O0 -g -Wall -Wextra
debug: wii-weigh

clean:
	rm -f wii-weigh sim-weigh
