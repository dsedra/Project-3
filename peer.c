/*
 * peer.c
 *
 * Authors: Ed Bardsley <ebardsle+441@andrew.cmu.edu>,
 *          Dave Andersen
 * Class: 15-441 (Spring 2005)
 *
 * Skeleton for 15-441 Project 2.
 *
 */

#include <sys/types.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <unistd.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <sys/time.h>
#include <signal.h>
#include "debug.h"
#include "spiffy.h"
#include "bt_parse.h"
#include "input_buffer.h"
#include "linkedList.h"
#include "packet.h"
#include "chunkList.h"

#define min(a,b) ((a<=b)?a:b)

linkedList chunkList;
linkedList peerList;
linkedList haschunkList;
linkedList windowSets;
char masterDataFilePath[100];
FILE* outputFile;
time_t start;

/* global udp socket */
int sock;
int myId;
void peer_run(bt_config_t *config);
void parsePeerFile(char* peerFile);
void parseChunkFile(char* chunkfile, linkedList* list);
void parseMasterChunkFile(char* masterChunkFile);

int main(int argc, char **argv) {
  bt_config_t config;

  bt_init(&config, argc, argv);

  DPRINTF(DEBUG_INIT, "peer.c main beginning\n");

#ifdef TESTING
  config.identity = 1; // your group number here
  strcpy(config.chunk_file, "chunkfile");
  strcpy(config.has_chunk_file, "haschunks");
#endif

  bt_parse_command_line(&config);

#ifdef DEBUG
  if (debug & DEBUG_INIT) {
    bt_dump_config(&config);
  }
#endif
  peer_run(&config);
  return 0;
}


void process_inbound_udp(int sock) {
  #define BUFLEN 1500
  struct sockaddr_in from;
  socklen_t fromlen;
  char buf[BUFLEN];

  fromlen = sizeof(from);
  spiffy_recvfrom(sock, buf, BUFLEN, 0, (struct sockaddr *) &from, &fromlen);
	//recvfrom(sock, buf, BUFLEN, 0, (struct sockaddr *) &from, &fromlen);
  packetHead* pHead = (packetHead* )buf;
  peerEle* peer = resolvePeer(from, peerList);
  switch(pHead->type){
	case WHOHAS:{
		printf("Recieve WHOHAS request\n");
		void* ihavep = ihaveCons(buf, &haschunkList);
		if( ihavep!= NULL){
			unsigned int bufSize = ((packetHead *)ihavep)->packLen;
			spiffy_sendto(sock, ihavep, bufSize, 0, (struct sockaddr *) &from, fromlen);
			//sendto(sock, ihavep, bufSize, 0, (struct sockaddr *) &from, fromlen);
			time(&start);
		}
		break;
	}
	case IHAVE:{
		printf("Receieve IHAVE request\n");
		printf("packet length is %u\n", pHead->packLen);
		char tmp[sizeofHash];
		memcpy(tmp, (buf+20), sizeofHash);
		printf("chunk hash: %s\n", tmp);
		peerEle* thisPeer = resolvePeer(from, peerList);
		time_t finish;
		time(&finish);
		if( thisPeer == NULL ){
			printf("RESOLVE FAILED\n");
		}
		thisPeer->rtt = difftime(finish, start);
		AddResponses(thisPeer, buf, &chunkList, sock);
		printChunkList(chunkList);
		break;
	}
	case GET:{
		printf("Receive GET Request\n");
		printf("packet length is %u\n", pHead->packLen);
		//need to resolve peer 
		
		chunkEle* thisWindow = buildNewWindow(&windowSets, &haschunkList, peer, masterDataFilePath, buf);
		// here we try to send the full window of packets out
		int i;
		node* curr = thisWindow->packetList.headp->prevp;
		//node* curr = thisWindow->packetList.headp;
		for( i = 0 ; i < thisWindow->windowSize ; i++){
			void* packet = curr->data;
			unsigned int bufSize = ((packetHead *)packet)->packLen;
			spiffy_sendto(sock, packet, bufSize, 0, (struct sockaddr *) &peer->cli_addr, sizeof(peer->cli_addr));
			//sendto(sock, packet, bufSize, 0, (struct sockaddr *) &peer->cli_addr, sizeof(peer->cli_addr));
			thisWindow->lastSent = curr; 
			curr = curr->prevp;
			//curr = curr->nextp;
		}
		break;
	}
	case DATA:{
		printf("Receive data packet %d, with size %d \n", ((packetHead *)buf)->seqNum, ((packetHead *)buf)->packLen - headerSize);
		
		unsigned int bufSize = ((packetHead *)buf)->packLen;
		void* newBuf = malloc(bufSize);
		memcpy(newBuf,buf,bufSize);
		chunkEle* cep = resolveChunk(peer, chunkList);
		orderedAdd(cep,newBuf);
		
		findMex(cep);
		printPacketList(cep->packetList);
		void* packet = ackCons(cep->nextExpectedSeq - 1);


		spiffy_sendto(sock, packet, headerSize, 0, (struct sockaddr *) &peer->cli_addr, sizeof(peer->cli_addr));
		//sendto(sock, packet, headerSize, 0, (struct sockaddr *) &peer->cli_addr, sizeof(peer->cli_addr));
		printf("bytes read for hash %s : %d\n",cep->chunkHash, cep->bytesRead);
		//check if finish receiving the whole chunk
		if(cep->bytesRead == chunkSize ){
			printf("Sucessfully receive the chunk %s\n", cep->chunkHash);
			chunkList.finished ++;
			cep->fromThisPeer->inUse = 0;
			cep->inProgress = 0;
			if(chunkList.finished == chunkList.length){
				printf("Finish receiving the whole file\n");
				// merge the chunks and write to the corresponding output file
				buildOuputFile(outputFile, &chunkList);
				fclose(outputFile);
				printf("GOT output file\n");
			}else{
				// try to send rest of pending get requests 
				// they are deferred to make sure no concurrent downloading from the same peer
				sendPendingGetRequest(&chunkList, sock);
			}
		}
		
		break;
	}
	case ACK:{
		printf("Receive Ack %d\n", *(int*)(buf+12));
		chunkEle* cep = resolveChunk(peer, windowSets);

		if( (cep->lastAcked) && ((packetHead* )(cep->lastAcked->data))->seqNum == ((packetHead* )(buf))->ackNum ){
			cep->lastAckedCount ++;
			printf("Incrementing last ack counter for %d to %d\n", ((packetHead* )(buf))->ackNum,cep->lastAckedCount);
			if( cep->lastAckedCount  == 3 ){
				//try retrasmit
				cep->lastAckedCount = 0;
				node* retry = cep->lastAcked->prevp;
				printf("Retramsmit packet %d\n", ((packetHead* )retry->data)->seqNum);
				spiffy_sendto(sock, retry->data, ((packetHead* )retry->data)->packLen, 0, (struct sockaddr *) &peer->cli_addr, sizeof(peer->cli_addr));
				break;
			}

		}else{
			cep->lastAcked = resolveLastPacketAcked( ((packetHead*)buf)->ackNum, cep->packetList);
		}
		
		//check if finish sending the whole chunk
		if( cep->bytesRead == chunkSize ){
			printf("Successfully send the chunk %s\n", cep->chunkHash);
			if(cep->lastAcked == cep->lastSent){
				// some clear below
				printf("All sent packets have been acked\n");
				cep->inProgress = 0;
			}else{
				printf("lastAcked:%d, lastSent:%d\n",((packetHead*)cep->lastAcked->data)->seqNum, ((packetHead*)cep->lastSent->data)->seqNum );
			}
			
		}else{
		int sizeToRead = min(chunkSize - cep->bytesRead , 1500-headerSize);
		unsigned int seqToSend = ((packetHead* )cep->lastSent->data)->seqNum + 1;
		void* packet = nextDataPacket(cep->masterfp, seqToSend , sizeToRead);
		node* newNode = initNode(packet);
		addList(newNode, &(cep->packetList));
		spiffy_sendto(sock, packet, ((packetHead* )packet)->packLen, 0, (struct sockaddr *) &peer->cli_addr, sizeof(peer->cli_addr));
		//sendto(sock, packet, ((packetHead* )packet)->packLen, 0, (struct sockaddr *) &peer->cli_addr, sizeof(peer->cli_addr));
		cep->lastSent = newNode;
		cep->bytesRead += sizeToRead;
		printf("Bytes read: %d\n", cep->bytesRead);
		}
		break;
	}
	
	
	}
  printf("PROCESS_INBOUND_UDP SKELETON -- replace!\n"
	 "Incoming message from %s:%d\n%s\n\n", 
	 inet_ntoa(from.sin_addr),
	 ntohs(from.sin_port),
	 buf);
}

void process_get(char *chunkfile, char *outputfile) {
	parseChunkFile(chunkfile, &chunkList);
	printChunkList(chunkList);
	
	if((outputFile = fopen(outputfile, "w")) == NULL){
		fprintf(stderr,"Error opening %s\n",outputfile);
		exit(1);
	}
	
	int i;
	int j;
	/* needed if too many chunks for one packet */
	for(i = 0; i < chunkList.length; i += MAXCHUNKS){
		void* whohasp = whohasCons(&chunkList, i);
		unsigned int bufSize = ((packetHead *)whohasp)->packLen;
		node* itr = peerList.headp;	
		for(j = 0; j < peerList.length; j++){
	      peerEle* thisPeer = (peerEle*)itr->data;
	 		if( thisPeer->id != myId ){
				printf("Send packet to peer %d @  %s:%d\n", thisPeer->id, inet_ntoa(thisPeer->cli_addr.sin_addr) ,ntohs(thisPeer->cli_addr.sin_port));
		  		spiffy_sendto(sock, whohasp, bufSize, 0, (struct sockaddr *) &thisPeer->cli_addr, sizeof(thisPeer->cli_addr));
				//sendto(sock, whohasp, bufSize, 0, (struct sockaddr *) &thisPeer->cli_addr, sizeof(thisPeer->cli_addr));
	  		}
			itr = itr->nextp;
		}
	}
	
  	printf("PROCESS GET SKELETON CODE CALLED.  Fill me in!  (%s, %s)\n", 
	chunkfile, outputfile);
}

void handle_user_input(char *line, void *cbdata) {
  char chunkf[128], outf[128];

  bzero(chunkf, sizeof(chunkf));
  bzero(outf, sizeof(outf));

  if (sscanf(line, "GET %120s %120s", chunkf, outf)) {
    if (strlen(outf) > 0) {
      process_get(chunkf, outf);
    }
  }
}

void peer_run(bt_config_t *config) {
  struct sockaddr_in myaddr;
  fd_set readfds;
  struct user_iobuf *userbuf;
  // get my id from config
  myId = config->identity;
  // parse peer list file
  parsePeerFile(config->peer_list_file);
  // parse has chunk file
  parseChunkFile(config->has_chunk_file, &haschunkList);
  // parse master chunk file to get the path of master data file
  parseMasterChunkFile(config->chunk_file);		
  printf("The master data file name: %s\n", masterDataFilePath);
	
	
  if ((userbuf = create_userbuf()) == NULL) {
    perror("peer_run could not allocate userbuf");
    exit(-1);
  }
  
  if ((sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP)) == -1) {
    perror("peer_run could not create socket");
    exit(-1);
  }
  
  bzero(&myaddr, sizeof(myaddr));
  myaddr.sin_family = AF_INET;
  myaddr.sin_addr.s_addr = htonl(INADDR_ANY);
  myaddr.sin_port = htons(config->myport);
  
  if (bind(sock, (struct sockaddr *) &myaddr, sizeof(myaddr)) == -1) {
    perror("peer_run could not bind socket");
    exit(-1);
  }
  
  spiffy_init(config->identity, (struct sockaddr *)&myaddr, sizeof(myaddr));
  printf("I am peer %d, %s:%d\n",myId, inet_ntoa(myaddr.sin_addr) ,ntohs(myaddr.sin_port));
  while (1) {
    int nfds;
    FD_SET(STDIN_FILENO, &readfds);
    FD_SET(sock, &readfds);
    
    nfds = select(sock+1, &readfds, NULL, NULL, NULL);
    
    if (nfds > 0) {
      if (FD_ISSET(sock, &readfds)) {
		process_inbound_udp(sock);
      }
      
      if (FD_ISSET(STDIN_FILENO, &readfds)) {
		printf("going to process_user_input\n");
		process_user_input(STDIN_FILENO, userbuf, handle_user_input,
			   "Currently unused");
      }
    }
  }
}

void parsePeerFile(char* peerFile){
	FILE* fp;
	char locBuf[100];	
	unsigned int id;
	char hostname[60];
	unsigned int port;

	if((fp = fopen(peerFile, "r")) == NULL){
		fprintf(stderr,"Error opening %s",peerFile);
		exit(1);
	}
	
	while(fgets(locBuf,sizeof(locBuf),fp)){
		/* read each line in peerfile */
		if(sscanf(locBuf,"%u %s %u",&id,hostname,&port) < 2){
			fprintf(stderr,"Malformed chunkfile %s\n",peerFile);
			continue;
		}
		
		/* add new peer entry in list */
		peerEle* ele = malloc(sizeof(peerEle));
		ele->id = id;
		struct hostent *h;
		if((h = gethostbyname(hostname))==NULL) {
			printf("error resolving host\n");
			break;
		}
		memset(&ele->cli_addr, '\0', sizeof(ele->cli_addr));
	   	ele->cli_addr.sin_family = AF_INET;
		ele->cli_addr.sin_addr.s_addr = *(in_addr_t *)h->h_addr;
  		ele->cli_addr.sin_port = htons(port);
		
		
		memcpy(ele->host, hostname, strlen(hostname));
		ele->port = port;
		ele->inUse = 0;
		
		node* newNode = initNode(ele);
		addList(newNode, &peerList);
	}
	fclose(fp);
	return;
}

void parseChunkFile(char* chunkfile, linkedList* list){
	FILE* fp;
	char locBuf[100];
	unsigned int id;
	char hash[sizeofHash];
	/* parse chunkfile*/
	if((fp = fopen(chunkfile, "r")) == NULL){
		fprintf(stderr,"Error opening %s",chunkfile);
		exit(1);
	}
	list->length = 0;
	list->finished = 0;
	while(fgets(locBuf, sizeof(locBuf),fp)){
		/* read each line in chunkfile */
		if(sscanf(locBuf,"%d %40c",&id,hash) < 2){
			fprintf(stderr,"Malformed chunkfile %s\n",chunkfile);
		}
		
		/* add new chunk entry in list */
		chunkEle* ele = initChunkEle(id,hash);
		node* newNode = initNode(ele);
		addList(newNode, list);
	}
	fclose(fp);
}

void parseMasterChunkFile(char* masterChunkFile){
	FILE* fp;
	if((fp = fopen(masterChunkFile, "r")) == NULL){
		fprintf(stderr,"Error opening %s",masterChunkFile);
		exit(1);
	}
	char localbuf[100];
	fgets(localbuf, 100, fp);
	sscanf(localbuf, "File: %s", masterDataFilePath);
	fclose(fp);
}
