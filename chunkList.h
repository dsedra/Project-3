#include "linkedList.h"

typedef struct cE{
	unsigned int chunkId;
	char chunkHash[20];
	linkedList packetList;
	linkedList responseList;
}chunkEle;
	

chunkEle* initChunkEle(unsigned int id, char hash[20]);

