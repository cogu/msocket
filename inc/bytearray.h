/*****************************************************************************
* \file:    bytearray.h
* \author:  Conny Gustafsson
* \date:    2015-02-05
* \brief:   general purpose data container
*
* Copyright (c) 2015-2016 Conny Gustafsson
*
******************************************************************************/
#ifndef BYTE_ARRAY_H
#define BYTE_ARRAY_H
#include <stdint.h>

typedef struct bytearray_t{
   uint8_t *pData;
   uint32_t u32CurLen;
   uint32_t u32AllocLen;
   uint32_t u32GrowSize;
} bytearray_t;

/***************** Public Function Declarations *******************/
void bytearray_create(bytearray_t *buf,uint32_t u32GrowSize);
void bytearray_destroy(bytearray_t *buf);
int8_t bytearray_reserve(bytearray_t *buf, uint32_t u32NewLen);
int8_t bytearray_grow(bytearray_t *buf, uint32_t u32MinLen);
int8_t bytearray_append(bytearray_t *buf, const uint8_t *pData, uint32_t u32DataLen);
int8_t bytearray_trimLeft(bytearray_t *buf, const uint8_t *pSrc);
uint8_t *bytearray_data(bytearray_t *buf);
uint32_t bytearray_length(bytearray_t *buf);
void bytearray_clear(bytearray_t *self);


#endif //BYTE_ARRAY_H
