#include "linkedList.h"
#include "packet.h"
#include <stdio.h>
#include <sys/time.h>

typedef struct cE{
	unsigned int chunkId;
	char chunkHash[sizeofHash];
	linkedList packetList;
	peerEle* fromThisPeer;
	
	// sender side fields
	node* lastSent;
	node* lastAcked;
	FILE* masterfp;
	int windowSize;
	int lastAckedCount;
	time_t afterLastAckedTime; // last time that lasted
	time_t timeInRTT; // goes off every rtt for congestion avoidance
	time_t sinceStart; // time since start
	int ssthresh;
	int hasRecvRTT;// 1 if received ack in last rtt, 0 else
	
	// receiver side fields
	unsigned int nextExpectedSeq;
	
	// number of byte read from file or receive from sender
	// this can be used by both sender and receiver
	int bytesRead;
	int inProgress;
	
}chunkEle;
	

chunkEle* initChunkEle(unsigned int id, char hash[sizeofHash]);
void printPacketList(linkedList packList);
void printChunkList(linkedList clist);
int compareChunkHash(char hash1[sizeofHash], char hash2[sizeofHash]);
chunkEle* lookupChunkHash(char target[sizeofHash], linkedList* cList);
peerEle* resolvePeer( struct sockaddr_in from, linkedList pList);
void AddResponses(peerEle* thisPeer, char* buf, linkedList* chunkList, int sock);
void printPeerList(linkedList pList);
void* nextDataPacket(FILE* fp, int seq, int size);
chunkEle* buildNewWindow(linkedList* windowSets, linkedList* hasChunkList, peerEle* peer, char* masterDataFilePath, char* buf);
void orderedAdd(chunkEle* cep, void* buf);
chunkEle* resolveChunk(peerEle* peerp, linkedList list);
node* resolveLastPacketAcked(unsigned int target, chunkEle* cep);
void sendPendingGetRequest(linkedList* chunkList, int sock);
int writeChunkToFile(FILE* outfile, linkedList* packetList);
int buildOuputFile(FILE* outfile, linkedList* chunkList);
void findMex(chunkEle* cep);