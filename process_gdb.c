/*
 * process_gdb.c
 *
 *  Created on: Sep 2, 2013
 *      Author: heavey
 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include "process_gdb.h"

/* msp430-gdb info. */
struct gdb_info {
	pid_t pid;
	int fdin;
	int fdout;
	int argc;
	char **argv;
};

static struct gdb_info gdb = { 0 };

static void gdb_die(char *msg) {
	fprintf(stderr, "%s\n", msg);
	exit(EXIT_FAILURE);
}

/**
 * start gdb
 * @return: Return 0 on success, nonzero on error
 */
int gdb_start(int argc, char **argv) {
	pid_t pid;
	int fd1[2], fd2[2];
	gdb.pid = 0;
	gdb.fdout = 0;
	gdb.fdin = 0;
	gdb.argc = argc;
	gdb.argv = argv;

	if (pipe(fd1) || pipe(fd2)) {
		fprintf(stderr, "Pipe failed\n");
		return -1;
	}

	pid = fork();
	if (pid == (pid_t) 0) {
		signal(SIGPIPE, SIG_IGN);
		close(fd1[0]);
		close(fd2[1]);
		dup2(fd1[1], STDOUT_FILENO);
		//dup2(fd1[1], STDERR_FILENO);
		dup2(fd2[0], STDIN_FILENO);
		close(fd1[1]);
		close(fd2[0]);
		int ret = execvp(gdb.argv[0], gdb.argv);
		if (ret < 0) {
			perror("execvp");
			_exit(1);
		}
		_exit(0);
	} else if (pid > (pid_t) 0) {
		close(fd1[1]);
		close(fd2[0]);
		gdb.pid = pid;
		gdb.fdout = fd1[0];
		gdb.fdin = fd2[1];
		return 0;
	} else if (pid < (pid_t) 0) {
		perror("fork");
		return -1;
	}

	return 0;
}

/**
 * gdb_send - Send data, exit if error occur
 */
void gdb_send(const char *data, int len) {
	if (gdb.fdin < 0 || data == NULL || len <= 0) {
		return;
	}

	if (write(gdb.fdin, data, len) != len)
		gdb_die("gdb");
}

/**
 * read one line
 */
int gdb_readline(char *data, int size) {
	char *p = data;
	while (size-- > 0) {
		if (read(gdb.fdout, p, 1) != 1) {
			*p = '\0';
			break;
		}
		if (*p == '\n') {
			*(++p) = '\0';
			break;
		}
		p++;
	}

	return (int)(p - data + 1);
}

int gdb_get_fdin(void) {
	return gdb.fdin;
}

int gdb_get_fdout(void) {
	return gdb.fdout;
}

