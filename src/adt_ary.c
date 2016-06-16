/*****************************************************************************
* \file:   		adt_array.c
* \author:		Conny Gustafsson
* \date:		2013-06-03
* \brief:		array data structure
* \details		https://github.com/cogu/adt
*
* Copyright (c) 2013-2014 Conny Gustafsson
*
******************************************************************************/
#include "adt_ary.h"
#include <malloc.h>
#include <assert.h>
#include <string.h>
#ifdef MEM_LEAK_TEST
#include "CMemLeak.h"
#endif


#define DATA_BLOCK_MAX 65536 	//maximum amount of bytes that can be copied in memmmove is implementation specific,
								      //use define to control how many bytes shall be copied


/**************** Private Function Declarations *******************/

/**************** Private Variable Declarations *******************/


/****************** Public Function Definitions *******************/
adt_ary_t*	adt_ary_new(void (*pDestructor)(void*)){
	adt_ary_t *self;
	if((self = malloc(sizeof(adt_ary_t)))==(adt_ary_t*)0){
		return (adt_ary_t*)0;
	}
	adt_ary_create(self,pDestructor);
	return self;
}
adt_ary_t*	adt_ary_make(void** ppElem, int32_t s32Len,void (*pDestructor)(void*)){
	adt_ary_t *self;
	int32_t s32i;
	void **ppDest,**ppSrc;

	if(s32Len>= INT32_MAX){
		//array length too long
		return (adt_ary_t*) 0;
	}

	self = adt_ary_new(pDestructor);
	if(self==(adt_ary_t*)0){
		return (adt_ary_t*)0;
	}
	adt_ary_extend(self,s32Len);
	ppDest=self->pFirst;
	ppSrc=ppElem;
	for(s32i=0;s32i<s32Len;s32i++){
		*(ppDest++) = *(ppSrc++);
	}
	return self;
}
void adt_ary_delete(adt_ary_t *self){
	if(self != 0){
		adt_ary_destroy(self);
		free(self);
	}
}

void adt_ary_destructorEnable(adt_ary_t *self, uint8_t enable){
   if(self != 0){
      self->destructorEnable = (enable != 0)? 1 : 0;
   }
}

//Accessors
void**	adt_ary_set(adt_ary_t *self, int32_t s32Index, void *pElem){
	uint32_t u32Index;
	if(self==0){
		return (void**)0;
	}
	if(s32Index<0){
		u32Index = (uint32_t) (-s32Index);
		if(u32Index > (self->u32CurLen) ){
			//negative index outside array bounds
			return (void**) 0;
		}
		//negative index inside array bounds
		u32Index=self->u32CurLen-u32Index;
	}
	else{
		u32Index = (uint32_t) s32Index;
	}
	adt_ary_fill(self,((int32_t) u32Index)+1);
	self->pFirst[u32Index]=pElem;
	return &self->pFirst[u32Index];
}

void**	adt_ary_get(adt_ary_t *self, int32_t s32Index){
	uint32_t u32Index;
	if(self==0){
		return (void**)0;
	}
	if(s32Index<0){
		u32Index = (int32_t) (-s32Index);
		if(u32Index > (self->u32CurLen) ){
			//negative index outside array bounds
			return (void**) 0;
		}
		//negative index inside array bounds
		u32Index=self->u32CurLen-u32Index;
	}
	else{
		u32Index = (int32_t) s32Index;
	}
	adt_ary_fill(self,(int32_t) (u32Index+1));
	return &self->pFirst[u32Index];
}

void	adt_ary_push(adt_ary_t *self, void *pElem){
	int32_t u32Index;
	assert( (self != 0) && (pElem != 0));
	u32Index = self->u32CurLen;
	assert(self->u32CurLen < INT32_MAX);
	adt_ary_extend(self,u32Index+1);
	self->pFirst[u32Index]=pElem;
}
void*	adt_ary_pop(adt_ary_t *self){
	void *pElem;
	assert( (self!=0) );
	if(self->u32CurLen==0){
		return (void*) 0;
	}
	pElem = self->pFirst[--self->u32CurLen];
	if(self->u32CurLen == 0){
		//reallign pFirst with pAlloc when buffer becomes empty
		self->pFirst = self->ppAlloc;
	}
	return pElem;
}
void*	adt_ary_shift(adt_ary_t *self){
	void *pElem;
	assert( (self!=0) );
	if(self->u32CurLen==0){
		return (void*)0;
	}
	pElem=*(self->pFirst++); //move pFirst forward by 1
	self->u32CurLen--; //reduce array length by 1
	if(self->u32CurLen == 0){
		//reallign pFirst with pAlloc when buffer becomes empty
		self->pFirst = self->ppAlloc;
	}
	return pElem;
}
void	adt_ary_unshift(adt_ary_t *self, void *pElem){
	assert( (self != 0) && (pElem != 0));
	assert(self->u32CurLen < INT32_MAX);
	if(self->pFirst > self->ppAlloc){
		//room for one more element at the beginning
		*(--self->pFirst)=pElem;
		self->u32CurLen++;
	}
	else{
		//no room at beginning of array, move all array data forward by one
		int32_t u32Remain,u32ElemSize;
		uint8_t *pBegin,*pEnd;
		u32ElemSize = sizeof(void**);
		adt_ary_extend(self,self->u32CurLen+1);
		pBegin = (uint8_t*) self->pFirst+u32ElemSize;
		pEnd = ((uint8_t*) &self->pFirst[self->u32CurLen])-1;
		u32Remain = pEnd-pBegin+1;
		while(u32Remain>0){
			uint32_t u32Size = (u32Remain>DATA_BLOCK_MAX)? DATA_BLOCK_MAX : u32Remain;
			memmove(pBegin,pBegin-u32ElemSize,u32Size);
			u32Remain-=u32Size;
			pBegin+=u32Size;
		}
		self->pFirst[0]=pElem;
	}
}


//Utility functions
void	adt_ary_extend(adt_ary_t *self, int32_t s32Len){
	void **ppAlloc;
	uint32_t u32Len = (uint32_t) s32Len;
	assert(self);
	//check if current length is greater than requested length
	if( self->u32CurLen>=u32Len ) return;

	//check if allocated length is greater than requested length
	if( (self->u32AllocLen>=u32Len) ){
	  //shift array data to start of allocated array
      memmove(self->ppAlloc,self->pFirst,self->u32CurLen * sizeof(uint16_t));
      self->pFirst = self->ppAlloc;
      self->u32CurLen = u32Len;
	}
	else {
		//need to allocate new array data element and copy data to newly allocated memory
		if(u32Len>= INT32_MAX){
			//invalid argument
			return;
		}
		ppAlloc = (void**) malloc(sizeof(void*)*u32Len);
		assert(ppAlloc != 0);
		if(self->ppAlloc){
			memset(ppAlloc,0,self->u32CurLen * sizeof(void*));
			memcpy(ppAlloc,self->pFirst,self->u32CurLen * sizeof(void*));
			free(self->ppAlloc);
		}
		self->ppAlloc = self->pFirst = ppAlloc;
		self->u32AllocLen = self->u32CurLen = u32Len;
	}
}
void adt_ary_fill(adt_ary_t *self, int32_t s32Len){
	int32_t u32Index;
	uint32_t u32Len = (uint32_t) s32Len;
	uint32_t u32CurLen = self->u32CurLen;
	assert( self != 0);
	if(self->u32CurLen >= u32Len) return; //increase not necessary
	adt_ary_extend(self,u32Len);
	//set undef to all newly created array elements
	for(u32Index=u32CurLen; u32Index<u32Len; u32Index++){
		self->pFirst[u32Index]=self->pFillElem;
	}
	assert(self->u32CurLen>=u32Len);
	assert(u32Index==self->u32CurLen);
}
void adt_ary_clear(adt_ary_t *self){
	if(self){
		adt_ary_destroy(self);
	}
}

uint32_t adt_ary_length(adt_ary_t *self){
	if(self){
		return self->u32CurLen;
	}
	return 0;
}

//Returns nonzero if the element exists
int32_t	adt_ary_exists(adt_ary_t *self, int32_t s32Index){
	int32_t u32Index;
	if(self==0){
		return 0;
	}
	if(s32Index<0){
		u32Index = (int32_t) (-s32Index);
		if(u32Index > (self->u32CurLen) ){
			//negative index outside array bounds
			return 0;
		}
		//negative index inside array bounds
		u32Index=self->u32CurLen-u32Index;
	}
	else{
		u32Index = (int32_t) s32Index;
	}
	if(u32Index<self->u32CurLen){
		return 1;
	}
	return 0;
}

void adt_ary_create(adt_ary_t *self,void (*pDestructor)(void*)){
	self->ppAlloc = (void**) 0;
	self->pFirst = (void**) 0;
	self->u32AllocLen = 0;
	self->u32CurLen = 0;
	self->pDestructor = pDestructor;
	self->pFillElem = (void*)0;
}

void adt_ary_destroy(adt_ary_t *self){
	int32_t s32i;

	void **ppElem=self->pFirst;
	if( (self->pDestructor != 0) && (self->destructorEnable != 0) ){
		for(s32i=0;s32i<self->u32CurLen;s32i++){
			self->pDestructor(*(ppElem++));
		}
	}
	if(self->ppAlloc != 0){
		free(self->ppAlloc);
	}
	self->ppAlloc = (void**) 0;
	self->u32AllocLen = 0;
	self->pFirst = (void**) 0;
	self->u32CurLen = 0;
}

void 	adt_ary_set_fill_elem(adt_ary_t *self,void* pFillElem){
	if(self){
		self->pFillElem = pFillElem;
	}
}
void* 	adt_ary_get_fill_elem(adt_ary_t *self){
	if(self){
		return self->pFillElem;
	}
	return (void*)0;
}

void adt_ary_splice(adt_ary_t *self,int32_t s32Index, int32_t s32Len){
   uint32_t u32Index;
   uint32_t u32Source;
   uint32_t u32Destination;
   uint32_t u32Removed = 0;
   uint32_t i;
   if( (self==0) || (s32Len <= 0) ){
      return; //invalid input argument
   }
   if(s32Index<0){
      u32Index = (int32_t) (-s32Index);
      if(u32Index > (self->u32CurLen) ){
         return; //negative index out of bounds
      }
      //negative index inside array bounds
      u32Index=self->u32CurLen-u32Index;
   }
   else{
      u32Index = (int32_t) s32Index;
   }
   if(u32Index >= self->u32CurLen){
      return;
   }
   u32Destination = u32Index;
   u32Source = u32Index + ((uint32_t) s32Len);
   for(i=0;i<s32Len;i++){
      if(u32Source < self->u32CurLen){
         //move item
         assert(u32Destination != u32Source);
         assert(u32Destination < self->u32CurLen);
         assert(u32Source < self->u32CurLen);
         if( (self->destructorEnable != 0) && (self->pDestructor != 0) ){
            self->pDestructor(self->pFirst[u32Destination]);
         }
         u32Removed++;
         self->pFirst[u32Destination] = self->pFirst[u32Source];
         self->pFirst[u32Source] = self->pFillElem;
      }
      else if(u32Destination < self->u32CurLen){
         //remove item
         if( (self->destructorEnable != 0) && (self->pDestructor != 0) ){
            self->pDestructor(self->pFirst[u32Destination]);
         }
         u32Removed++;
         self->pFirst[u32Destination] = self->pFillElem;
      }
      u32Source++;
      u32Destination++;
   }
   //move remaining items
   while(u32Source < self->u32CurLen){
      self->pFirst[u32Destination] = self->pFirst[u32Source];
      self->pFirst[u32Source] = self->pFillElem;
      u32Source++;
      u32Destination++;
   }
   self->u32CurLen-=u32Removed;
}
/***************** Private Function Definitions *******************/
