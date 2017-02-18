/*****************************************************************************
* \file      osutil.h
* \author    Conny Gustafsson
* \date      2014-10-24
* \brief     helper functions for osmacro header
* \details   https://github.com/cogu/msocket
*
* Copyright (c) 2014-2016 Conny Gustafsson
*
******************************************************************************/
#ifndef OSUTIL_H
#define OSUTIL_H


/********************************* Includes **********************************/
#include "osmacro.h"
/**************************** Constants and Types ****************************/
#define SEMAPHORE_EV_POST(sem) _sem_ev_post(&sem);
#define PROG_MAX_STACK_SIZE 65536
/********************************* Functions *********************************/
int8_t _sem_test(SEMAPHORE_T *sem);
void _sem_ev_post(SEMAPHORE_T *sem);

#endif //OSUTIL_H
