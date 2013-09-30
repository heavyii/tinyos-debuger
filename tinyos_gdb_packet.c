#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include "tinyos_gdb_packet.h"

static const size_t size_packet = sizeof(struct packet);


void clear_packet(struct packet* p)
{
	short int t = p->conid;
	memset(p, 0, sizeof(struct packet));
	p->conid = t;
}

struct packet* ntohp(struct packet* np)
{
	np->conid = ntohs(np->conid);
	np->type = ntohs(np->type);
	np->comid = ntohs(np->comid);
	np->datalen = ntohs(np->datalen);
	return np;
}

struct packet* htonp(struct packet* hp)
{
	hp->conid = ntohs(hp->conid);
	hp->type = ntohs(hp->type);
	hp->comid = ntohs(hp->comid);
	hp->datalen = ntohs(hp->datalen);
	return hp;
}

void printpacket(struct packet* p, int ptype)
{
	if(!DEBUG)
		return;
	
	if(ptype)
		printf("\t\tHP#");
	else
		printf("\t\tNP#");
	
	printf(" conid(%d) type(%d) comid(%d) datalen(%d)\n",
			p->conid, p->type, p->comid, p->datalen);
	fflush(stdout);
}


/**
 * if error occur, exit(-1)
 */
void send_packet(int sfd, struct packet* hp)
{
	int x;
	struct packet pkt;
	memcpy(&pkt, hp, size_packet);
	//printpacket(&pkt, HP);
	if((x = send(sfd, htonp(&pkt), size_packet, 0)) != size_packet)
		er("send()", x);
}

/**
 * read one packet, exit(-1) for errors.
 */
void recv_packet(int sfd, struct packet* pkt)
{
	int x;
	unsigned char *p = (unsigned char *)pkt;
	int rlen = size_packet;
	while(rlen > 0) {
		if((x = recv(sfd, p, rlen, 0)) <= 0)
			er("recv()", x);
		p += x;
		rlen -= x;
	}
	ntohp(pkt);
	//printpacket(pkt, NP);
}

/**
 * 0 on success, or -1 for errors.
 */
int recv_packet_ret(int sfd, struct packet* pkt)
{
	int x;
	unsigned char *p = (unsigned char *)pkt;
	int rlen = size_packet;
	while(rlen > 0) {
		if((x = recv(sfd, p, rlen, 0)) <= 0)
			return -1;
		p += x;
		rlen -= x;
	}
	ntohp(pkt);
	//printpacket(pkt, NP);
	return 0;
}
