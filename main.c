#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include "process_gdb.h"
#include "step_filters.h"

#define GDB_IO_BUFFER_SIZE 4000

#define FILE_MAX_LEN  1024
#define FUNC_MAX_LEN  256
#define ARGS_MAX_LEN  256

struct frameInfo {
	int addr;
	char func[FUNC_MAX_LEN];
	char args[ARGS_MAX_LEN];
	char file[FILE_MAX_LEN];
	int line;
};

struct gdb_buffer {
	char ibuf[GDB_IO_BUFFER_SIZE];
	char obuf[GDB_IO_BUFFER_SIZE];
};

static struct gdb_buffer gb;
/**
 * get_str_value - get value by variable name. (variableStr="value")
 * @line: source string
 * @variableStr: variable name
 * @value:	where to store value string
 * @valueLen: buffer length
 * @return: returns value's pointer, NULL for no such variable or NULL value
 */
static char *get_str_value(const char *pline, const char *variable_str,
		char *value, int vlen) {
	char *phead = NULL;
	char *pend = NULL;
	int len = 0;
	if (pline == NULL || variable_str == NULL || value == NULL) {
		fprintf(stderr, "%s: NULL para\n", __FUNCTION__);
		return NULL;
	}

	memset(value, 0, vlen);

	//pHead ==> variableStr[0]
	phead = strstr(pline, variable_str);
	if (phead == NULL) {
		return NULL;
	}

	//pHead ==> "value"
	phead = phead + strlen(variable_str) + 1;

	if (*phead != '"') {
		return NULL;
	}

	//pHead ==> value"
	phead++;

	pend = phead;
	while(1){
		//pEnd ==> " (escape char is \")
		pend = strchr(pend, '"');
		if (pend == NULL) {
			return NULL;
		}
		if(*(pend-1) == '\\'){
			//escape char
			pend++;
			continue;
		} else {
			break;
		}

	}


	//len = strlen(value)
	len = pend - phead;

	//if buffer overflow, cut it
	if (len > vlen - 1) {
		// '\0' at the end
		len = vlen - 1;
	}

	//strncpy may miss '\0' when coped string long enough
	strncpy(value, phead, len);
	value[len] = '\0';

	return value;
}

/**
 * get_frame - write frameInfo structure according to lineP
 * @pline:	source string
 * @pframe:	where to store frame
 * @return: returns 0 on success, -1 on no frame in lineP, -2 on error
 */
static int get_frame(const char *pline, struct frameInfo *pframe) {
	const int bufSize = 32;
	char buf[bufSize];

	memset(pframe, 0, sizeof(struct frameInfo));

	//lineP ==> frame={
	pline = strstr(pline, "frame=");
	if (pline == NULL) {
		return -1;
	}

	//read addr
	if (get_str_value(pline, "addr", buf, bufSize) == NULL) {
		return -1;
	}
	sscanf(buf, "%x", &pframe->addr);

	//func
	if (get_str_value(pline, "func", pframe->func, FUNC_MAX_LEN) == NULL) {
		return -1;
	}

	//file
	if (get_str_value(pline, "file", pframe->file, FILE_MAX_LEN) == NULL) {
		return -1;
	}

	//line
	if (get_str_value(pline, "line", buf, bufSize) == NULL) {
		return -1;
	}
	pframe->line = atoi(buf);

	return 0;
}

char *to_step_over_result(char *obuf) {

	return obuf;
}

/*  *stopped,reason="end-stepping-range",frame={addr="0x00004048",func="main",
	args=[],file="/opt/tinyos-2.1.2/tos/system/RealMainP.nc",
	fullname="/opt/tinyos-2.1.2/tos/system/RealMainP.nc",line="73"},
	thread-id="1",stopped-threads="all"
 */
void handle_gdb_io(char *ibuf, char *obuf) {
	char mi_cmd[256];
	bzero(mi_cmd, sizeof(mi_cmd));
	if (strstr(ibuf, "exec-next") != NULL) {
		if (strstr(obuf, "*stopped") != NULL) {
			int seqnum;
			sscanf(ibuf, "%d", &seqnum);
			struct frameInfo frame;
			if(get_frame(obuf, &frame) == 0) {
				stepfilter_t step_type = step_filter(frame.file, frame.func, frame.line);
				switch (step_type) {
				case SF_INTO:
					fprintf(stderr, "step into\n");
					snprintf(mi_cmd, sizeof(mi_cmd), "%d-exec-step\n", seqnum);
					break;
				case SF_OVER:
					fprintf(stderr, "step over\n");
					snprintf(mi_cmd, sizeof(mi_cmd), "%d-exec-next\n", seqnum);
					break;
				case SF_RETURN:
					fprintf(stderr, "step return\n");
					snprintf(mi_cmd, sizeof(mi_cmd), "%d-exec-finish\n", seqnum);
					break;
				case SF_NONE:
				default:
					break;
				}
			}
		}
	}
	if(strlen(mi_cmd) > 0)
		gdb_send(mi_cmd, strlen(mi_cmd));
	else
		printf("%s", obuf);
}

static int process() {
	int r;

	while (1) {
		struct timeval timeout;
		fd_set input;
		FD_ZERO(&input);
		FD_SET(STDIN_FILENO, &input);
		FD_SET(gdb_get_fdout(), &input);

		timeout.tv_sec = 0;
		timeout.tv_usec = 500000;
		r = select(gdb_get_fdout() + 1, &input, NULL, NULL, &timeout);

		// see if there was an error or actual data
		if (r <= 0) {
			//printf("select TIMEOUT\n");
		} else {
			if (FD_ISSET(STDIN_FILENO, &input)) {
				/* user input */
				r = read(STDIN_FILENO, gb.ibuf, sizeof(gb.ibuf) - 1);
				if (r > 0) {
					gb.ibuf[r] = '\0';
					//fprintf(stderr, "gdb_send: %s\n", ibuf);
					gdb_send(gb.ibuf, r);
					continue;
				}
				break;
			} else if (FD_ISSET(gdb_get_fdout(), &input)) {
				/* gdb output */
				r = gdb_readline(gb.obuf, sizeof(gb.obuf));
				if (r > 0) {
					//fprintf(stderr, "gdb_readline: (%d) [%s]\n", r, obuf);
					handle_gdb_io(gb.ibuf, gb.obuf);
					continue;
				}
				break;
			}
		}

	}

	return r;
}

void sig_handler(int sig) {
	if (sig == SIGINT) {
		int i = 0;
		while(isdigit(gb.ibuf[i]) != 0 && i < sizeof(gb.ibuf))
			i++;
		gb.ibuf[i] = '\0';
	}
}

int main(int argc, char **argv) {
	setvbuf(stderr, NULL, _IONBF, 0);
	setvbuf(stdout, NULL, _IONBF, 0);

	filters_init("/home/heavey/workspace/tinyos-gdb/stepfilters.txt");
	atexit(filters_destroy);
	argv[0] = "msp430-gdb";
	if (gdb_start(argc, argv) < 0)
		return -1;

	signal(SIGINT, sig_handler);
	signal(SIGPIPE, SIG_IGN);

	process();
	return 0;
}
