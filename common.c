/*
 * common.c
 *
 *  Created on: Jun 8, 2013
 *      Author: developer
 */

#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <termios.h>
#include <stdlib.h>

#include "common.h"


static int read_line(int tty, char *line, int len)
{
	int n, idx = 0;
	char ch;

	while(1) {
		n = read(tty, &ch, 1);

		if (n == 1) {
			*(line+idx++) = ch;

			if (ch == '\n' || ch == '\b' || idx == (len-1))
				break;
		} else if (n < 0)
			return -1;
		else
			break;
	}

	*(line+idx) = '\0';

	return idx;
}

void send_cmd(int tty, const char *cmd)
{
	write(tty, cmd, strlen(cmd));
}

int check_line(int tty, char* content, int retry)
{
	int n;
	char line[200];

	while (retry--) {
		n = read_line(tty, line, sizeof(line));
		if (n <= 0)
			continue;

		//printf("%s", line);
		if (strncmp(line, content, strlen(content)) == 0)
			return 0;
	}

	return -1;
}

#define STATUS_LEN		50

int run_shell_command(int tty, char *cmd, int timeout)
{
	char dat, exit_status[STATUS_LEN];
	int n, idx = 0, head = 0, count = 0;

	tcflush(tty_fd, TCIOFLUSH);
	send_cmd(tty, cmd);

	while (1) {
		n = read(tty, &dat, 1);
		if (n == 1) {
			if (dat == '$')
				break;
		} else if (--timeout == 0) {
			printf("timeout\n");
			return -1;
		}
	}

	usleep(100000);
	tcflush(tty_fd, TCIOFLUSH);
	send_cmd(tty, "echo $?\n");
	usleep(100000);

	while (1) {
		n = read(tty, &dat, 1);
		if (n <= 0)
			return -1;

		if (head) {
			exit_status[idx++] = dat;
			if (dat == '\n' || idx == STATUS_LEN-1) {
				exit_status[idx++] = '\0';
				break;
			}
		} else {
			if (dat == '\n') {
				if (++count == 2)
					head = 1;
			} else
				count = 0;
		}
	}

	printf("%s\n", exit_status);
	return atoi(exit_status);
}
