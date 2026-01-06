#define _GNU_SOURCE
#include <errno.h>
#include <fcntl.h>
#include <linux/input.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/wait.h>
#include <unistd.h>

/* all measurements reported in centigrams; 20kg threshold */
#define THRESHOLD 2000
#define SAMPLES 200

static int terse = 0;

static void debug(const char *msg, int force)
{
	if (!terse || force) {
		puts(msg);
		fflush(stdout);
	}
}

void spawn(char *cmd)
{
	char *argv[64];
	int i = 0;
	char *copy = strdup(cmd);
	char *token = strtok(copy, " ");
	while (token && i < 63) {
		argv[i++] = token;
		token = strtok(NULL, " ");
	}
	argv[i] = NULL;

	pid_t pid = fork();
	if (pid < 0) {
		perror("fork");
		exit(EXIT_FAILURE);
	}
	if (pid == 0) {
		execvp(argv[0], argv);
		perror("execvp");
		exit(EXIT_FAILURE);
	} else {
		int status;
		if (waitpid(pid, &status, 0) < 0) {
			perror("waitpid");
			exit(EXIT_FAILURE);
		}
	}
	free(copy);
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

static int quickselect(int *a, int left, int right, int k)
{
	while (1) {
		if (left == right)
			return a[left];

		int pivot = left + (right - left) / 2;
		pivot = partition(a, left, right, pivot);

		if (k == pivot)
			return a[k];
		if (k < pivot)
			right = pivot - 1;
		else
			left = pivot + 1;
	}
}

static int median(int *data, int n)
{
	int z = n >> 1;
	if (n & 1)
		return quickselect(data, 0, n - 1, z);
	n--;
	int m1 = quickselect(data, 0, n, z - 1);
	int m2 = quickselect(data, 0, n, z);
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
	const char *command = NULL;
	const char *disconnect = NULL;

	int opt;
	while ((opt = getopt(argc, argv, "a:c:d:w")) != -1) {
		switch (opt) {
		case 'a': adjust = atof(optarg); break;
		case 'c': command = optarg; break;
		case 'd': disconnect = optarg; break;
		case 'w': terse = 1; break;
		default:
			fprintf(stderr, "Usage: %s [-a adjust] [-c cmd] [-d addr] [-w]\n", argv[0]);
			exit(1);
		}
	}

	char device[64];
	debug("Waiting for balance board...", 0);
	while (find_board_device(device, sizeof(device)) != 0) {
		usleep(500000);
	}

	debug("\aBalance board found, please step on.", 0);

	int fd = -1;
#ifndef SIMULATOR
	fd = open(device, O_RDONLY | O_NONBLOCK);
	if (fd < 0) {
		perror("open");
		exit(EXIT_FAILURE);
	}
#endif

	int data[SAMPLES];
	int n = read_measurements(fd, data, SAMPLES);
#ifndef SIMULATOR
	close(fd);
#endif

	if (n <= 0) {
		fprintf(stderr, "ERROR: No valid data collected.\n");
		exit(EXIT_FAILURE);
	}

	double result = adjust + median(data, n) / 100.0;

	char buf[16];
	snprintf(buf, sizeof(buf), "%.2f", result);

	if (terse)
		puts(buf);
	else
		printf("\aDone, weight: %skg.\n", buf);

	if (disconnect) {
		char cmd[256];
		snprintf(cmd, sizeof(cmd),
				 "bluetoothctl disconnect %s >/dev/null 2>&1", disconnect);
		spawn(cmd);
	}

	if (command) {
		char expanded[256];
		snprintf(expanded, sizeof(expanded), command, buf);
		spawn(expanded);
	}

	exit(EXIT_SUCCESS);
}

