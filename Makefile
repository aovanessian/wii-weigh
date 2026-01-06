compile:
	gcc -O2 -Wall -Wextra -o wii-weigh wii-weigh.c

sim:
	gcc -O2 -Wall -Wextra -DSIMULATOR -o sim-weigh wii-weigh.c

strip:
	strip wii-weigh

clean:
	rm -f wii-weigh sim-weigh
