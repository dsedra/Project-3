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

void printChunkList(linkedList clist){
	node* itr = clist.headp;
	int counter = 0;
	
	do{
		chunkEle* itrChunk = (chunkEle*)(itr->data);
		printf("id: %u, %s\n",itrChunk->chunkId,itrChunk->chunkHash);
		counter++;
		itr = itr->prevp;
	}while(itr != clist.headp);
	
	if(counter != clist.length)
		printf("Length incorrect!\n");
		
	return;
}