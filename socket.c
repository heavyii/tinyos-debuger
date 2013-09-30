#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <strings.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <stdio.h>
#include "socket.h"

/**
 *	Connect client socket to server
 *	return socket fd on success, -1 on error
 */
int socket_connect_dst(char *dst_ip, int dst_port) {
	int sockfd;
	struct sockaddr_in dst_addr;

	if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
		return -1;

	bzero(&dst_addr, sizeof(struct sockaddr_in));
	dst_addr.sin_family = AF_INET;
	dst_addr.sin_port = htons(dst_port);
	dst_addr.sin_addr.s_addr = inet_addr(dst_ip);
	if (connect(sockfd, (struct sockaddr *) &dst_addr, sizeof(struct sockaddr_in)) < 0) {
		close(sockfd);
		return -1;
	}

	return sockfd;
}

/**
 * recv socket with timeout
 * @return: Returns the number read or -1 for errors, -2 for timeout
 */
int socket_recv_timeout(int sockfd, void *buffer, int length, struct timeval *timeout) {
	int ret;
	fd_set input;

	FD_ZERO(&input);
	FD_SET(sockfd, &input);

	ret = select(sockfd + 1, &input, NULL, NULL, timeout);

	// see if there was an error or actual data
	if (ret < 0) {
		perror("select");
	} else if (ret == 0) {
		return -2;
	} else {
		if (FD_ISSET(sockfd, &input)) {
			return recv(sockfd, buffer, length, 0);
		}
	}

	return -1;
}

/**
 * become a TCP server, bind to port, listen for maxconnect connections
 * return socket fd, -1 on error
 */
int socket_server(int port, int maxconnect, uint32_t hostlong) {
	int sockfd;
	struct sockaddr_in servaddr;

	if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		perror("socket");
		return -1;
	}

	int on = 1;
	setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));

	bzero(&servaddr, sizeof(servaddr));
	servaddr.sin_family = AF_INET;
	servaddr.sin_addr.s_addr = htonl(hostlong);
	servaddr.sin_port = htons(port);
	if ((bind(sockfd, (struct sockaddr *) &servaddr, sizeof(servaddr))) < 0) {
		perror("listen");
		close(sockfd);
		return -1;
	}

	if (listen(sockfd, maxconnect) < 0) {
		perror("listen");
		close(sockfd);
		return -1;
	}

	return sockfd;
}
