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
	}
	else{
		toAdd->nextp = listp->headp;
		toAdd->prevp = listp->headp->prevp;
		toAdd->nextp->prevp = toAdd;
		toAdd->prevp->nextp = toAdd;
	}
	
	listp->headp = toAdd;
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
		}
		
		itr = itr->nextp;
	}while(itr != listp->headp);
	
	return;
}