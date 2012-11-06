#include "packet.h"
#include "linkedList.h"
#include "chunkList.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


void* whohasCons(linkedList* listp, unsigned int start){
	if((listp == NULL) || (listp->headp == NULL))
		return NULL;
	void* packet = malloc(headerSize + numChunks + 20*listp->length);
	void* i = packet + headerSize + numChunks; /* beginning of chunks */
	
	packetHead* pHp = (packetHead*) packet;
	pHp->magicNum = MAGICNUM;
	pHp->version = VERSION;
	pHp->type = WHOHAS;
	pHp->headerLen = headerSize;
	pHp->packLen = headerSize + numChunks + 20*listp->length;
	pHp->seqNum = 0;
	pHp->ackNum = 0;
	
	*(unsigned char*)(packet+headerSize) = (unsigned char)listp->length;
	
	node* itr = listp->headp;
	unsigned int counter = 0;
	/* store chunk hashes */
	do{
		if((counter >= start) && (counter < start+MAXCHUNKS)){
			chunkEle* chunkItr = (chunkEle*)itr->data;
			memcpy(i,chunkItr->chunkHash,sizeofHash);
			i += sizeofHash;
		}
		itr = itr->nextp;
		counter++;
	}while(itr != listp->headp);
	
	return packet;
}

void* ihaveCons(char* buf, linkedList* chunkList){
	void* packet = malloc(1500);
	packetHead* pHp = (packetHead*) packet;
	pHp->magicNum = MAGICNUM;
	pHp->version = VERSION;
	pHp->type = IHAVE;
	pHp->headerLen = headerSize;
	pHp->packLen = headerSize + numChunks;// this is temporary
	pHp->seqNum = 0;
	pHp->ackNum = 0;
	void* packetPtr = packet + headerSize + numChunks;// beginning of chunks
	
	char* curr = buf;
	unsigned char* numOfchunks = (unsigned char*)(curr+headerSize);	
	printf("the num of chunks is %u\n", *numOfchunks);
	curr = curr+headerSize+numChunks;
	int i;
	int counter = 0;
	char hash[20];
	for(i = 0 ; i < *numOfchunks ; i ++){
		sscanf( curr, "%20c", hash );
		chunkEle* thisChunk = lookupChunkHash(hash, chunkList);
		if( thisChunk ){
			memcpy(packetPtr, hash, sizeofHash);
			pHp->packLen += sizeofHash;
			packetPtr += sizeofHash;
			counter++;
		}
		curr += sizeofHash;
	}
	if(counter == 0){
		return NULL;
	}
	*(unsigned char*)(packet+headerSize) = (unsigned char)counter;
	return packet;
}

void* getCons(char hash[sizeofHash]){
	void* packet = malloc(40); /* fixed and known */
	packetHead* pHp = (packetHead*) packet;
	pHp->magicNum = MAGICNUM;
	pHp->version = VERSION;
	pHp->type = GET;
	pHp->headerLen = headerSize;
	pHp->packLen = headerSize + numChunks+sizeofHash;
	pHp->seqNum = 0;
	pHp->ackNum = 0;
	void* packetPtr = packet + headerSize + numChunks;// beginning of chunks
	
	memcpy(packetPtr, hash, sizeofHash);
	
	return packet;
}

void* ackCons(unsigned int seq){
	void* packet = malloc(headerSize);
	packetHead* pHp = (packetHead*) packet;
	pHp->magicNum = MAGICNUM;
	pHp->version = VERSION;
	pHp->type = ACK;
	pHp->headerLen = headerSize;
	pHp->packLen = headerSize;
	pHp->seqNum = 0;
	pHp->ackNum = seq;
	return packet;
}
