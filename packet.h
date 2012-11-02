typedef struct ph{
	unsigned short magicNum, headerLen, packLen;
 	unsigned char version, type;
	unsigned int seqNum, ackNum;
}packetHead;

