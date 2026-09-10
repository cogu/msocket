/*****************************************************************************
* \file:    msocket_adt.h
* \author:  Conny Gustafsson
* \date:    2020-11-16
* \brief:   Standalone version of datastructures derived from github.com/cogu/adt
*
* Copyright (c) 2020 Conny Gustafsson
*
******************************************************************************/

#ifndef MSOCKET_ADT_H
#define MSOCKET_ADT_H
#ifdef __cplusplus
extern "C" {
#endif

//////////////////////////////////////////////////////////////////////////////
// INCLUDES
//////////////////////////////////////////////////////////////////////////////
#include <stdint.h>
#include <stdbool.h>


//////////////////////////////////////////////////////////////////////////////
// CONSTANTS AND DATA TYPES
//////////////////////////////////////////////////////////////////////////////
#define ADT_NO_ERROR                   0
#define ADT_INVALID_ARGUMENT_ERROR     1
#define ADT_MEM_ERROR                  2
#define ADT_INDEX_OUT_OF_BOUNDS_ERROR  3
#define ADT_LENGTH_ERROR               4
#define ADT_ARRAY_TOO_LARGE_ERROR      5
#define ADT_NOT_IMPLEMENTED_ERROR      6
#define ADT_UNKNOWN_ENCODING_ERROR     7
#define ADT_OBJECT_COMPARE_ERROR       8
typedef int8_t msocket_adt_error_t;

typedef struct msocket_bytearray_tag
{
   uint8_t* pData;
   uint32_t u32CurLen;
   uint32_t u32AllocLen;
   uint32_t u32GrowSize;
} msocket_bytearray_t;

typedef struct msocket_ary_tag
{
   void** ppAlloc;		       //array of (void*)
   void** pFirst;		          //pointer to first array element
   int32_t s32AllocLen;	       //number of elements allocated
   int32_t s32CurLen;	       //number of elements currently in the array
   void (*pDestructor)(void*); //optional destructor function (typically vdelete functions from other data structures)
   void* pFillElem;            //optional fill element for new elements (defaults to NULL)
   bool destructorEnable;      //Temporarily disables use of element pDestructor
} msocket_ary_t;

#define MSOCKET_BYTEARRAY_NO_GROWTH 0u  //will malloc exactly the number of bytes it currently needs
#define MSOCKET_BYTEARRAY_DEFAULT_GROW_SIZE ((uint32_t)8192u)
#define MSOCKET_BYTEARRAY_MAX_GROW_SIZE ((uint32_t)32u*1024u*1024u)


//////////////////////////////////////////////////////////////////////////////
// FUNCTION PROTOTYPES
//////////////////////////////////////////////////////////////////////////////
void msocket_bytearray_create(msocket_bytearray_t* self, uint32_t u32GrowSize);
void msocket_bytearray_destroy(msocket_bytearray_t* self);
msocket_adt_error_t msocket_bytearray_reserve(msocket_bytearray_t* self, uint32_t u32NewLen);
msocket_adt_error_t msocket_bytearray_grow(msocket_bytearray_t* self, uint32_t u32MinLen);
msocket_adt_error_t msocket_bytearray_append(msocket_bytearray_t* self, const uint8_t* pData, uint32_t u32DataLen);
msocket_adt_error_t msocket_bytearray_trimLeft(msocket_bytearray_t* self, const uint8_t* pSrc);
uint8_t* msocket_bytearray_data(const msocket_bytearray_t* self);
uint32_t msocket_bytearray_length(const msocket_bytearray_t* self);
void msocket_bytearray_clear(msocket_bytearray_t* self);

void msocket_ary_create(msocket_ary_t* self, void (*pDestructor)(void*));
void msocket_ary_destroy(msocket_ary_t* self);
msocket_adt_error_t msocket_ary_push(msocket_ary_t* self, void* pElem);
void* msocket_ary_shift(msocket_ary_t* self);
int32_t msocket_ary_length(const msocket_ary_t* self);
msocket_adt_error_t msocket_ary_extend(msocket_ary_t* self, int32_t s32Len);



#ifdef __cplusplus
}
#endif

#endif //MSOCKET_ADT_H
