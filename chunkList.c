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
#include <sys/time.h>
#include "spiffy.h"

chunkEle* initChunkEle(unsigned int id, char hash[sizeofHash]){
	chunkEle* ele = malloc(sizeof(chunkEle));
	ele->chunkId = id;
	memcpy(ele->chunkHash,hash,sizeofHash);
	ele->packetList.headp = NULL;
	ele->packetList.length = 0;
	ele->bytesRead = 0;
	ele->inProgress = 0;
	ele->lastAckedCount = 0;
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


int compareChunkHash(char hash1[sizeofHash], char hash2[sizeofHash]){
	int i;
	for(i = 0 ; i < sizeofHash ; i++){
		if(hash1[i] != hash2[i]){
			return 0;
		}
	}
	return 1;
}

chunkEle* lookupChunkHash(char target[sizeofHash], linkedList* cList){
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
	char hash[sizeofHash];
	for(i = 0; i < numOfchunks ; i++){
		sscanf( curr, "%40c", hash );
		chunkEle* thisChunk = lookupChunkHash(hash, chunkList);
		if(!(thisChunk->fromThisPeer)){
			thisChunk->fromThisPeer = thisPeer;
			// initialize the next expected seq num to 1 here, may be put in another place
			thisChunk->nextExpectedSeq = 1;
			if( thisChunk->fromThisPeer->inUse == 0){
				thisChunk->inProgress = 1;
				thisChunk->fromThisPeer->inUse = 1; 
				void* getPack = getCons(thisChunk->chunkHash);
				printf("Send GET request to peer %d @  %s:%d\n", thisPeer->id, inet_ntoa(thisPeer->cli_addr.sin_addr) ,ntohs(thisPeer->cli_addr.sin_port));
				spiffy_sendto(sock, getPack, sizeofGetPacket, 0, (struct sockaddr *) &thisPeer->cli_addr, sizeof(thisPeer->cli_addr));
				//sendto(sock, getPack, sizeofGetPacket, 0, (struct sockaddr *) &thisPeer->cli_addr, sizeof(thisPeer->cli_addr));
		}
		}
		curr += sizeofHash;
	}
}

void sendPendingGetRequest(linkedList* chunkList, int sock){
	int i;
	node* itr = chunkList->headp;
	for(i = 0 ; i < chunkList->length; i ++){
		chunkEle*  thisELe = (chunkEle* )itr->data;
		
		if((thisELe->fromThisPeer) && (thisELe->fromThisPeer->inUse == 0) && (thisELe->bytesRead == 0)){
			thisELe->fromThisPeer->inUse = 1; 
			thisELe->inProgress = 1;
			void* getPack = getCons(thisELe->chunkHash);
			printf("Send GET request to peer %d @  %s:%d\n", thisELe->fromThisPeer->id, inet_ntoa(thisELe->fromThisPeer->cli_addr.sin_addr) ,ntohs(thisELe->fromThisPeer->cli_addr.sin_port));
			spiffy_sendto(sock, getPack, sizeofGetPacket, 0, (struct sockaddr *) &(thisELe->fromThisPeer->cli_addr), sizeof(thisELe->fromThisPeer->cli_addr));
			//sendto(sock, getPack, sizeofGetPacket, 0, (struct sockaddr *) &(thisELe->fromThisPeer->cli_addr), sizeof(thisELe->fromThisPeer->cli_addr));
		}
		
		itr = itr->nextp;
	}
	
}



void orderedAdd(chunkEle* cep, void* buf){
	node* curp = cep->packetList.headp;
	unsigned int target = ((packetHead*)buf)->seqNum;
	int contentLength = ((packetHead*)buf)->packLen - headerSize;
	
	node* newNode = initNode(buf);
	
	
	if(cep->packetList.headp == NULL){
		cep->bytesRead += contentLength;
		newNode->nextp = newNode;
		newNode->prevp = newNode;
		cep->packetList.headp = newNode;
		cep->packetList.length++;
		return;
	}
	else{
		int yes = 0;
		//printf("Before add, head is %d\n", ((packetHead*)cep->packetList.headp->data)->seqNum);
		if( target < ((packetHead*)cep->packetList.headp->data)->seqNum){
			yes = 1;
		}
		
		int i;
		for( i = 0 ; i < cep->packetList.length ; i++){
			unsigned int currSeq = ((packetHead* )curp->data)->seqNum;
			unsigned int prevSeq = ((packetHead* )curp->prevp->data)->seqNum;
			//printf("curr:%d, target:%d\n",currSeq, target);
			if(target == currSeq)
				return;
			else if( target < currSeq ){
				if( target > prevSeq){
				cep->bytesRead += contentLength;
				newNode->nextp = curp;
				newNode->prevp = curp->prevp;
				newNode->prevp->nextp = newNode;
				newNode->nextp->prevp = newNode;
				if( yes ){
					printf("head switches to %d\n", target);
					cep->packetList.headp = newNode;
				}
				cep->packetList.length++;
				
				}
				return;
			}
			curp = curp->nextp;
		}
		// back to the end
		curp = curp->prevp;
		cep->bytesRead += contentLength;
		//printf("Add after %d\n", ((packetHead* )curp->data)->seqNum);
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
	chunkEle* finished;
	
	do{
		chunkEle* thisEle = (chunkEle*)(curp->data);
		if(thisEle->fromThisPeer == peerp && thisEle->inProgress == 1){
			return thisEle;
		}
		else if( thisEle->fromThisPeer == peerp && !thisEle->inProgress && thisEle->packetList.length != 0){
			finished = thisEle;
		}
		curp = curp->nextp;
	}while(curp != list.headp);
	
	return finished;
}

node* resolveLastPacketAcked(unsigned int target, chunkEle* cep){	
	node* curp = cep->packetList.headp;
	do{
		unsigned int seq = ((packetHead*)curp->data)->seqNum;
		if( target == seq ){
			printf("***Resolve last ack as %d\n", target);
			return curp;
		}	
		curp = curp->nextp;
	}while(curp != cep->packetList.headp);
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
	char hash[sizeofHash];
	sscanf( curr, "%40c", hash );
	printf("*** hash: %s***\n", hash);
	
	chunkEle* thisChunk = lookupChunkHash(hash, hasChunkList);
	chunkEle* thisWindow = initChunkEle( thisChunk->chunkId, thisChunk->chunkHash);
	thisWindow->fromThisPeer = peer;
	thisWindow->bytesRead = 0;
	thisWindow->windowSize = fixedWindowSize;
	thisWindow->lastSent = NULL;
	thisWindow->lastAcked = NULL;
	time(&thisWindow->afterLastAckedTime);

	thisWindow->inProgress = 1;
	if((fp = fopen(masterDataFilePath, "r")) == NULL){
		fprintf(stderr,"Error opening %s",masterDataFilePath);
		exit(1);
	}
	// need to lookup master data file
	long offset = thisWindow->chunkId * chunkSize;
	int size = 1500 - headerSize;
	int seq = 1; 
	fseek(fp, offset, SEEK_SET);

	/* add 1 packet to list, per slow start spec */
	void* thisPacket = nextDataPacket(fp, seq, size);
	thisWindow->bytesRead += size;
	node* newNode = initNode(thisPacket);
	addList(newNode, &(thisWindow->packetList));



	thisWindow->masterfp = fp;
	node* wNode = initNode(thisWindow);
	addList(wNode, windowSets);
	return thisWindow;
}

int writeChunkToFile(FILE* outfile, linkedList* packetList){
	node* itr = packetList->headp;
	int sum = 0;
	do{
		void* thisPacket = itr->data;
		int bufSize = ((packetHead* )thisPacket)->packLen - headerSize;
		fwrite(thisPacket + headerSize ,bufSize, 1 , outfile);
		sum += bufSize;
		itr = itr->nextp;
	}while(itr != packetList->headp );
	return sum;
}

void findMex(chunkEle* cep){
	node* curp = cep->packetList.headp->nextp;
	node* prevp = cep->packetList.headp;
	
	do{
		int curNum = ((packetHead*)curp->data)->seqNum;
		int prevNum = ((packetHead*)prevp->data)->seqNum;
		if((curNum-prevNum) != 1){
			cep->nextExpectedSeq = prevNum+1;
			return;
		}
		prevp = curp;
		curp = curp->nextp;
	}while(curp != cep->packetList.headp->nextp);

	return;
}

int buildOuputFile(FILE* outfile, linkedList* chunkList){
	node* curr = chunkList->headp->prevp;
	int sum = 0;
	int i;
	for( i = 0; i < chunkList->length; i++ ){
		chunkEle* thisChunk = (chunkEle* )curr->data;
		int chunkWrites = writeChunkToFile( outfile, &thisChunk->packetList);
		sum += chunkWrites;
		printf("Write chunk %d, size %d to file\n",thisChunk->chunkId,  chunkWrites);
		curr = curr->prevp;
	}
	return sum;
}



