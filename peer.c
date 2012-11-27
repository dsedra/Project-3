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


#define timeout 3

linkedList chunkList;
linkedList peerList;
linkedList haschunkList;
linkedList windowSets;
char masterDataFilePath[100];
char outputFilePath[100];
FILE* outputFile;
time_t start;
int maxCon;

/* global udp socket */
int sock;
int myId;
peerEle* mePeer;
void peer_run(bt_config_t *config);
void parsePeerFile(char* peerFile);
void parseChunkFile(char* chunkfile, linkedList* list);
void parseMasterChunkFile(char* masterChunkFile);
void fillWindow(chunkEle* cep);
void cleanList(chunkEle* cep);
void sendWindow(chunkEle* cep, int sock);
void alarmHandler(int sig);
void exitFunc();
void sendAfterLastAcked(chunkEle* cep);


int main(int argc, char **argv) {
  bt_config_t config;

  bt_init(&config, argc, argv);

  DPRINTF(DEBUG_INIT, "peer.c main beginning\n");


  struct itimerval tout_val;
  
  tout_val.it_interval.tv_sec = 0;
  tout_val.it_interval.tv_usec = 0;
  tout_val.it_value.tv_sec = 0; 
  tout_val.it_value.tv_usec = 100000; // 100 ms
  setitimer(ITIMER_REAL, &tout_val,0);

  signal(SIGALRM,alarmHandler); /* set the Alarm signal capture */
  signal(SIGINT,exitFunc);

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
		
		//check for free connections
		printf("maxcon:%d id: %d numout: %d\n",maxCon,mePeer->id,mePeer->numOut);
		if(mePeer->numOut == maxCon){
			chunkEle* dcp = lookupChunkHash((buf+20),&haschunkList);
			void* deniedp = deniedCons(dcp->chunkId);
			spiffy_sendto(sock, deniedp, headerSize, 0, (struct sockaddr *) &peer->cli_addr, sizeof(peer->cli_addr));
			//sendto(sock, deniedp, headerSize, 0, (struct sockaddr *) &peer->cli_addr, sizeof(peer->cli_addr));
			break;
		}
		

		chunkEle* thisWindow = buildNewWindow(&windowSets, &haschunkList, peer, masterDataFilePath, buf);

		// send first element, no need to clean or fill
		sendWindow(thisWindow, sock);
		mePeer->numOut++;//increase num
		break;
	}
	case DATA:{
		unsigned int bufSize = ((packetHead *)buf)->packLen;
		void* newBuf = malloc(bufSize);
		memcpy(newBuf,buf,bufSize);
		chunkEle* cep = resolveChunk(peer, chunkList);
		

		// reject packets from finished chunk
		if(cep && (cep->chunkId != ((packetHead*)buf)->ackNum)){
			printf("Wrong chunk!\n");
			break;
		}

		orderedAdd(cep,newBuf);

		printf("Receive data packet %d, with size %d \n", ((packetHead *)buf)->seqNum, ((packetHead *)buf)->packLen - headerSize);

		findMex(cep);
		printPacketList(cep->packetList);
		void* packet = ackCons(cep->nextExpectedSeq - 1);

		printf("Send Ack %d\n", cep->nextExpectedSeq - 1);
		spiffy_sendto(sock, packet, headerSize, 0, (struct sockaddr *) &peer->cli_addr, sizeof(peer->cli_addr));
		//sendto(sock, packet, headerSize, 0, (struct sockaddr *) &peer->cli_addr, sizeof(peer->cli_addr));
		printf("bytes read for hash %s : %d\n",cep->chunkHash, cep->bytesRead);
		//check if finish receiving the whole chunk
		if(cep->bytesRead == chunkSize ){
			if ( cep->fromThisPeer->inUse == 1){
			printf("Sucessfully receive the chunk %s\n", cep->chunkHash);
			chunkList.finished ++;
			cep->fromThisPeer->inUse = 0;
			cep->inProgress = 0;
			mePeer->numOut--;
			}
			printf("FINISHED %d\n", chunkList.finished);
			if(chunkList.finished == chunkList.length){
				printf("Finish receiving the whole file\n");
				// merge the chunks and write to the corresponding output file
				buildOuputFile(outputFile, &chunkList);
				fclose(outputFile);
				printf("GOT %s\n",outputFilePath);
			}else{
				// try to send rest of pending get requests 
				// they are deferred to make sure no concurrent downloading from the same peer
				sendPendingGetRequest(&chunkList, sock);
			}
		}
		
		break;
	}
	case ACK:{
		chunkEle* cep = resolveChunk(peer, windowSets);

		if ( cep->inProgress == 0){
			break;
		}
		//check which wether SLOWSTART or CONGAVOID
		if(cep->mode == SLOWSTART){
			cep->windowSize++;
			printf("####in slow start\n");
		}
		else
			printf("@@@@in cong avoid rtt: %f\n",cep->fromThisPeer->rtt);
		//check if enough to enter cong avoid
		if(cep->windowSize == cep->ssthresh)
			cep->mode = CONGAVOID;

		//check for no ack in rtt
		if((cep->mode == CONGAVOID) && (cep->haveACK == 0)){
			cep->windowSize++;
			printf("IIIIincreasing in cong avoid\n");
		}

		//set ack
		cep->haveACK = 1;

		printf("Receive Ack %d\n", *(int*)(buf+12));
		if( (cep->lastAcked) && ((packetHead* )(cep->lastAcked->data))->seqNum == ((packetHead* )(buf))->ackNum ){
			cep->lastAckedCount ++;
			printf("Incrementing last ack counter for %d to %d\n", ((packetHead* )(buf))->ackNum,cep->lastAckedCount);
			if( cep->lastAckedCount  == 3 ){
				//cut ssthresh in half and set window 1
				cep->ssthresh = halve(cep->windowSize);
				cep->windowSize = 1;
				cep->mode = SLOWSTART;

				//reset this ones time
				time(&cep->afterLastAckedTime);

				//try retrasmit
				cep->lastAckedCount = 0;
				node* retry = cep->lastAcked->prevp;
				printf("Retramsmit packet %d\n", ((packetHead* )retry->data)->seqNum);
				spiffy_sendto(sock, retry->data, ((packetHead* )retry->data)->packLen, 0, (struct sockaddr *) &peer->cli_addr, sizeof(peer->cli_addr));
				break;
			}

		}else{
			cep->lastAcked = resolveLastPacketAcked( ((packetHead*)buf)->ackNum, cep);
			cleanList(cep);
			time(&cep->afterLastAckedTime);
		}
		
		//check if finish sending the whole chunk
		if( cep->bytesRead == chunkSize ){
			printf("Successfully send the chunk %s\n", cep->chunkHash);
			if(cep->lastAcked == cep->lastSent){
				// some clear below
				printf("All sent packets have been acked\n");
				cep->inProgress = 0;
				mePeer->numOut--;
			}else{
				printf("lastAcked:%d, lastSent:%d\n",((packetHead*)cep->lastAcked->data)->seqNum, ((packetHead*)cep->lastSent->data)->seqNum );
			}
			
		}else{
			cleanList(cep);
			fillWindow(cep);
			sendWindow(cep, sock);
		}

		printPacketList(cep->packetList);
		printf("window: %d, ssthresh: %d, mode: %d\n",cep->windowSize, cep->ssthresh,
			cep->mode);
		break;
	}
	case DENIED:{
		printf("Received a denied ack!\n");
		fclose(outputFile);
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
	
	strcpy(outputFilePath,outputfile);

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
	
  maxCon = config->max_conn;// rename max connections

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
		//printf("going to process_user_input\n");
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
		ele->numOut = 0;
		ele->numIn = 0;
		
		memcpy(ele->host, hostname, strlen(hostname));
		ele->port = port;
		ele->inUse = 0;
		
		node* newNode = initNode(ele);

		//a pointer to myslef in peer list
		if(ele->id == myId)
			mePeer = ele;

		addList(newNode, &peerList);
	}
	fclose(fp);
	return;
}

/* remove old elements up to last acked */
void cleanList(chunkEle* cep){
	node* itr = cep->packetList.headp->prevp;

	if(cep->lastAcked == NULL)
		return;


	while(cep->packetList.length > 0){
		if(itr == cep->lastAcked)
			return;
		node* temp = itr;
		remList(itr, &cep->packetList);
		free(itr->data);
		itr = itr->prevp;
		free(temp);
	}

	return;
}

/* make sure packelist is  window size or EOF */
void fillWindow(chunkEle* cep){
	int sizeToRead;
	unsigned int seqToSend; 
	int i;

	for(i = cep->packetList.length; ((i < cep->windowSize) && (cep->bytesRead != chunkSize)); i++){
		sizeToRead = min(chunkSize - cep->bytesRead , 1500-headerSize);
		seqToSend = ((packetHead* )cep->packetList.headp->data)->seqNum + 1;
		cep->bytesRead += sizeToRead;
		void* packet = nextDataPacket(cep->masterfp, seqToSend , sizeToRead, cep->chunkId);
		node* newNode = initNode(packet);
		addList(newNode,&cep->packetList);
	}
}

/* send packets until window is maxed out */
void sendWindow(chunkEle* cep, int sock){

	// no acks yet, send single packet window
	if( cep->lastAcked == NULL ){
		void* packet = cep->packetList.headp->data;
		unsigned int bufSize = ((packetHead *)packet)->packLen;

		spiffy_sendto(sock, packet, bufSize, 0, (struct sockaddr *) &cep->fromThisPeer->cli_addr, sizeof(cep->fromThisPeer->cli_addr));
		//sendto(sock, packet, bufSize, 0, (struct sockaddr *) &cep->fromThisPeer->cli_addr, sizeof(cep->fromThisPeer->cli_addr));		
	
		cep->lastSent = cep->packetList.headp;
		return;
	}

	//while window isn't exhausted
	while( ( ((packetHead*)cep->lastSent->data)->seqNum - ((packetHead*)cep->lastAcked->data)->seqNum ) < cep->packetList.length - 1 ){
		cep->lastSent = cep->lastSent->prevp; 
		void* packet = cep->lastSent->data;
		unsigned int bufSize = ((packetHead *)packet)->packLen;

		spiffy_sendto(sock, packet, bufSize, 0, (struct sockaddr *) &cep->fromThisPeer->cli_addr, sizeof(cep->fromThisPeer->cli_addr));
		//sendto(sock, packet, bufSize, 0, (struct sockaddr *) &cep->fromThisPeer->cli_addr, sizeof(cep->fromThisPeer->cli_addr));
	}

	return;
}


void parseChunkFile(char* chunkfile, linkedList* list){
	cleanChunkList(list);
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


/* re-send everything from last acked to last sent */
void alarmHandler(int sig){
	struct itimerval tout_val;
	node* curWindow;
	int i;
	time_t curTime;
	double dif;

  	signal(SIGALRM,alarmHandler);
   
   	tout_val.it_interval.tv_sec = 0;
   	tout_val.it_interval.tv_usec = 0;
   	tout_val.it_value.tv_sec = 0; 
   	tout_val.it_value.tv_usec = 100000; // 100 ms

   	curWindow = windowSets.headp;

   	//traverse windowSets
	for(i = 0; i < windowSets.length; i++){
	
		chunkEle* cep = (chunkEle*) curWindow->data;
	// which means not all packets have been acked yet	
	if(cep->inProgress == 1){
		time(&curTime);
		dif = difftime(curTime,cep->afterLastAckedTime);

		if(dif >= timeout){
			sendAfterLastAcked(cep);
			time(&cep->afterLastAckedTime);

			//cut ssthresh in half and set window 1
			cep->ssthresh = halve(cep->windowSize);
			cep->windowSize = 1;
			cep->mode = SLOWSTART;
		}

		//reset rtt timer
		dif = difftime(curTime,cep->rttCounter);
		if(dif >= cep->fromThisPeer->rtt){
			cep->haveACK = 0;
			cep->rttCounter = curTime;
		}
	}
		curWindow = curWindow->prevp;
	}   

   	setitimer(ITIMER_REAL, &tout_val,0);
 	setitimer(ITIMER_REAL, &tout_val,0);

 	return;
}

void sendAfterLastAcked(chunkEle* cep){
	node* cur;

	if(cep->lastSent == NULL)
		return;

	//last acked is head if nothing acked yet
	if(cep->lastAcked == NULL)
		cur = cep->packetList.headp;
	else 
		cur = cep->lastAcked->prevp;

	do{
		void* packet = cur->data;
		unsigned int bufSize = ((packetHead *)packet)->packLen;
		//printf("***Timeout, retrans packet %d\n to peer %d***\n", ((packetHead* )packet)->seqNum, cep->fromThisPeer->id );
		spiffy_sendto(sock, packet, bufSize, 0, (struct sockaddr *) &cep->fromThisPeer->cli_addr, sizeof(cep->fromThisPeer->cli_addr));
		//sendto(sock, ihavep, bufSize, 0, (struct sockaddr *) &from, fromlen);
		cur = cur->prevp;
	}while(cur != cep->lastSent->prevp);

	return;
}

/* handle ^C */
void exitFunc(){
	signal(SIGINT,exitFunc);
    printf("\nBye Bye!!!\n");
    exit(0);
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
