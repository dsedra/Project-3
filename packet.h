#include "linkedList.h"
#define headerSize 16
#define numChunks 4 /* 1 byte for num chunks, rest padding */
#define sizeofHash 20

#define WHOHAS 0
#define IHAVE 1
#define GET 2
#define DATA 3
#define ACK 4
#define DENIED 5

#define MAGICNUM 15441
#define VERSION 1
#define MAXCHUNKS 74/* max chunks per packet */


void* whohasCons(linkedList* listp, unsigned int start);
