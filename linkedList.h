#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>

#ifndef linkedList_h
#define linkedList_h
typedef struct n{
	struct n* nextp;
	struct n* prevp;
	void* data;
}node;

typedef struct ll{
	node* headp;
	unsigned int length;
}linkedList;


struct ph{
	unsigned short magicNum;
 	unsigned char version, type;
	unsigned short headerLen, packLen;
	unsigned int seqNum, ackNum;
}__attribute__((__packed__));

typedef struct ph packetHead;


typedef struct ps{
	unsigned int id;
	char host[60];
	int port;
	struct sockaddr_in cli_addr;
	int inUse;
}peerEle;

void addList(node* toAdd, linkedList* listp);
void remList(node* toRem, linkedList* listp);
node* initNode(void* data);
#endif