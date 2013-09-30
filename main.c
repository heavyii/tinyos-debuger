#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <netinet/in.h>
#include "process_gdb.h"
#include "step_filters.h"
#include "socket.h"
#include "tinyos_gdb_packet.h"

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

void change_gie(int on) {
	char sp[12] = "";
	char changevar[128] = "1-data-evaluate-expression $r2\n";
	gdb_send(changevar, strlen(changevar));
	int r = gdb_readline(gb.obuf, sizeof(gb.obuf));
	if (r <= 0)
		return;

	if(!strstr(gb.obuf, "1^done,value="))
		return;

	if (get_str_value(gb.obuf, "value", sp, sizeof(sp)) != NULL) {
		u_int16_t r2 = (u_int16_t)atoi(sp);
		u_int16_t r2_dst = 0;
		r2_dst = on > 0 ? (r2 | (1<<3)) : (r2 & (~(1<<3)));

		if(r2 != r2_dst) {
			snprintf(changevar, sizeof(changevar), "1-data-evaluate-expression $r2=0x%04x\n", r2_dst);
			gdb_send(changevar, strlen(changevar));
			gdb_readline(gb.obuf, sizeof(gb.obuf));
			fprintf(stderr, "%s", changevar);
		}
	}


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
					snprintf(mi_cmd, sizeof(mi_cmd), "%d-exec-step\n", seqnum);
					break;
				case SF_OVER:
					snprintf(mi_cmd, sizeof(mi_cmd), "%d-exec-next\n", seqnum);
					break;
				case SF_RETURN:
					snprintf(mi_cmd, sizeof(mi_cmd), "%d-exec-finish\n", seqnum);
					break;
				case SF_NONE:
				default:
					break;
				}
			}
		}
	}
	if(strlen(mi_cmd) > 0) {
		fprintf(stderr, "step_filter: %s", obuf);
		fprintf(stderr, "step_filter: %s", mi_cmd);
		gdb_send(mi_cmd, strlen(mi_cmd));
	} else {
		printf("%s", obuf);
	}
}

int serve_client(int server_fd) {
	/* socket message */
	struct client_args {
		int sockfd;
		struct sockaddr_in addr;
	};
	struct client_args client;
	socklen_t client_addr_len;
	bzero(&client, sizeof(client));
	client_addr_len = sizeof(client.addr);
	client.sockfd = accept(server_fd, (struct sockaddr *) &client.addr,
					&client_addr_len);
	if (client.sockfd > 0) {
		struct packet pkt;
		if(recv_packet_ret(client.sockfd, &pkt) == 0) {
			if (pkt.type == REQU) {
				int result = 0;
				switch (pkt.comid) {
				case GIE_ON:
					change_gie(1);
					result = 1;
					break;
				case GIE_OFF:
					change_gie(0);
					result = 1;
					break;
				default:
					break;
				}

				pkt.type = INFO;
				if(result)
					strcpy(pkt.buffer, "done");
				else
					strcpy(pkt.buffer, "unknown command");
				send_packet(client.sockfd, &pkt);
			}
		}
		close(client.sockfd);
	}
	return 0;
}

static int process() {
	int r;
	int server_fd;
	server_fd = socket_server(SERVER_PORT, 5, INADDR_LOOPBACK);

	while (1) {
		struct timeval timeout;
		fd_set input;
		FD_ZERO(&input);
		FD_SET(STDIN_FILENO, &input);
		FD_SET(gdb_get_fdout(), &input);
		if(server_fd > 0)
			FD_SET(server_fd, &input);
		timeout.tv_sec = 0;
		timeout.tv_usec = 500000;
		r = select(gdb_get_fdout() > server_fd ? gdb_get_fdout() + 1 : server_fd + 1, &input, NULL, NULL, &timeout);

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
			} else if (FD_ISSET(server_fd, &input)) {
				serve_client(server_fd);
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

int control_gie(int on) {
	struct packet pkt;
	int fd = socket_connect_dst("127.0.0.1", SERVER_PORT);
	if(fd < 0)
		return -1;

	bzero(&pkt, sizeof(pkt));
	pkt.type  = REQU;
	pkt.comid = on ? GIE_ON : GIE_OFF;
	send_packet(fd, &pkt);

	alarm(3);
	recv_packet(fd, &pkt);
	if(pkt.type == DONE) {
		if (!strcmp(pkt.buffer, "done"))
			return 0;
	}
	return -1;
}

int main(int argc, char **argv) {
	setvbuf(stderr, NULL, _IONBF, 0);
	setvbuf(stdout, NULL, _IONBF, 0);

	if (strcmp(argv[1], "--gieon") == 0)
		return control_gie(1);
	else if (strcmp(argv[1], "--gieoff") == 0)
		return control_gie(0);

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
