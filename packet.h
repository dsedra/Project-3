#include "linkedList.h"
#define headerSize 16
#define numChunks 4 /* 1 byte for num chunks, rest padding */
#define sizeofHash 40
#define chunkSize (512*1024)
#define fixedWindowSize 8
#define sizeofGetPacket (headerSize + numChunks + sizeofHash)
#define min(a,b) ((a<=b)?a:b)
#define max(a,b) ((a>=b)?a:b)
#define halve(a) (max((a/2),2))

#define WHOHAS 0
#define IHAVE 1
#define GET 2
#define DATA 3
#define ACK 4
#define DENIED 5

#define MAGICNUM 15441
#define VERSION 1
#define MAXCHUNKS 74/* max chunks per packet */

#define SLOWSTART 6
#define CONGAVOID 7


void* whohasCons(linkedList* listp, unsigned int start);
void* ihaveCons(char* buf, linkedList* chunkList);
void* getCons(char hash[20]);
void* ackCons(unsigned int seq);
void* deniedCons(int chunkId);