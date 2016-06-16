/*****************************************************************************
* \file      osutil.c
* \author    Conny Gustafsson
* \date      2014-10-24
* \brief     helper functions for osmacro header
* \details   https://github.com/cogu/msocket
*
* Copyright (c) 2014-2016 Conny Gustafsson
*
******************************************************************************/

/********************************* Includes **********************************/
#ifdef _WIN32
#include <Windows.h>
#else
#include <semaphore.h>
#endif
#include <errno.h>
#include <assert.h>
#include "osutil.h"
/**************************** Constants and Types ****************************/

/************************* Local Function Prototypes *************************/

/********************************* Variables *********************************/

/***************************** Exported Functions ****************************/

//returns 1 if semaphore was successfully decreased
sint8 _sem_test(SEMAPHORE_T *sem){
#ifdef _WIN32
   DWORD rc;
     rc = WaitForSingleObject(*sem,0);
     switch(rc){
     case WAIT_OBJECT_0:
        return 1;
     case WAIT_TIMEOUT:
        return 0;
     }
     return -1;
#else
   int rc;
   rc = sem_trywait(sem);
   if(rc<0){
      if(errno == EAGAIN){
         return 0;
      }
      else{
         return -1;
      }
   }
   assert(rc == 0);
   return 1;
#endif
}

//increases count of semaphore
void _sem_post(SEMAPHORE_T *sem){
   sem_post(sem);
}

//increases count of semaphore up to count of 1
//for Win32 it is assumed that the caller created the semaphore using the SEMAPHORE_EV_CREATE-macro
void _sem_ev_post(SEMAPHORE_T *sem){
#ifdef _WIN32
   SEMAPHORE_POST(*sem);
#else
   int val=0;
   int rc = sem_getvalue(sem,&val);
   if( (rc==0) && (val==0) ){
      sem_post(sem);
      rc = sem_getvalue(sem,&val);
   }
#endif
}

/****************************** Local Functions ******************************/
