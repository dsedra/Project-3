#include "linkedList.h"

typedef struct cE{
	unsigned int chunkId;
	char chunkHash[20];
	linkedList packetList;
	peerEle* fromThisPeer;
}chunkEle;
	

chunkEle* initChunkEle(unsigned int id, char hash[20]);
void printChunkList(linkedList clist);
int compareChunkHash(char hash1[20], char hash2[20]);
chunkEle* lookupChunkHash(char target[20], linkedList* cList);
peerEle* resolvePeer( struct sockaddr_in from, linkedList pList);
void AddResponses(peerEle* thisPeer, char* buf, linkedList* chunkList, int sock);
void printPeerList(linkedList pList);
