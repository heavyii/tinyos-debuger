#ifndef __SOCKET_H__
#define	__SOCKET_H__

/**
 *	Connect client socket to server
 *	return socket fd on success, -1 on error
 */
int socket_connect_dst(char *dst_ip, int dst_port);

/**
 * recv socket with timeout
 * @return: Returns the number read or -1 for errors, -2 for timeout
 */
int socket_recv_timeout(int socket_fd, void *buffer, int length, struct timeval *timeout);

/**
 * become a TCP server, bind to port, listen for maxconnect connections
 * return socket fd, -1 on error
 */
int socket_server(int port, int maxconnect, uint32_t hostlong);

#endif/* __SOCKET_H__ */

