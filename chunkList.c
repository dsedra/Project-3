#include "chunkList.h"
#include "packet.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netdb.h>
#include <arpa/inet.h>
#include "spiffy.h"

chunkEle* initChunkEle(unsigned int id, char hash[20]){
	chunkEle* ele = malloc(sizeof(chunkEle));
	ele->chunkId = id;
	memcpy(ele->chunkHash,hash,20);
	ele->packetList.headp = NULL;
	ele->packetList.length = 0;
	return ele;
}

void printChunkList(linkedList clist){
	node* itr = clist.headp;
	if( itr == NULL){
		return;
	}
	int counter = 0;
	do{
		chunkEle* itrChunk = (chunkEle*)(itr->data);
		printf("id: %u, %s\n",itrChunk->chunkId,itrChunk->chunkHash);
		if(itrChunk->fromThisPeer != NULL){
			printf("peer id %d, %s:%d\n", itrChunk->fromThisPeer->id, itrChunk->fromThisPeer->host, itrChunk->fromThisPeer->port);
		}
		counter++;
		itr = itr->prevp;
	}while(itr != clist.headp);
	
	if(counter != clist.length)
		printf("Length incorrect!\n");
	
	return;
}

void printPeerList(linkedList pList){
	node* itr = pList.headp;
	if( itr == NULL){
		printf("No peers\n");
		return;
	}
	int counter = 0;
	do{
		peerEle* thisPeer = (peerEle*)(itr->data);
		printf("Peer id: %u %s:%d\n", thisPeer->id, thisPeer->host, thisPeer->port);
		itr = itr->nextp;
		counter++;
	}while(itr != pList.headp );
	if( counter != pList.length ){
		printf("Length incorrect!\n");
	}
}

void printPacketList(linkedList packList){
	node* itr = packList.headp;
	if( itr == NULL){
		printf("No packets\n");
		return;
	}
	int counter = 0;
	do{
		void* thisPacket = itr->data;
		printf("Packet with seq num %d\n", ((packetHead* )thisPacket)->seqNum);
		itr = itr->nextp;
		counter++;
	}while(itr != packList.headp );
	if( counter != packList.length ){
		printf("Length incorrect!\n");
	}
}


int compareChunkHash(char hash1[20], char hash2[20]){
	int i;
	for(i = 0 ; i < 20 ; i++){
		if(hash1[i] != hash2[i]){
			return 0;
		}
	}
	return 1;
}

chunkEle* lookupChunkHash(char target[20], linkedList* cList){
	node* itr = cList->headp;
	do{
		chunkEle* itrChunk = (chunkEle*)(itr->data);
		if( compareChunkHash(target, itrChunk->chunkHash) ){
			return itrChunk;
		}
		itr = itr->nextp;	
	}while( itr != cList->headp );
	return NULL;
}

peerEle* resolvePeer( struct sockaddr_in from, linkedList pList ){
	node* itr = pList.headp;
	do{
		peerEle* thisPeer = (peerEle* ) itr->data;
		//printf("**%s:%d**\n",thisPeer->host, thisPeer->port);
		//printf("TARGET %s:%d\n",inet_ntoa(from.sin_addr), ntohs(from.sin_port));
		//strcmp(thisPeer->host, inet_ntoa(from.sin_addr))== 0 &&
		if( thisPeer->port == ntohs(from.sin_port)){	
			return thisPeer;
		}
		itr = itr->nextp;
	}while(itr != pList.headp);
	return NULL;
}

void AddResponses(peerEle* thisPeer, char* buf, linkedList* chunkList, int sock){
	unsigned char* curr = buf;
	unsigned char numOfchunks = *(curr+headerSize);
	curr = curr + headerSize + numChunks; // beginning of the chunks
	int i;
	char hash[20];
	for(i = 0; i < numOfchunks ; i++){
		sscanf( curr, "%20c", hash );
		chunkEle* thisChunk = lookupChunkHash(hash, chunkList);
		if(!(thisChunk->fromThisPeer)){
			thisChunk->fromThisPeer = thisPeer;
			// initialize the next expected seq num to 1 here, may be put in another place
			thisChunk->nextExpectedSeq = 1;
			thisChunk->bytesRead = 0;
			
			if( thisChunk->fromThisPeer->inUse == 0){
				thisChunk->fromThisPeer->inUse = 1; 
				void* getPack = getCons(thisChunk->chunkHash);
				printf("Send GET request to peer %d @  %s:%d\n", thisPeer->id, inet_ntoa(thisPeer->cli_addr.sin_addr) ,ntohs(thisPeer->cli_addr.sin_port));
				spiffy_sendto(sock, getPack, 40, 0, (struct sockaddr *) &thisPeer->cli_addr, sizeof(thisPeer->cli_addr));
		}
		}
		curr += sizeofHash;
	}
}

void orderedAdd(chunkEle* cep, void* buf){
	node* curp = cep->packetList.headp;
	unsigned int target = ((packetHead*)buf)->seqNum;
	int contentLength = ((packetHead*)buf)->packLen - headerSize;
	cep->bytesRead += contentLength;
	node* newNode = initNode(buf);
	
	
	if(cep->packetList.headp == NULL){
		newNode->nextp = newNode;
		newNode->prevp = newNode;
		cep->packetList.headp = newNode;
		cep->packetList.length++;
		return;
	}
	else{
		int yes = 0;
		printf("Before add, head is %d\n", ((packetHead*)cep->packetList.headp->data)->seqNum);
		if( target < ((packetHead*)cep->packetList.headp->data)->seqNum){
			yes = 1;
		}
		
		int i;
		for( i = 0 ; i < cep->packetList.length ; i++){
			unsigned int currSeq = ((packetHead* )curp->data)->seqNum;
			printf("curr:%d, target:%d\n",currSeq, target);
			if( target < currSeq){
				newNode->nextp = curp;
				newNode->prevp = curp->prevp;
				newNode->prevp->nextp = newNode;
				newNode->nextp->prevp = newNode;
				if( yes ){
					printf("head switches to %d\n", target);
					cep->packetList.headp = newNode;
				}
				cep->packetList.length++;
				return;
			}
			curp = curp->nextp;
		}
		// back to the end
		curp = curp->prevp;
		printf("Add after %d\n", ((packetHead* )curp->data)->seqNum);
		newNode->nextp = cep->packetList.headp;
		cep->packetList.headp->prevp = newNode;
		newNode->prevp = curp;
		curp->nextp = newNode;
		cep->packetList.length++;
		return;
	}
}

chunkEle* resolveChunk(peerEle* peerp, linkedList list){
	node* curp = list.headp;
	do{
		chunkEle* thisEle = (chunkEle*)(curp->data);
		if(thisEle->fromThisPeer == peerp)
			return thisEle;
		curp = curp->nextp;
	}while(curp != list.headp);
	return NULL;
}

node* resolveLastPacketAcked(unsigned int target, linkedList packetList){
	node* curp = packetList.headp;
	do{
		unsigned int seq = ((packetHead*)curp->data)->seqNum;
		if( target == seq ){
			printf("Resolve last ack as %d\n", target);
			return curp;
		}	
		curp = curp->nextp;
	}while(curp != packetList.headp);
	return NULL;
}


// return the next data packet based on offset, size need to read and seq
void* nextDataPacket(FILE* fp, int seq, int size){
	//fseek(fp, offset, SEEK_SET);
	void* packet = malloc(headerSize + size);
	packetHead* pHp = (packetHead*) packet;
	pHp->magicNum = MAGICNUM;
	pHp->version = VERSION;
	pHp->type = DATA;
	pHp->headerLen = headerSize;
	pHp->packLen = headerSize +size;
	pHp->seqNum = seq;
	pHp->ackNum = 0;
	void* packetPtr = packet + headerSize; // beginning of chunk data
	fread(packetPtr, size, 1, fp);
	return packet;
}

chunkEle* buildNewWindow(linkedList* windowSets, linkedList* hasChunkList, peerEle* peer, char* masterDataFilePath, char* buf){
	FILE* fp;
	char* curr = buf;
	curr = curr + headerSize + numChunks;// beginning of the chunks
	char hash[20];
	sscanf( curr, "%20c", hash );
	chunkEle* thisChunk = lookupChunkHash(hash, hasChunkList);
	chunkEle* thisWindow = initChunkEle( thisChunk->chunkId, thisChunk->chunkHash);
	thisWindow->fromThisPeer = peer;
	thisWindow->bytesRead = 0;
	thisWindow->windowSize = fixedWindowSize;
	thisWindow->lastSent = NULL;
	thisWindow->lastAcked = NULL;
	
	if((fp = fopen(masterDataFilePath, "r")) == NULL){
		fprintf(stderr,"Error opening %s",masterDataFilePath);
		exit(1);
	}
	// need to lookup master data file
	long offset = thisWindow->chunkId * chunkSize;
	int size = 1500 - headerSize;
	int seq; 
	fseek(fp, offset, SEEK_SET);
	for( seq = 1; seq <= fixedWindowSize; seq ++ ){
		void* thisPacket = nextDataPacket(fp, seq, size);
		thisWindow->bytesRead += size;
		node* newNode = initNode(thisPacket);
		addList(newNode, &(thisWindow->packetList));
	}
	thisWindow->masterfp = fp;
	node* wNode = initNode(thisWindow);
	addList(wNode, windowSets);
	return thisWindow;
}


