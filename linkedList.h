#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct n{
	struct n* nextp;
	struct n* prevp;
	void* data;
}node;

typedef struct ll{
	node* headp;
	unsigned int length;
}linkedList;

void addList(node* toAdd, linkedList* listp);
void remList(node* toRem, linkedList* listp);