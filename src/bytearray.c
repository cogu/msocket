/*****************************************************************************
* \file:    bytearray.c
* \author:  Conny Gustafsson
* \date:    2015-02-05
* \brief:   general purpose data container
*
* Copyright (c) 2015-2016 Conny Gustafsson
*
******************************************************************************/
#include "bytearray.h"
#include <errno.h>
#include <string.h>
#include <malloc.h>
#include <assert.h>


/**************** Private Function Declarations *******************/


/**************** Private Variable Declarations *******************/


/****************** Public Function Definitions *******************/
void bytearray_create(bytearray_t *buf,uint32_t u32GrowSize){
   if(buf){
      buf->pData = 0;
      buf->u32AllocLen = 0;
      buf->u32CurLen = 0;
      buf->u32GrowSize = u32GrowSize;
   }
}

void bytearray_destroy(bytearray_t *buf){
   if(buf){
      if(buf->pData != 0){
         free(buf->pData);
         buf->pData = 0;
      }
   }
}

int8_t bytearray_reserve(bytearray_t *buf, uint32_t u32NewLen){
   if(buf){
      if(u32NewLen > buf->u32AllocLen){
         int8_t rc = bytearray_grow(buf,u32NewLen);
         if(rc != 0){
            return rc;
         }
      }
      return 0;
   }
   return -1;
}

int8_t bytearray_append(bytearray_t *buf, const uint8_t *pData, uint32_t u32DataLen){
   if(buf && pData && (u32DataLen > 0)){
      if(bytearray_reserve(buf,buf->u32CurLen + u32DataLen) == 0){
         uint8_t *pNext, *pEnd;
         pNext = buf->pData + buf->u32CurLen;
         pEnd = buf->pData + buf->u32AllocLen;
         assert(pNext + u32DataLen <= pEnd);
         memcpy(pNext,pData,u32DataLen);
         buf->u32CurLen+=u32DataLen;
      }
      else{
         errno = ENOMEM;
         return -1;
      }
      return 0;
   }
   errno = EINVAL;
   return -1;
}

/**
 * Removes all bytes to the left of pSrc, saves all bytes to the right of pSrc (including pSrc itself)
 * \param buf pointer to bytearray_t
 * \param pSrc pointer to a byte inside the array
 */
int8_t bytearray_trimLeft(bytearray_t *buf, const uint8_t *pSrc){
   if( (buf!=0) && (pSrc!=0) && (buf->pData <= pSrc) && (pSrc <= buf->pData + buf->u32CurLen) ){
      uint32_t start, remain;
      /*
       * boundary cases:
       *    pBegin = buf->pData
       *    =>
       *       start = 0
       *       remain = buf->u32CurLen
       *
       *    pBegin = buf->pData+buf->u32CurLen
       *    =>
       *       start = buf->u32CurLen
       *       remain = 0
       */
      start = pSrc - buf->pData;
      remain = buf->u32CurLen - start;
      if(pSrc == buf->pData){
         //no action
         assert(start == 0);
      }
      else if(remain == 0){
         //remove all
         buf->u32CurLen = 0;
      }
      else{
         memmove(buf->pData,pSrc,remain);
         buf->u32CurLen = remain;
      }
      return 0;
   }
   errno = EINVAL;
   return -1;
}

/**
 * grows byte array by a predefined size
 */
int8_t bytearray_grow(bytearray_t *buf, uint32_t u32MinLen){
   if(buf != 0){
      uint8_t *pNewData;
      uint32_t u32NewLen = buf->u32AllocLen;
      while(u32NewLen<u32MinLen){
         u32NewLen += buf->u32GrowSize;;
      }
      pNewData = (uint8_t*) malloc(u32NewLen);
      if(pNewData){
         if(buf->pData != 0){
            memcpy(pNewData,buf->pData,buf->u32CurLen);
            free(buf->pData);
         }
         buf->pData = pNewData;
         buf->u32AllocLen = u32NewLen;
      }
      else{
         return -1;
      }
   }
   return 0;
}

uint8_t *bytearray_data(bytearray_t *buf){
   if(buf != 0){
      return buf->pData;
   }
   return 0;
}

uint32_t bytearray_length(bytearray_t *buf){
   if(buf != 0){
      return buf->u32CurLen;
   }
   return 0;
}

void bytearray_clear(bytearray_t *self){
   if(self != 0){
      self->u32CurLen = 0;
   }
}

/***************** Private Function Definitions *******************/

