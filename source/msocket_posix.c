/*****************************************************************************
* \file:    msocket_posix.c
* \author:  Conny Gustafsson
* \date:    2026-09-10
* \brief:   POSIX platform-specific threading, sync, and UNIX domain sockets
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
#include <unistd.h>
#include <errno.h>
#include "msocket.h"
#include "msocket_internal.h"

//////////////////////////////////////////////////////////////////////////////
// PRIVATE CONSTANTS AND DATA TYPES
//////////////////////////////////////////////////////////////////////////////
struct msocket_thread_t {
   pthread_t thread;
   void (*func)(void *arg);
   void *arg;
};

struct msocket_sem_t {
   sem_t sem;
};

//////////////////////////////////////////////////////////////////////////////
// PRIVATE FUNCTION PROTOTYPES
//////////////////////////////////////////////////////////////////////////////
static void *thread_runner(void *arg);

//////////////////////////////////////////////////////////////////////////////
// PLATFORM LIFECYCLE
//////////////////////////////////////////////////////////////////////////////
void msocket_os_init(void)
{
}

void msocket_os_cleanup(void)
{
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

   if (pthread_create(&thread->thread, NULL, thread_runner, thread) != 0) {
      free(thread);
      return NULL;
   }
   return thread;
}

void msocket_thread_join(msocket_thread_t *thread)
{
   if (thread != NULL) {
      pthread_join(thread->thread, NULL);
   }
}

void msocket_thread_delete(msocket_thread_t *thread)
{
   if (thread != NULL) {
      free(thread);
   }
}

bool msocket_thread_is_current(msocket_thread_t *thread)
{
   if (thread == NULL) {
      return false;
   }
   return pthread_equal(pthread_self(), thread->thread) != 0;
}

static void *thread_runner(void *arg)
{
   msocket_thread_t *thread = (msocket_thread_t *)arg;
   if (thread != NULL && thread->func != NULL) {
      thread->func(thread->arg);
   }
   return NULL;
}

msocket_sem_t *msocket_sem_new(uint32_t initial_count)
{
   msocket_sem_t *sem = (msocket_sem_t *)malloc(sizeof(msocket_sem_t));
   if (sem != NULL) {
      sem_init(&sem->sem, 0, (unsigned int)initial_count);
   }
   return sem;
}

void msocket_sem_delete(msocket_sem_t *sem)
{
   if (sem != NULL) {
      sem_destroy(&sem->sem);
      free(sem);
   }
}

void msocket_sem_post(msocket_sem_t *sem)
{
   if (sem != NULL) {
      sem_post(&sem->sem);
   }
}

int8_t msocket_sem_test(msocket_sem_t *sem)
{
   if (sem == NULL) {
      return -1;
   }
   int rc = sem_trywait(&sem->sem);
   if (rc < 0) {
      if (errno == EAGAIN) {
         return 0;
      }
      return -1;
   }
   return 1;
}

//////////////////////////////////////////////////////////////////////////////
// UNIX DOMAIN SOCKETS
//////////////////////////////////////////////////////////////////////////////

msocket_error_t msocket_os_unix_listen(msocket_t *self, const char *socket_path)
{
   struct sockaddr_un saddr;
   int one = 1;
   int sockunix;
   int rc;

   memset(&saddr, 0, sizeof(saddr));
   saddr.sun_family = AF_UNIX;
   if (*socket_path == '\0') {
      *saddr.sun_path = '\0';
      strncpy(saddr.sun_path + 1, socket_path + 1, sizeof(saddr.sun_path) - 2);
   } else {
      strncpy(saddr.sun_path, socket_path, sizeof(saddr.sun_path) - 1);
      unlink(socket_path);
   }

   sockunix = socket(PF_LOCAL, SOCK_STREAM, 0);
   if (sockunix < 0) {
      return MSOCKET_SOCKET_ERROR;
   }

   setsockopt(sockunix, SOL_SOCKET, SO_REUSEADDR, (const void *)&one, sizeof(one));
   rc = bind(sockunix, (struct sockaddr *)&saddr, sizeof(saddr));
   if (rc < 0) {
      close(sockunix);
      return MSOCKET_SOCKET_ERROR;
   }

   rc = listen(sockunix, 5);
   if (rc < 0) {
      close(sockunix);
      return MSOCKET_SOCKET_ERROR;
   }

   self->os->tcp_sockfd = sockunix;
   self->state = MSOCKET_STATE_LISTENING;
   self->socket_mode |= MSOCKET_MODE_STREAM;
   return MSOCKET_NO_ERROR;
}

msocket_error_t msocket_os_unix_connect(msocket_t *self, const char *socket_path)
{
   struct sockaddr_un saddr;
   int sockfd = socket(PF_LOCAL, SOCK_STREAM, 0);
   if (sockfd < 0) {
      return MSOCKET_SOCKET_ERROR;
   }

   memset(&saddr, 0, sizeof(saddr));
   saddr.sun_family = AF_UNIX;
   if (*socket_path == '\0') {
      *saddr.sun_path = '\0';
      strncpy(saddr.sun_path + 1, socket_path + 1, sizeof(saddr.sun_path) - 2);
   } else {
      strncpy(saddr.sun_path, socket_path, sizeof(saddr.sun_path) - 1);
   }

   int rc = connect(sockfd, (struct sockaddr *)&saddr, sizeof(saddr));
   if (rc < 0) {
      close(sockfd);
      return MSOCKET_SOCKET_ERROR;
   }

   strncpy(self->stream_info.addr, socket_path, MSOCKET_ADDRSTRLEN - 1);
   self->stream_info.port = 0;
   self->os->tcp_sockfd = sockfd;
   self->socket_mode |= MSOCKET_MODE_STREAM;
   self->state = MSOCKET_STATE_ESTABLISHED;
   self->os->new_connection = true;

   msocket_error_t io_rc = msocket_start_io(self);
   if (io_rc != MSOCKET_NO_ERROR) {
      close(sockfd);
      self->os->tcp_sockfd = -1;
      self->state = MSOCKET_STATE_CLOSED;
      return io_rc;
   }
   return MSOCKET_NO_ERROR;
}
