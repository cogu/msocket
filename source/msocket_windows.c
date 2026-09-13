/*****************************************************************************
* \file:    msocket_windows.c
* \author:  Conny Gustafsson
* \date:    2026-09-10
* \brief:   Windows platform-specific threading, sync, and lifecycle management
*
* Copyright (c) 2026 Conny Gustafsson
* SPDX-License-Identifier: MIT
* See LICENSE in project root for full license terms.
******************************************************************************/

//////////////////////////////////////////////////////////////////////////////
// INCLUDES
//////////////////////////////////////////////////////////////////////////////
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "msocket.h"
#include "msocket_internal.h"

//////////////////////////////////////////////////////////////////////////////
// PRIVATE CONSTANTS AND DATA TYPES
//////////////////////////////////////////////////////////////////////////////
struct msocket_thread_t {
   HANDLE thread;
   unsigned int thread_id;
   void (*func)(void *arg);
   void *arg;
};

struct msocket_sem_t {
   HANDLE sem;
};

//////////////////////////////////////////////////////////////////////////////
// PRIVATE FUNCTION PROTOTYPES
//////////////////////////////////////////////////////////////////////////////
static unsigned __stdcall thread_runner(void *arg);

//////////////////////////////////////////////////////////////////////////////
// PLATFORM LIFECYCLE
//////////////////////////////////////////////////////////////////////////////
static int wsa_ref_count = 0;

void msocket_os_init(void)
{
   if (wsa_ref_count++ == 0) {
      WSADATA wsa_data;
      WSAStartup(MAKEWORD(2, 2), &wsa_data);
   }
}

void msocket_os_cleanup(void)
{
   if (--wsa_ref_count == 0) {
      WSACleanup();
   }
}

//////////////////////////////////////////////////////////////////////////////
// PLATFORM THREADING & SYNC
//////////////////////////////////////////////////////////////////////////////

msocket_thread_t *msocket_thread_create(void (*func)(void *arg), void *arg)
{
   msocket_thread_t *thread = (msocket_thread_t *)malloc(sizeof(msocket_thread_t));
   if (thread == NULL) {
      return NULL;
   }
   thread->func = func;
   thread->arg = arg;

   thread->thread = (HANDLE)_beginthreadex(NULL, 0u, thread_runner, thread, 0u, &thread->thread_id);
   if (thread->thread == NULL) {
      free(thread);
      return NULL;
   }
   return thread;
}

void msocket_thread_join(msocket_thread_t *thread)
{
   if (thread != NULL && thread->thread != NULL) {
      WaitForSingleObject(thread->thread, INFINITE);
   }
}

void msocket_thread_delete(msocket_thread_t *thread)
{
   if (thread != NULL) {
      if (thread->thread != NULL) {
         CloseHandle(thread->thread);
      }
      free(thread);
   }
}

bool msocket_thread_is_current(msocket_thread_t *thread)
{
   if (thread == NULL) {
      return false;
   }
   return GetCurrentThreadId() == thread->thread_id;
}

static unsigned __stdcall thread_runner(void *arg)
{
   msocket_thread_t *thread = (msocket_thread_t *)arg;
   if (thread != NULL && thread->func != NULL) {
      thread->func(thread->arg);
   }
   return 0;
}

msocket_sem_t *msocket_sem_new(uint32_t initial_count)
{
   msocket_sem_t *sem = (msocket_sem_t *)malloc(sizeof(msocket_sem_t));
   if (sem != NULL) {
      sem->sem = CreateSemaphore(NULL, (LONG)initial_count, 1000, NULL);
   }
   return sem;
}

void msocket_sem_delete(msocket_sem_t *sem)
{
   if (sem != NULL) {
      if (sem->sem != NULL) {
         CloseHandle(sem->sem);
      }
      free(sem);
   }
}

void msocket_sem_post(msocket_sem_t *sem)
{
   if (sem != NULL && sem->sem != NULL) {
      ReleaseSemaphore(sem->sem, 1, NULL);
   }
}

int8_t msocket_sem_test(msocket_sem_t *sem)
{
   if (sem == NULL || sem->sem == NULL) {
      return -1;
   }
   DWORD rc = WaitForSingleObject(sem->sem, 0u);
   if (rc == WAIT_OBJECT_0) {
      return 1;
   }
   if (rc == WAIT_TIMEOUT) {
      return 0;
   }
   return -1;
}

//////////////////////////////////////////////////////////////////////////////
// UNIX DOMAIN SOCKETS (WINDOWS STUBS)
//////////////////////////////////////////////////////////////////////////////

msocket_error_t msocket_os_unix_listen(msocket_t *self, const char *socket_path)
{
   (void)self;
   (void)socket_path;
   return MSOCKET_NOT_IMPLEMENTED_ERROR;
}

msocket_error_t msocket_os_unix_connect(msocket_t *self, const char *socket_path)
{
   (void)self;
   (void)socket_path;
   return MSOCKET_NOT_IMPLEMENTED_ERROR;
}
