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