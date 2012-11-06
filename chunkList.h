#include "linkedList.h"
#include <stdio.h>

typedef struct cE{
	unsigned int chunkId;
	char chunkHash[20];
	linkedList packetList;
	peerEle* fromThisPeer;
	
	// sender side fields
	node* lastSent;
	node* lastAcked;
	FILE* masterfp;
	int windowSize;
	
	// receiver side fields
	unsigned int nextExpectedSeq;
	
	// number of byte read from file or receive from sender
	// this can be used by both sender and receiver
	int bytesRead;
	int inProgress;
	
}chunkEle;
	

chunkEle* initChunkEle(unsigned int id, char hash[20]);
void printPacketList(linkedList packList);
void printChunkList(linkedList clist);
int compareChunkHash(char hash1[20], char hash2[20]);
chunkEle* lookupChunkHash(char target[20], linkedList* cList);
peerEle* resolvePeer( struct sockaddr_in from, linkedList pList);
void AddResponses(peerEle* thisPeer, char* buf, linkedList* chunkList, int sock);
void printPeerList(linkedList pList);
void* nextDataPacket(FILE* fp, int seq, int size);
chunkEle* buildNewWindow(linkedList* windowSets, linkedList* hasChunkList, peerEle* peer, char* masterDataFilePath, char* buf);
void orderedAdd(chunkEle* cep, void* buf);
chunkEle* resolveChunk(peerEle* peerp, linkedList list);
node* resolveLastPacketAcked(unsigned int target, linkedList packetList);
void sendPendingGetRequest(linkedList* chunkList, int sock);