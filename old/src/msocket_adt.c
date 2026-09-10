/*****************************************************************************
* \file:    msocket_adt.h
* \author:  Conny Gustafsson
* \date:    2020-11-16
* \brief:   Standalone version of datastructures derived from github.com/cogu/adt
*
* Copyright (c) 2020 Conny Gustafsson
*
******************************************************************************/

//////////////////////////////////////////////////////////////////////////////
// INCLUDES
//////////////////////////////////////////////////////////////////////////////
#include "msocket_adt.h"
#include <malloc.h>
#include <string.h>
#include <assert.h>

//////////////////////////////////////////////////////////////////////////////
// CONSTANTS AND DATA TYPES
//////////////////////////////////////////////////////////////////////////////
#define ELEM_SIZE (sizeof(void*))

//////////////////////////////////////////////////////////////////////////////
// LOCAL FUNCTION PROTOTYPES
//////////////////////////////////////////////////////////////////////////////
static msocket_adt_error_t msocket_bytearray_realloc(msocket_bytearray_t *self, uint32_t u32NewLen);


//////////////////////////////////////////////////////////////////////////////
// LOCAL VARIABLES
//////////////////////////////////////////////////////////////////////////////


//////////////////////////////////////////////////////////////////////////////
// GLOBAL FUNCTIONS
//////////////////////////////////////////////////////////////////////////////

/*** msocket_bytearray API ***/

void msocket_bytearray_create(msocket_bytearray_t *self,uint32_t u32GrowSize){
   if(self){
      self->pData = 0;
      self->u32AllocLen = 0;
      self->u32CurLen = 0;
      if (u32GrowSize > MSOCKET_BYTEARRAY_MAX_GROW_SIZE) {
         self->u32GrowSize = MSOCKET_BYTEARRAY_MAX_GROW_SIZE;
      }
      else {
         self->u32GrowSize = u32GrowSize;
      }
   }
}

void msocket_bytearray_destroy(msocket_bytearray_t *self){
   if(self){
      if(self->pData != 0){
         free(self->pData);
         self->pData = 0;
      }
   }
}

msocket_adt_error_t msocket_bytearray_reserve(msocket_bytearray_t *self, uint32_t u32NewLen){
   if(self){
      if(u32NewLen > self->u32AllocLen){
         msocket_adt_error_t errorCode = msocket_bytearray_grow(self,u32NewLen);
         if(errorCode != ADT_NO_ERROR){
            return errorCode;
         }
      }
      return ADT_NO_ERROR;
   }
   return ADT_INVALID_ARGUMENT_ERROR;
}

msocket_adt_error_t msocket_bytearray_append(msocket_bytearray_t *self, const uint8_t *pData, uint32_t u32DataLen){
   if(self && pData && (u32DataLen > 0)){
      msocket_adt_error_t errorCode = msocket_bytearray_reserve(self, self->u32CurLen + u32DataLen);
      if(errorCode == ADT_NO_ERROR){
         uint8_t *pNext, *pEnd;
         pNext = self->pData + self->u32CurLen;
         pEnd = self->pData + self->u32AllocLen;
         assert(pNext + u32DataLen <= pEnd);
         memcpy(pNext,pData,u32DataLen);
         self->u32CurLen+=u32DataLen;
      }
      return errorCode;
   }
   return ADT_INVALID_ARGUMENT_ERROR;
}

/**
 * Removes all bytes to the left of pSrc, saves all bytes to the right of pSrc (including pSrc itself)
 * \param self pointer to bytearray_t
 * \param pSrc pointer to a byte inside the array
 */
msocket_adt_error_t msocket_bytearray_trimLeft(msocket_bytearray_t *self, const uint8_t *pSrc){
   if( (self!=0) && (pSrc!=0) && (self->pData <= pSrc) && (pSrc <= self->pData + self->u32CurLen) ){
      uint32_t start, remain;
      /*
       * boundary cases:
       *    pBegin = self->pData
       *    =>
       *       start = 0
       *       remain = self->u32CurLen
       *
       *    pBegin = self->pData+self->u32CurLen
       *    =>
       *       start = self->u32CurLen
       *       remain = 0
       */
      start = (uint32_t) (pSrc - self->pData);
      remain = self->u32CurLen - start;
      if(pSrc == self->pData){
         //no action
         assert(start == 0);
      }
      else if(remain == 0){
         //remove all
         self->u32CurLen = 0;
      }
      else{
         memmove(self->pData,pSrc,remain);
         self->u32CurLen = remain;
      }
      return ADT_NO_ERROR;
   }
   return ADT_INVALID_ARGUMENT_ERROR;
}

/**
 * grows byte array by a predefined size
 */
msocket_adt_error_t msocket_bytearray_grow(msocket_bytearray_t *self, uint32_t u32MinLen){
   if( self != 0 ){
      if (u32MinLen > self->u32AllocLen) {
         uint32_t u32NewLen = self->u32AllocLen;
         if (self->u32GrowSize > 0){
            while(u32NewLen<u32MinLen){
               u32NewLen += self->u32GrowSize;
            }
         }
         else
         {
            u32NewLen = u32MinLen;
         }
         return msocket_bytearray_realloc(self, u32NewLen);
      }
      return ADT_NO_ERROR;
   }
   return ADT_INVALID_ARGUMENT_ERROR;
}

uint8_t *msocket_bytearray_data(const msocket_bytearray_t *self){
   if(self != 0){
      return self->pData;
   }
   return 0;
}

uint32_t msocket_bytearray_length(const msocket_bytearray_t *self){
   if(self != 0){
      return self->u32CurLen;
   }
   return 0;
}

void msocket_bytearray_clear(msocket_bytearray_t *self){
   if(self != 0){
      self->u32CurLen = 0;
   }
}

/*** msocket_ary API ***/

void msocket_ary_create(msocket_ary_t* self, void (*pDestructor)(void*)) {
   self->ppAlloc = (void**)0;
   self->pFirst = (void**)0;
   self->s32AllocLen = 0;
   self->s32CurLen = 0;
   self->pDestructor = pDestructor;
   self->pFillElem = (void*)0;
   self->destructorEnable = true;
}

void msocket_ary_destroy(msocket_ary_t* self) {
   int32_t s32i;

   void** ppElem = self->pFirst;
   if ((self->pDestructor != 0) && (self->destructorEnable != false)) {
      for (s32i = 0; s32i < (int32_t)self->s32CurLen; s32i++) {
         self->pDestructor(*(ppElem++));
      }
   }
   if (self->ppAlloc != 0) {
      free(self->ppAlloc);
   }
   self->ppAlloc = (void**)0;
   self->s32AllocLen = 0;
   self->pFirst = (void**)0;
   self->s32CurLen = 0;
}

msocket_adt_error_t	msocket_ary_push(msocket_ary_t* self, void* pElem) {
   if (self != 0) {
      int32_t s32Index;
      msocket_adt_error_t result;
      s32Index = self->s32CurLen;
      assert(self->s32CurLen < INT32_MAX);
      result = msocket_ary_extend(self, ((int32_t)s32Index + 1));
      if (result == ADT_NO_ERROR) {
         self->pFirst[s32Index] = pElem;
      }
      return result;
   }
   return ADT_INVALID_ARGUMENT_ERROR;
}

void* msocket_ary_shift(msocket_ary_t* self) {
   void* pElem;
   assert((self != 0));
   if (self->s32CurLen == 0) {
      return (void*)0;
   }
   pElem = *(self->pFirst++); //move pFirst forward by 1
   self->s32CurLen--; //reduce array length by 1
   if (self->s32CurLen == 0) {
      //reallign pFirst with pAlloc when buffer becomes empty
      self->pFirst = self->ppAlloc;
   }
   return pElem;
}

int32_t msocket_ary_length(const msocket_ary_t* self) {
   if (self) {
      return self->s32CurLen;
   }
   return -1;
}

msocket_adt_error_t	msocket_ary_extend(msocket_ary_t* self, int32_t s32Len) {
   if (self != 0) {
      void** ppAlloc;
      //check if current length is greater than requested length
      if (self->s32CurLen >= s32Len) return ADT_NO_ERROR;

      //check if allocated length is greater than requested length
      if ((self->s32AllocLen >= s32Len)) {
         //shift array data to start of allocated array
         memmove(self->ppAlloc, self->pFirst, ((unsigned int)self->s32CurLen) * sizeof(void*));
         self->pFirst = self->ppAlloc;
         self->s32CurLen = s32Len;
      }
      else {
         //need to allocate new array data element and copy data to newly allocated memory
         if (s32Len >= INT32_MAX) {
            return ADT_LENGTH_ERROR;
         }
         ppAlloc = (void**)malloc(ELEM_SIZE * ((unsigned int)s32Len));
         if (ppAlloc == 0)
         {
            return ADT_MEM_ERROR;
         }
         if (self->ppAlloc) {
            size_t numNewElems = ( ((size_t)s32Len) - self->s32CurLen);
            memset(ppAlloc + self->s32CurLen, 0, numNewElems * ELEM_SIZE);
            memcpy(ppAlloc, self->pFirst, ((unsigned int)self->s32CurLen) * ELEM_SIZE);
            free(self->ppAlloc);
         }
         self->ppAlloc = self->pFirst = ppAlloc;
         self->s32AllocLen = self->s32CurLen = s32Len;
      }
      return ADT_NO_ERROR;
   }
   return ADT_INVALID_ARGUMENT_ERROR;
}


//////////////////////////////////////////////////////////////////////////////
// LOCAL FUNCTIONS
//////////////////////////////////////////////////////////////////////////////

static msocket_adt_error_t msocket_bytearray_realloc(msocket_bytearray_t *self, uint32_t u32NewLen) {
   if (self != 0) {
      uint8_t *pNewData = (uint8_t*) malloc(u32NewLen);
      if(pNewData != 0){
         if(self->pData != 0) {
            uint32_t u32CopyLen = (u32NewLen < self->u32CurLen)? u32NewLen : self->u32CurLen;
            memcpy(pNewData, self->pData, u32CopyLen);
            free(self->pData);
         }
         self->pData = pNewData;
         self->u32AllocLen = u32NewLen;
      }
      else {
         return ADT_MEM_ERROR;
      }
      return ADT_NO_ERROR;
   }
   return ADT_INVALID_ARGUMENT_ERROR;
}