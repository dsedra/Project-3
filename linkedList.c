#include "linkedList.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void addList(node* toAdd, linkedList* listp){
	if(listp == NULL)
		return;
	
	if(listp->headp == NULL){
		toAdd->nextp = toAdd;
		toAdd->prevp = toAdd;
		listp->headp = toAdd;
	}
	else{
		toAdd->nextp = listp->headp;
		toAdd->prevp = listp->headp->prevp;
		toAdd->nextp->prevp = toAdd;
		toAdd->prevp->nextp = toAdd;
	}
	
	listp->length++;
	return;
}

void remList(node* toRem, linkedList* listp){
	if((listp == NULL) || (listp->headp == NULL))
		return;
	
	node* itr = listp->headp;
	
	do{
		if(itr == toRem){
			toRem->nextp->prevp = toRem->prevp;
			toRem->prevp->nextp = toRem->nextp;
		
			if(toRem->nextp == toRem)
				listp->headp = NULL;
			else if(toRem == listp->headp)
				listp->headp = toRem->nextp;
		
			free(toRem->data);
			listp->length--;
		}
		
		itr = itr->nextp;
	}while(itr != listp->headp);
	
	return;
}

node* initNode(void* data){
	node* newNode = malloc(sizeof(node));
	newNode->data = data;
	newNode->nextp = NULL;
	newNode->prevp = NULL;
	
	return newNode;
}