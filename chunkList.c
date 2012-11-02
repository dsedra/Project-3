#include "chunkList.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

chunkEle* initChunkEle(unsigned int id, char hash[20]){
	chunkEle* ele = malloc(sizeof(chunkEle));
	ele->chunkId = id;
	memcpy(ele->chunkHash,hash,20);
	ele->packetList.headp = NULL;
	ele->packetList.length = 0;
	ele->responseList.headp = NULL;
	ele->responseList.length = 0;
	return ele;
}