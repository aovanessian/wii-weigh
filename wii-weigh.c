#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <linux/input.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifdef SIMULATOR
#include <time.h>
#endif

/* all measurements reported in centigrams */
/* 10kg threshold to determine if someone is on the scale */
#define THRESHOLD 1000
#define SAMPLES 200

static int prolix = 1;

static void debug(const char *msg, int force)
{
	if (prolix || force) {
		puts(msg);
		fflush(stdout);
	}
}

static int partition(int *a, int left, int right, int pivot)
{
	int pivot_value = a[pivot];
	int tmp = a[pivot];
	a[pivot] = a[right];
	a[right] = tmp;

	int store = left;
	for (int i = left; i < right; i++) {
		if (a[i] < pivot_value) {
			tmp = a[store];
			a[store] = a[i];
			a[i] = tmp;
			store++;
		}
	}

	tmp = a[right];
	a[right] = a[store];
	a[store] = tmp;

	return store;
}

static int median(int *data, int n)
{
	int z = n >> 1;       /* middle index */
	int lo = 0, hi = n-1;

	while (1) {
		int pivot = partition(data, lo, hi, lo + (hi-lo)/2);

		if (pivot == z)
			break;
		if (z < pivot)
			hi = pivot - 1;
		else
			lo = pivot + 1;
	}

	if (n & 1)
		return data[z];   /* odd count: middle element */

	/* even count: find the other middle element */
	int m1 = data[z];       /* upper median */
	int m2 = data[0];       /* init to first element in the array */

	for (int i = 0; i < z; i++) {
		if (data[i] > m2)
			m2 = data[i];  /* largest element among lower half */
	}

	return (m1 + m2) >> 1;
}

#ifndef SIMULATOR

static int find_board_device(char *path, size_t len)
{
	for (int i = 0; i < 64; i++) {
		char dev[64];
		snprintf(dev, sizeof(dev), "/dev/input/event%d", i);

		int fd = open(dev, O_RDONLY | O_NONBLOCK);
		if (fd < 0)
			continue;

		char name[256] = {0};
		if (ioctl(fd, EVIOCGNAME(sizeof(name)), name) >= 0) {
			if (strcmp(name, "Nintendo Wii Remote Balance Board") == 0) {
				strncpy(path, dev, len);
				close(fd);
				return 0;
			}
		}
		close(fd);
	}
	return -1;
}

static int read_measurements(int fd, int *out, int max)
{
	int tl = -1, tr = -1, bl = -1, br = -1;
	int count = 0;

	struct input_event ev;

	for (;;) {
		ssize_t r = read(fd, &ev, sizeof(ev));
		if (r < 0) {
			if (errno == EAGAIN) {
				usleep(1000);
				continue;
			}
			perror("read");
			break;
		}

		if (ev.type == EV_KEY && ev.code == BTN_A) {
			fprintf(stderr, "ERROR: Button pressed, aborting.\n");
			exit(1);
		}

		if (ev.type == EV_ABS) {
			int v = ev.value;
			switch (ev.code) {
			case ABS_HAT1X: tl = v; break;
			case ABS_HAT0X: tr = v; break;
			case ABS_HAT0Y: bl = v; break;
			case ABS_HAT1Y: br = v; break;
			}
		}

		if (ev.type == EV_SYN && ev.code == SYN_REPORT) {
			if (tl >= 0 && tr >= 0 && bl >= 0 && br >= 0) {
				int weight = tl + tr + bl + br;

				if (count == 0 && weight < THRESHOLD)
					goto reset;

				if (count > 0 && weight < THRESHOLD)
					break;

				out[count++] = weight;
				if (count >= max)
					break;
			}
		reset:
			tl = tr = bl = br = -1;
		}
	}

	return count;
}

#else  /* SIMULATOR VERSION */

static int sim_random(int max)
{
	return rand() / (RAND_MAX / max + 1);
}

static int read_measurements(int fd_unused, int *out, int max)
{
	(void)fd_unused;
	int count = 0;
	srand(time(NULL));
	/* simulate light stepping onto board */
	for (int i = 0; i < 15 && count < max; i++)
		out[count++] = sim_random(1000); /* 0–10 kg */

	/* simulate stable readings */
	for (int i = 0; i < 150 && count < max; i++)
		out[count++] = 6200 + sim_random(500); /* 62-67 kg */

	/* simulate user stepping off */
	for (int i = 0; i < 18 && count < max; i++)
		out[count++] = sim_random(1000); /* 0–10 kg */

	return count;
}

static int find_board_device(char *path, size_t len)
{
	(void)path;
	(void)len;
	debug("SIMULATOR: pretending balance board found.", 0);
	return 0;
}

#endif /* SIMULATOR */

int main(int argc, char **argv)
{
	double adjust = 0.0;

	int opt;
	while ((opt = getopt(argc, argv, "a:w")) != -1) {
		switch (opt) {
		case 'a': adjust = atof(optarg); break;
		case 'w': prolix = 0; break;
		default:
			fprintf(stderr, "Usage: %s [-a adjust] [-w]\n", argv[0]);
			exit(1);
		}
	}

	char device[64];
	debug("Waiting for balance board...", 0);
	while (find_board_device(device, sizeof(device)) != 0) {
		usleep(500000);
	}

	int fd = -1;
#ifndef SIMULATOR
	fd = open(device, O_RDONLY | O_NONBLOCK);
	if (fd < 0) {
		perror("open");
		exit(EXIT_FAILURE);
	}
#endif
	debug("\aBalance board found, please step on.", 0);
	int data[SAMPLES];
	int count = read_measurements(fd, data, SAMPLES);
#ifndef SIMULATOR
	close(fd);
#endif

	if (count <= 0) {
		fprintf(stderr, "ERROR: No valid data collected.\n");
		exit(EXIT_FAILURE);
	}

	double result = adjust + median(data, count) / 100.0;

	char buf[8];
	snprintf(buf, sizeof(buf), "%.2f", result);

	if (prolix)
		printf("\aDone, weight: %skg.\n", buf);
	else
		puts(buf);

	exit(EXIT_SUCCESS);
}

