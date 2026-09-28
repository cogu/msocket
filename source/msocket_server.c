/*****************************************************************************
* \file      msocket_server.c
* \author    Conny Gustafsson
* \date      2014-12-18
* \brief     msocket server connection and cleanup manager
*
* Copyright (c) 2014-2026 Conny Gustafsson
* SPDX-License-Identifier: MIT
* See LICENSE in project root for full license terms.
******************************************************************************/

//////////////////////////////////////////////////////////////////////////////
// INCLUDES
//////////////////////////////////////////////////////////////////////////////
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "msocket_server.h"
#include "msocket_internal.h"
#include "msocket_tls.h"
#include "msocket_tls_internal.h"



//////////////////////////////////////////////////////////////////////////////
// PRIVATE FUNCTION PROTOTYPES
//////////////////////////////////////////////////////////////////////////////
static void accept_task(void *arg);
static void cleanup_task(void *arg);
static void msocket_server_start_threads(msocket_server_t *self);

//////////////////////////////////////////////////////////////////////////////
// PUBLIC FUNCTIONS
//////////////////////////////////////////////////////////////////////////////

void msocket_server_create(msocket_server_t *self, uint8_t address_family, void (*destructor)(void *))
{
   if (self != NULL) {
      self->tcp_port = 0u;
      self->udp_port = 0u;
      self->udp_addr = NULL;
      self->socket_path = NULL;
      self->vsock_cid = 0u;
      self->vsock_port = 0u;
      self->accept_socket = NULL;
      self->cleanup_stop = 0u;
      memset(&self->handler_table, 0, sizeof(self->handler_table));
      self->handler_arg = NULL;
      self->address_family = address_family;
      self->destructor = (destructor != NULL) ? destructor : msocket_vdelete;
      self->tls_server = NULL;

      adt_ary_create(&self->cleanup_items, NULL);

      self->os = (msocket_server_os_t *)malloc(sizeof(msocket_server_os_t));
      if (self->os != NULL) {
         self->os->accept_thread = NULL;
         self->os->cleanup_thread = NULL;
         MUTEX_INIT(self->os->mutex);
         COND_INIT(self->os->cond);
      }
   }
}

void msocket_server_destroy(msocket_server_t *self)
{
   if (self != NULL && self->os != NULL) {
      MUTEX_LOCK(self->os->mutex);
      self->cleanup_stop = 1u;
      msocket_t *accept_socket = self->accept_socket;
      if (accept_socket != NULL) {
         msocket_close(accept_socket);
      }
      COND_BROADCAST(self->os->cond);
      MUTEX_UNLOCK(self->os->mutex);

      if (self->os->accept_thread != NULL) {
         msocket_thread_join(self->os->accept_thread);
         msocket_thread_delete(self->os->accept_thread);
         self->os->accept_thread = NULL;
      }

      if (self->os->cleanup_thread != NULL) {
         msocket_thread_join(self->os->cleanup_thread);
         msocket_thread_delete(self->os->cleanup_thread);
         self->os->cleanup_thread = NULL;
      }

      adt_ary_destroy(&self->cleanup_items);
      COND_DESTROY(self->os->cond);
      MUTEX_DESTROY(self->os->mutex);

      if (self->udp_addr != NULL) {
         free(self->udp_addr);
         self->udp_addr = NULL;
      }
      if (self->socket_path != NULL) {
#ifndef _WIN32
         if (*self->socket_path != '\0') {
            unlink(self->socket_path);
         }
#endif
         free(self->socket_path);
         self->socket_path = NULL;
      }

#if defined(MSOCKET_ENABLE_TLS)
      if (self->tls_server != NULL) {
         msocket_tls_server_delete(self->tls_server);
         self->tls_server = NULL;
      }
#endif
      free(self->os);
      self->os = NULL;
   }
}

msocket_server_t *msocket_server_new(uint8_t address_family, void (*destructor)(void *))
{
   msocket_server_t *self = (msocket_server_t *)malloc(sizeof(msocket_server_t));
   if (self != NULL) {
      msocket_server_create(self, address_family, destructor);
   }
   return self;
}

void msocket_server_delete(msocket_server_t *self)
{
   if (self != NULL) {
      msocket_server_destroy(self);
      free(self);
   }
}

void msocket_server_set_handler(msocket_server_t *self, const msocket_handler_t *handler, void *handler_arg)
{
   if (self != NULL && handler != NULL) {
      memcpy(&self->handler_table, handler, sizeof(msocket_handler_t));
      self->handler_arg = handler_arg;
   }
}

void msocket_server_start(msocket_server_t *self, const char *udp_addr, uint16_t udp_port, uint16_t tcp_port)
{
   if (self != NULL) {
      self->tcp_port = tcp_port;
      self->udp_port = udp_port;
      if (udp_addr != NULL) {
         size_t len = strlen(udp_addr) + 1u;
         self->udp_addr = (char *)malloc(len);
         if (self->udp_addr != NULL) {
            memcpy(self->udp_addr, udp_addr, len);
         }
      }
      msocket_server_start_threads(self);
   }
}

void msocket_server_unix_start(msocket_server_t *self, const char *socket_path)
{
   if (self != NULL && socket_path != NULL) {
      self->tcp_port = 0u;
      self->udp_port = 0u;
      size_t len = (*socket_path == '\0') ? (strlen(socket_path + 1) + 2u) : (strlen(socket_path) + 1u);
      self->socket_path = (char *)malloc(len);
      if (self->socket_path != NULL) {
         memcpy(self->socket_path, socket_path, len);
      }
      msocket_server_start_threads(self);
   }
}

void msocket_server_vsock_start(msocket_server_t *self, uint32_t cid, uint32_t port)
{
   if (self != NULL && port != 0u) {
      self->tcp_port = 0u;
      self->udp_port = 0u;
      self->vsock_cid = cid;
      self->vsock_port = port;
      msocket_server_start_threads(self);
   }
}

msocket_error_t msocket_server_start_tls(msocket_server_t *self, uint16_t tcp_port, const struct msocket_tls_config_tag *tls_config)
{
#if defined(MSOCKET_ENABLE_TLS)
   if (self == NULL || tls_config == NULL) {
      return MSOCKET_INVALID_ARGUMENT_ERROR;
   }
   self->tls_server = msocket_tls_server_new(tls_config);
   if (self->tls_server == NULL) {
      return MSOCKET_TLS_ERROR;
   }
   msocket_server_start(self, NULL, 0, tcp_port);
   return MSOCKET_NO_ERROR;
#else
   (void)self;
   (void)tcp_port;
   (void)tls_config;
   return MSOCKET_NOT_IMPLEMENTED_ERROR;
#endif
}

void msocket_server_disable_cleanup(msocket_server_t *self)
{
   if (self != NULL) {
      self->destructor = NULL;
   }
}

void msocket_server_reap_connection(msocket_server_t *self, void *arg)
{
   if (self != NULL && self->os != NULL && arg != NULL) {
      MUTEX_LOCK(self->os->mutex);
      if (self->cleanup_stop == 0u) {
         adt_ary_push(&self->cleanup_items, arg);
         COND_SIGNAL(self->os->cond);
      }
      MUTEX_UNLOCK(self->os->mutex);
   }
}

void msocket_server_cleanup_connection(msocket_server_t *self, void *arg)
{
   if (self != NULL && arg != NULL && self->destructor == msocket_vdelete) {
      msocket_set_server((msocket_t *)arg, NULL);
   }
   msocket_server_reap_connection(self, arg);
}

//////////////////////////////////////////////////////////////////////////////
// PRIVATE FUNCTIONS
//////////////////////////////////////////////////////////////////////////////

static msocket_error_t msocket_server_bind(msocket_server_t *self)
{
   self->accept_socket = msocket_new(self->address_family);
   if (self->accept_socket == NULL) {
      return MSOCKET_MEM_ERROR;
   }

   msocket_error_t rc;
   if (self->udp_port != 0u) {
      rc = msocket_listen(self->accept_socket, MSOCKET_MODE_DGRAM, self->udp_port, self->udp_addr);
      if (rc != MSOCKET_NO_ERROR) {
         fprintf(stderr, "[MSOCKET_SERVER] Failed to bind UDP port %u\n", self->udp_port);
         msocket_delete(self->accept_socket);
         self->accept_socket = NULL;
         return rc;
      }
   }

   if (self->tcp_port != 0u) {
      rc = msocket_listen(self->accept_socket, MSOCKET_MODE_STREAM, self->tcp_port, NULL);
      if (rc != MSOCKET_NO_ERROR) {
         fprintf(stderr, "[MSOCKET_SERVER] Failed to bind TCP port %u\n", self->tcp_port);
         msocket_delete(self->accept_socket);
         self->accept_socket = NULL;
         return rc;
      }
   }

   if (self->socket_path != NULL) {
      rc = msocket_unix_listen(self->accept_socket, self->socket_path);
      if (rc != MSOCKET_NO_ERROR) {
         fprintf(stderr, "[MSOCKET_SERVER] Failed to bind UNIX path %s\n", self->socket_path);
         msocket_delete(self->accept_socket);
         self->accept_socket = NULL;
         return rc;
      }
   }

   if (self->vsock_port != 0u) {
      rc = msocket_vsock_listen(self->accept_socket, self->vsock_cid, self->vsock_port);
      if (rc != MSOCKET_NO_ERROR) {
         fprintf(stderr, "[MSOCKET_SERVER] Failed to bind VSOCK port %u (CID %u)\n", self->vsock_port, self->vsock_cid);
         msocket_delete(self->accept_socket);
         self->accept_socket = NULL;
         return rc;
      }
   }

   return MSOCKET_NO_ERROR;
}

static void msocket_server_start_threads(msocket_server_t *self)
{
   if (msocket_server_bind(self) != MSOCKET_NO_ERROR) {
      return;
   }
   self->os->accept_thread = msocket_thread_create(accept_task, (void *)self);
   if (self->destructor != NULL) {
      self->os->cleanup_thread = msocket_thread_create(cleanup_task, (void *)self);
   }
}

static void accept_task(void *arg)
{
   msocket_server_t *self = (msocket_server_t *)arg;
   if (self == NULL || self->accept_socket == NULL) {
      return;
   }

   while (1) {
      msocket_t *child = msocket_accept(self->accept_socket, NULL);
      if (child == NULL) {
         break;
      }
#if defined(MSOCKET_ENABLE_TLS)
      if (self->tls_server != NULL) {
         struct msocket_tls_tag *tls = msocket_tls_server_accept(self->tls_server, child->os->tcp_sockfd);
         if (tls == NULL) {
            msocket_delete(child);
            continue;
         }
         msocket_set_tls(child, tls);
      }
#endif
      if (self->destructor == msocket_vdelete) {
         msocket_set_server(child, self);
      }
      if (self->handler_table.stream_accept != NULL) {
         self->handler_table.stream_accept(self->handler_arg, self, (void *)child);
      }
   }

   MUTEX_LOCK(self->os->mutex);
   if (self->accept_socket != NULL) {
      msocket_delete(self->accept_socket);
      self->accept_socket = NULL;
   }
   MUTEX_UNLOCK(self->os->mutex);
}

static void cleanup_task(void *arg)
{
   msocket_server_t *self = (msocket_server_t *)arg;
   if (self == NULL || self->destructor == NULL) {
      return;
   }

   while (1) {
      void *item = NULL;
      MUTEX_LOCK(self->os->mutex);
      while (adt_ary_length(&self->cleanup_items) == 0 && self->cleanup_stop == 0u) {
         COND_WAIT(self->os->cond, self->os->mutex);
      }
      if (adt_ary_length(&self->cleanup_items) > 0) {
         item = adt_ary_shift(&self->cleanup_items);
      } else if (self->cleanup_stop != 0u) {
         MUTEX_UNLOCK(self->os->mutex);
         break;
      }
      MUTEX_UNLOCK(self->os->mutex);

      if (item != NULL && self->destructor != NULL) {
         self->destructor(item);
      }
   }
}
