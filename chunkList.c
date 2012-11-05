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
			void* getPack = getCons(thisChunk->chunkHash);
			printf("Send packet to peer %d @  %s:%d\n", thisPeer->id, inet_ntoa(thisPeer->cli_addr.sin_addr) ,ntohs(thisPeer->cli_addr.sin_port));
			spiffy_sendto(sock, getPack, 40, 0, (struct sockaddr *) &thisPeer->cli_addr, sizeof(thisPeer->cli_addr));
		}
		curr += sizeofHash;
	}
}



