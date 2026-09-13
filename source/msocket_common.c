/*****************************************************************************
* \file:    msocket_common.c
* \author:  Conny Gustafsson
* \date:    2014-10-01
* \brief:   Platform-independent socket handling and state management
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
#include <ctype.h>
#include <assert.h>
#include <errno.h>
#include "msocket.h"
#include "msocket_internal.h"
#include "msocket_server.h"

//////////////////////////////////////////////////////////////////////////////
// PRIVATE CONSTANTS AND DATA TYPES
//////////////////////////////////////////////////////////////////////////////
#define TIMEOUT_CALL_INTERVAL_MS 1000u
#define TIMEOUT_STEP_MS          50u
#define MSG_BUF_SIZE             2048
#define TIMEOUT_US               50000

//////////////////////////////////////////////////////////////////////////////
// PRIVATE FUNCTION PROTOTYPES
//////////////////////////////////////////////////////////////////////////////
static void msocket_timeout_reset(msocket_t *self);
static bool msocket_timeout_increase(msocket_t *self);
static msocket_error_t msocket_start_io_thread(msocket_t *self);
static void io_task(void *arg);
static bool io_read_dgram(msocket_t *self);
static bool io_read_stream(msocket_t *self);

//////////////////////////////////////////////////////////////////////////////
// PLATFORM SOCKET MANAGEMENT
//////////////////////////////////////////////////////////////////////////////

msocket_os_t *msocket_os_new(void)
{
   msocket_os_init();
   msocket_os_t *os = (msocket_os_t *)malloc(sizeof(msocket_os_t));
   if (os != NULL) {
      os->tcp_sockfd = OS_SOCKET_INVALID;
      os->udp_sockfd = OS_SOCKET_INVALID;
      os->io_thread = NULL;
      os->thread_running = false;
      os->new_connection = false;
      MUTEX_INIT(os->mutex);
   }
   return os;
}

void msocket_os_delete(msocket_os_t *os)
{
   if (os != NULL) {
      MUTEX_DESTROY(os->mutex);
      free(os);
      msocket_os_cleanup();
   }
}

void msocket_os_mutex_lock(msocket_os_t *os)
{
   if (os != NULL) {
      MUTEX_LOCK(os->mutex);
   }
}

void msocket_os_mutex_unlock(msocket_os_t *os)
{
   if (os != NULL) {
      MUTEX_UNLOCK(os->mutex);
   }
}

//////////////////////////////////////////////////////////////////////////////
// PUBLIC FUNCTIONS
//////////////////////////////////////////////////////////////////////////////

msocket_error_t msocket_create(msocket_t *self, uint8_t address_family)
{
   if (self == NULL) {
      return MSOCKET_INVALID_ARGUMENT_ERROR;
   }

   switch (address_family) {
   case MSOCKET_ADDR_INET:
#if defined(AF_INET) && (AF_INET != MSOCKET_ADDR_INET)
   case AF_INET:
#endif
      self->address_family = MSOCKET_ADDR_INET;
      break;
   case MSOCKET_ADDR_INET6:
#if defined(AF_INET6) && (AF_INET6 != MSOCKET_ADDR_INET6)
   case AF_INET6:
#endif
      self->address_family = MSOCKET_ADDR_INET6;
      break;
   case MSOCKET_ADDR_UNIX:
#if defined(AF_LOCAL) && (AF_LOCAL != MSOCKET_ADDR_UNIX)
   case AF_LOCAL:
#endif
      self->address_family = MSOCKET_ADDR_UNIX;
      break;
   default:
      return MSOCKET_INVALID_ARGUMENT_ERROR;
   }

   memset(&self->stream_info, 0, sizeof(msocket_addr_info_t));
   memset(&self->udp_info, 0, sizeof(msocket_addr_info_t));
   self->handler_table = NULL;
   self->handler_arg = NULL;
   self->server = NULL;
   self->socket_mode = MSOCKET_MODE_NONE;
   self->state = MSOCKET_STATE_NONE;

   msocket_timeout_reset(self);
   adt_streambuffer_create(&self->stream_rx_buf, 0u, 0u);

   self->os = msocket_os_new();
   if (self->os == NULL) {
      adt_streambuffer_destroy(&self->stream_rx_buf);
      return MSOCKET_MEM_ERROR;
   }

   return MSOCKET_NO_ERROR;
}

void msocket_destroy(msocket_t *self)
{
   if (self != NULL) {
      msocket_close(self);
      adt_streambuffer_destroy(&self->stream_rx_buf);
      if (self->handler_table != NULL) {
         free(self->handler_table);
         self->handler_table = NULL;
      }
      if (self->os != NULL) {
         msocket_os_delete(self->os);
         self->os = NULL;
      }
   }
}

msocket_t *msocket_new(uint8_t address_family)
{
   msocket_t *self = (msocket_t *)malloc(sizeof(msocket_t));
   if (self != NULL) {
      msocket_error_t rc = msocket_create(self, address_family);
      if (rc != MSOCKET_NO_ERROR) {
         free(self);
         return NULL;
      }
   }
   return self;
}

void msocket_delete(msocket_t *self)
{
   if (self != NULL) {
      msocket_destroy(self);
      free(self);
   }
}

void msocket_vdelete(void *arg)
{
   msocket_delete((msocket_t *)arg);
}

void msocket_set_handler(msocket_t *self, const msocket_handler_t *handler_table, void *handler_arg)
{
   if (self != NULL) {
      if (self->handler_table != NULL) {
         free(self->handler_table);
         self->handler_table = NULL;
      }
      if (handler_table != NULL) {
         self->handler_table = (msocket_handler_t *)malloc(sizeof(msocket_handler_t));
         if (self->handler_table != NULL) {
            memcpy(self->handler_table, handler_table, sizeof(msocket_handler_t));
         }
      }
      self->handler_arg = handler_arg;
   }
}

void msocket_set_server(msocket_t *self, msocket_server_t *server)
{
   if (self != NULL && self->os != NULL) {
      msocket_os_mutex_lock(self->os);
      self->server = server;
      msocket_os_mutex_unlock(self->os);
   }
}

msocket_state_t msocket_state(msocket_t *self)
{
   if (self != NULL && self->os != NULL) {
      msocket_os_mutex_lock(self->os);
      msocket_state_t state = self->state;
      msocket_os_mutex_unlock(self->os);
      return state;
   }
   return MSOCKET_STATE_NONE;
}

msocket_error_t msocket_listen(msocket_t *self, uint8_t mode, uint16_t port, const char *addr)
{
   if (self == NULL || (mode != MSOCKET_MODE_DGRAM && mode != MSOCKET_MODE_STREAM)) {
      return MSOCKET_INVALID_ARGUMENT_ERROR;
   }
   if (self->os == NULL) {
      return MSOCKET_INVALID_ARGUMENT_ERROR;
   }

   int rc;
   int one = 1;
   struct sockaddr_in saddr;
   struct sockaddr_in6 saddr6;

   if (self->address_family == MSOCKET_ADDR_INET6) {
      memset(&saddr6, 0, sizeof(saddr6));
      saddr6.sin6_family = AF_INET6;
      saddr6.sin6_addr = in6addr_any;
      saddr6.sin6_port = htons(port);
   } else {
      memset(&saddr, 0, sizeof(saddr));
      saddr.sin_family = AF_INET;
      if (addr == NULL) {
         saddr.sin_addr.s_addr = INADDR_ANY;
      } else {
         inet_pton(AF_INET, addr, &saddr.sin_addr);
      }
      saddr.sin_port = htons(port);
   }

   if (mode == MSOCKET_MODE_DGRAM) {
      os_socket_t sockudp;
      self->udp_info.port = port;
      if (self->address_family == MSOCKET_ADDR_INET6) {
         sockudp = socket(PF_INET6, SOCK_DGRAM, IPPROTO_UDP);
      } else {
         sockudp = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);
      }
      if (OS_SOCKET_IS_INVALID(sockudp)) {
         return MSOCKET_SOCKET_ERROR;
      }

      setsockopt(sockudp, SOL_SOCKET, SO_REUSEADDR, OS_SOCKOPT_CAST(&one), sizeof(one));
      if (self->address_family == MSOCKET_ADDR_INET6) {
         struct ipv6_mreq mreq;
         inet_pton(AF_INET6, addr, &(mreq.ipv6mr_multiaddr));
         mreq.ipv6mr_interface = 0;
         setsockopt(sockudp, IPPROTO_IPV6, IPV6_JOIN_GROUP, OS_SOCKOPT_CAST(&mreq), sizeof(mreq));
         rc = bind(sockudp, (struct sockaddr *)&saddr6, sizeof(saddr6));
      } else {
         setsockopt(sockudp, SOL_SOCKET, SO_BROADCAST, OS_SOCKOPT_CAST(&one), sizeof(one));
         rc = bind(sockudp, (struct sockaddr *)&saddr, sizeof(saddr));
      }

      if (rc < 0) {
         OS_SOCKET_CLOSE(sockudp);
         return MSOCKET_SOCKET_ERROR;
      }

      self->os->udp_sockfd = sockudp;
      self->socket_mode |= mode;
      return msocket_start_io_thread(self);
   } else {
      os_socket_t socktcp;
      self->stream_info.port = port;
      if (self->address_family == MSOCKET_ADDR_INET6) {
         socktcp = socket(PF_INET6, SOCK_STREAM, IPPROTO_TCP);
      } else {
         socktcp = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
      }
      if (OS_SOCKET_IS_INVALID(socktcp)) {
         return MSOCKET_SOCKET_ERROR;
      }

      setsockopt(socktcp, SOL_SOCKET, SO_REUSEADDR, OS_SOCKOPT_CAST(&one), sizeof(one));
      setsockopt(socktcp, IPPROTO_TCP, TCP_NODELAY, OS_SOCKOPT_CAST(&one), sizeof(one));

      if (self->address_family == MSOCKET_ADDR_INET6) {
         rc = bind(socktcp, (struct sockaddr *)&saddr6, sizeof(saddr6));
      } else {
         rc = bind(socktcp, (struct sockaddr *)&saddr, sizeof(saddr));
      }

      if (rc < 0) {
         OS_SOCKET_CLOSE(socktcp);
         return MSOCKET_SOCKET_ERROR;
      }

      rc = listen(socktcp, 5);
      if (rc < 0) {
         OS_SOCKET_CLOSE(socktcp);
         return MSOCKET_SOCKET_ERROR;
      }

      self->os->tcp_sockfd = socktcp;
      self->state = MSOCKET_STATE_LISTENING;
      self->socket_mode |= mode;
      return MSOCKET_NO_ERROR;
   }
}

msocket_error_t msocket_unix_listen(msocket_t *self, const char *socket_path)
{
   if (self == NULL || socket_path == NULL) {
      return MSOCKET_INVALID_ARGUMENT_ERROR;
   }
   return msocket_os_unix_listen(self, socket_path);
}

msocket_t *msocket_accept(msocket_t *self, msocket_t *child)
{
   if (self == NULL || self->os == NULL || self->state != MSOCKET_STATE_LISTENING) {
      return NULL;
   }

   bool placement_new = false;
   if (child == NULL) {
      child = msocket_new(self->address_family);
   } else {
      placement_new = true;
      msocket_create(child, self->address_family);
   }

   if (child == NULL) {
      return NULL;
   }

   msocket_os_mutex_lock(self->os);
   self->state = MSOCKET_STATE_ACCEPTING;
   msocket_os_mutex_unlock(self->os);

   os_socket_t sockfd;
   int one = 1;
   msocket_error_t result = MSOCKET_NO_ERROR;

   if (self->address_family == MSOCKET_ADDR_UNIX) {
      sockfd = accept(self->os->tcp_sockfd, NULL, NULL);
      if (OS_SOCKET_IS_INVALID(sockfd)) {
         result = MSOCKET_SOCKET_ERROR;
      } else {
         child->os->tcp_sockfd = sockfd;
         strcpy(child->stream_info.addr, "local");
         child->stream_info.port = 0;
      }
   } else if (self->address_family == MSOCKET_ADDR_INET6) {
      struct sockaddr_in6 cli_addr6;
      OS_SOCK_LEN_T cli_len = (OS_SOCK_LEN_T)sizeof(cli_addr6);
      memset(&cli_addr6, 0, sizeof(cli_addr6));
      sockfd = accept(self->os->tcp_sockfd, (struct sockaddr *)&cli_addr6, &cli_len);
      if (OS_SOCKET_IS_INVALID(sockfd)) {
         result = MSOCKET_SOCKET_ERROR;
      } else if (inet_ntop(AF_INET6, &(cli_addr6.sin6_addr), child->stream_info.addr, MSOCKET_ADDRSTRLEN) == NULL) {
         OS_SOCKET_CLOSE(sockfd);
         result = MSOCKET_SOCKET_ERROR;
      } else {
         child->stream_info.port = ntohs(cli_addr6.sin6_port);
         setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, OS_SOCKOPT_CAST(&one), sizeof(one));
         child->os->tcp_sockfd = sockfd;
      }
   } else {
      struct sockaddr_in cli_addr;
      OS_SOCK_LEN_T cli_len = (OS_SOCK_LEN_T)sizeof(cli_addr);
      memset(&cli_addr, 0, sizeof(cli_addr));
      sockfd = accept(self->os->tcp_sockfd, (struct sockaddr *)&cli_addr, &cli_len);
      if (OS_SOCKET_IS_INVALID(sockfd)) {
         result = MSOCKET_SOCKET_ERROR;
      } else if (inet_ntop(AF_INET, &(cli_addr.sin_addr), child->stream_info.addr, MSOCKET_ADDRSTRLEN) == NULL) {
         OS_SOCKET_CLOSE(sockfd);
         result = MSOCKET_SOCKET_ERROR;
      } else {
         child->stream_info.port = ntohs(cli_addr.sin_port);
         setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, OS_SOCKOPT_CAST(&one), sizeof(one));
         child->os->tcp_sockfd = sockfd;
      }
   }

   msocket_os_mutex_lock(self->os);
   self->state = MSOCKET_STATE_LISTENING;
   msocket_os_mutex_unlock(self->os);

   if (result != MSOCKET_NO_ERROR) {
      if (placement_new) {
         msocket_destroy(child);
      } else {
         msocket_delete(child);
      }
      return NULL;
   }

   msocket_os_mutex_lock(child->os);
   child->state = MSOCKET_STATE_ESTABLISHED;
   child->socket_mode = MSOCKET_MODE_STREAM;
   msocket_os_mutex_unlock(child->os);

   return child;
}

static msocket_error_t msocket_start_io_thread(msocket_t *self)
{
   if (self == NULL || self->os == NULL) {
      return MSOCKET_INVALID_ARGUMENT_ERROR;
   }
   if (!self->os->thread_running) {
      self->os->thread_running = true;
      self->os->io_thread = msocket_thread_create(io_task, self);
      if (self->os->io_thread == NULL) {
         self->os->thread_running = false;
         return MSOCKET_MEM_ERROR;
      }
   }
   return MSOCKET_NO_ERROR;
}

msocket_error_t msocket_start_io(msocket_t *self)
{
   if (self == NULL || self->os == NULL) {
      return MSOCKET_INVALID_ARGUMENT_ERROR;
   }
   if (self->handler_table == NULL) {
      return MSOCKET_INVALID_ARGUMENT_ERROR;
   }
   return msocket_start_io_thread(self);
}

msocket_error_t msocket_connect(msocket_t *self, const char *addr, uint16_t port)
{
   if (self == NULL || addr == NULL || (self->socket_mode & MSOCKET_MODE_STREAM) != 0) {
      return MSOCKET_INVALID_ARGUMENT_ERROR;
   }
   if (self->handler_table == NULL) {
      return MSOCKET_INVALID_ARGUMENT_ERROR;
   }
   if (self->os == NULL) {
      return MSOCKET_INVALID_ARGUMENT_ERROR;
   }

   os_socket_t sockfd;
   int one = 1;
   int rc;

   if (self->address_family == MSOCKET_ADDR_INET6) {
      struct sockaddr_in6 saddr6;
      memset(&saddr6, 0, sizeof(saddr6));
      rc = inet_pton(AF_INET6, addr, &(saddr6.sin6_addr));
      if (rc <= 0) {
         return MSOCKET_INVALID_ARGUMENT_ERROR;
      }
      saddr6.sin6_family = AF_INET6;
      saddr6.sin6_port = htons(port);
      sockfd = socket(PF_INET6, SOCK_STREAM, IPPROTO_TCP);
      if (OS_SOCKET_IS_INVALID(sockfd)) {
         return MSOCKET_SOCKET_ERROR;
      }
      rc = connect(sockfd, (struct sockaddr *)&saddr6, sizeof(saddr6));
   } else {
      struct sockaddr_in saddr;
      memset(&saddr, 0, sizeof(saddr));
      rc = inet_pton(AF_INET, addr, &(saddr.sin_addr));
      if (rc <= 0) {
         return MSOCKET_INVALID_ARGUMENT_ERROR;
      }
      saddr.sin_family = AF_INET;
      saddr.sin_port = htons(port);
      sockfd = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
      if (OS_SOCKET_IS_INVALID(sockfd)) {
         return MSOCKET_SOCKET_ERROR;
      }
      rc = connect(sockfd, (struct sockaddr *)&saddr, sizeof(saddr));
   }

   if (rc < 0) {
      OS_SOCKET_CLOSE(sockfd);
      return MSOCKET_SOCKET_ERROR;
   }

   setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, OS_SOCKOPT_CAST(&one), sizeof(one));
   strncpy(self->stream_info.addr, addr, MSOCKET_ADDRSTRLEN - 1);
   self->stream_info.port = port;
   self->os->tcp_sockfd = sockfd;
   self->socket_mode |= MSOCKET_MODE_STREAM;
   self->state = MSOCKET_STATE_ESTABLISHED;
   self->os->new_connection = true;

   msocket_error_t io_rc = msocket_start_io_thread(self);
   if (io_rc != MSOCKET_NO_ERROR) {
      OS_SOCKET_CLOSE(sockfd);
      self->os->tcp_sockfd = OS_SOCKET_INVALID;
      self->state = MSOCKET_STATE_CLOSED;
      return io_rc;
   }
   return MSOCKET_NO_ERROR;
}

msocket_error_t msocket_unix_connect(msocket_t *self, const char *socket_path)
{
   if (self == NULL || socket_path == NULL || (self->socket_mode & MSOCKET_MODE_STREAM) != 0) {
      return MSOCKET_INVALID_ARGUMENT_ERROR;
   }
   if (self->handler_table == NULL) {
      return MSOCKET_INVALID_ARGUMENT_ERROR;
   }
   return msocket_os_unix_connect(self, socket_path);
}

msocket_error_t msocket_send(msocket_t *self, const void *msg_data, uint32_t msg_len)
{
   if (self == NULL || msg_data == NULL || (self->socket_mode & MSOCKET_MODE_STREAM) == 0) {
      return MSOCKET_INVALID_ARGUMENT_ERROR;
   }
   if (self->os == NULL || OS_SOCKET_IS_INVALID(self->os->tcp_sockfd)) {
      return MSOCKET_NOT_CONNECTED_ERROR;
   }

   const char *p = (const char *)msg_data;
   uint32_t remain = msg_len;
   while (remain > 0u) {
      int n = (int)send(self->os->tcp_sockfd, p, (int)remain, 0);
      if (n <= 0) {
         return MSOCKET_SOCKET_ERROR;
      }
      remain -= (uint32_t)n;
      p += n;
   }

   msocket_os_mutex_lock(self->os);
   msocket_timeout_reset(self);
   msocket_os_mutex_unlock(self->os);

   return MSOCKET_NO_ERROR;
}

msocket_error_t msocket_send_to(msocket_t *self, const char *addr, uint16_t port, const void *msg_data, uint32_t msg_len)
{
   if (self == NULL || addr == NULL || (self->socket_mode & MSOCKET_MODE_DGRAM) == 0) {
      return MSOCKET_INVALID_ARGUMENT_ERROR;
   }
   if (self->os == NULL || OS_SOCKET_IS_INVALID(self->os->udp_sockfd)) {
      return MSOCKET_NOT_CONNECTED_ERROR;
   }

   int rc;
   if (self->address_family == MSOCKET_ADDR_INET6) {
      struct sockaddr_in6 saddr6;
      memset(&saddr6, 0, sizeof(saddr6));
      saddr6.sin6_family = AF_INET6;
      saddr6.sin6_port = htons(port);
      inet_pton(AF_INET6, addr, &(saddr6.sin6_addr));
      rc = (int)sendto(self->os->udp_sockfd, (const char *)msg_data, (int)msg_len, 0, (struct sockaddr *)&saddr6, sizeof(saddr6));
   } else {
      struct sockaddr_in saddr;
      memset(&saddr, 0, sizeof(saddr));
      saddr.sin_family = AF_INET;
      saddr.sin_port = htons(port);
      inet_pton(AF_INET, addr, &(saddr.sin_addr));
      rc = (int)sendto(self->os->udp_sockfd, (const char *)msg_data, (int)msg_len, 0, (struct sockaddr *)&saddr, sizeof(saddr));
   }

   if (rc >= 0) {
      msocket_os_mutex_lock(self->os);
      msocket_timeout_reset(self);
      msocket_os_mutex_unlock(self->os);
      return MSOCKET_NO_ERROR;
   }
   return MSOCKET_SOCKET_ERROR;
}

void msocket_close(msocket_t *self)
{
   if (self == NULL || self->os == NULL) {
      return;
   }

   /* Prevent joining I/O thread from within itself */
   if (self->os->thread_running && msocket_thread_is_current(self->os->io_thread)) {
      return;
   }

   msocket_os_mutex_lock(self->os);
   self->state = MSOCKET_STATE_CLOSING;
   if (OS_SOCKET_IS_VALID(self->os->tcp_sockfd)) {
      OS_SOCKET_SHUTDOWN(self->os->tcp_sockfd);
   }
   msocket_os_mutex_unlock(self->os);

   if (self->os->thread_running) {
      msocket_thread_join(self->os->io_thread);
      msocket_thread_delete(self->os->io_thread);
      self->os->io_thread = NULL;
      self->os->thread_running = false;
   }

   msocket_os_mutex_lock(self->os);
   if (OS_SOCKET_IS_VALID(self->os->tcp_sockfd)) {
      OS_SOCKET_CLOSE(self->os->tcp_sockfd);
      self->os->tcp_sockfd = OS_SOCKET_INVALID;
   }
   if (OS_SOCKET_IS_VALID(self->os->udp_sockfd)) {
      OS_SOCKET_CLOSE(self->os->udp_sockfd);
      self->os->udp_sockfd = OS_SOCKET_INVALID;
   }
   msocket_common_reset(self);
   msocket_os_mutex_unlock(self->os);
}

//////////////////////////////////////////////////////////////////////////////
// INTERNAL PLATFORM CALLBACKS
//////////////////////////////////////////////////////////////////////////////

void msocket_common_on_connected(msocket_t *self)
{
   if (self != NULL && self->handler_table != NULL && self->handler_table->stream_connected != NULL) {
      self->handler_table->stream_connected(self->handler_arg, (void *)self, self->stream_info.addr, self->stream_info.port);
   }
}

void msocket_common_on_disconnected(msocket_t *self)
{
   if (self != NULL) {
      bool trigger = false;
      msocket_os_mutex_lock(self->os);
      if (self->state != MSOCKET_STATE_CLOSING) {
         self->state = MSOCKET_STATE_CLOSING;
         trigger = true;
      }
      msocket_os_mutex_unlock(self->os);

      if (trigger && self->handler_table != NULL && self->handler_table->stream_disconnected != NULL) {
         self->handler_table->stream_disconnected(self->handler_arg, (void *)self);
      }
   }
}

msocket_error_t msocket_common_process_stream_data(msocket_t *self)
{
   if (self == NULL || self->handler_table == NULL || self->handler_table->stream_data == NULL) {
      return MSOCKET_NO_ERROR;
   }

   while (1) {
      uint32_t cur_len = 0u;
      const uint8_t *data = adt_streambuffer_read_begin(&self->stream_rx_buf, &cur_len);
      if (data == NULL || cur_len == 0u) {
         break;
      }

      uint32_t consumed_bytes = 0u;
      uint32_t msg_size_hint = 0u;
      msocket_error_t rc = self->handler_table->stream_data(self->handler_arg, (void *)self, data, cur_len, &consumed_bytes, &msg_size_hint);
      if (rc != MSOCKET_NO_ERROR) {
         msocket_os_mutex_lock(self->os);
         self->state = MSOCKET_STATE_CLOSING;
         msocket_os_mutex_unlock(self->os);
         return rc;
      }
      if (consumed_bytes == 0u) {
         if (msg_size_hint > 0u) {
            adt_error_t err = adt_streambuffer_reserve(&self->stream_rx_buf, msg_size_hint);
            if (err != ADT_NO_ERROR) {
               msocket_os_mutex_lock(self->os);
               self->state = MSOCKET_STATE_CLOSING;
               msocket_os_mutex_unlock(self->os);
               return MSOCKET_MEM_ERROR;
            }
         }
         break;
      }
      assert(consumed_bytes <= cur_len);
      adt_streambuffer_read_commit(&self->stream_rx_buf, consumed_bytes);
   }

   return MSOCKET_NO_ERROR;
}

msocket_error_t msocket_common_on_data(msocket_t *self, const uint8_t *data_buf, uint32_t data_len)
{
   if (self == NULL) {
      return MSOCKET_INVALID_ARGUMENT_ERROR;
   }

   if (self->handler_table == NULL || self->handler_table->stream_data == NULL) {
      return MSOCKET_NO_ERROR;
   }

   if (data_buf != NULL && data_len > 0u) {
      uint32_t avail_bytes = 0u;
      uint8_t *dest = adt_streambuffer_write_begin(&self->stream_rx_buf, data_len, &avail_bytes);
      if (dest == NULL) {
         return MSOCKET_MEM_ERROR;
      }
      memcpy(dest, data_buf, data_len);
      if (adt_streambuffer_write_commit(&self->stream_rx_buf, data_len) != ADT_NO_ERROR) {
         return MSOCKET_MEM_ERROR;
      }
   }

   return msocket_common_process_stream_data(self);
}

void msocket_common_on_udp_msg(msocket_t *self, const char *addr, uint16_t port, const uint8_t *data_buf, uint32_t data_len)
{
   if (self != NULL && self->handler_table != NULL && self->handler_table->datagram_msg != NULL) {
      self->handler_table->datagram_msg(self->handler_arg, (void *)self, addr, port, data_buf, data_len);
   }
}

void msocket_common_on_timeout(msocket_t *self)
{
   if (self != NULL) {
      bool trigger = false;
      msocket_os_mutex_lock(self->os);
      if (self->state == MSOCKET_STATE_ESTABLISHED) {
         trigger = msocket_timeout_increase(self);
      }
      msocket_os_mutex_unlock(self->os);

      if (trigger && self->handler_table != NULL && self->handler_table->stream_inactivity != NULL) {
         self->handler_table->stream_inactivity(self->handler_arg, (void *)self, self->inactivity_ms);
      }
   }
}

void msocket_common_reset(msocket_t *self)
{
   if (self != NULL) {
      self->state = MSOCKET_STATE_NONE;
      self->socket_mode = MSOCKET_MODE_NONE;
      msocket_timeout_reset(self);
      adt_streambuffer_clear(&self->stream_rx_buf);
   }
}

//////////////////////////////////////////////////////////////////////////////
// PRIVATE FUNCTIONS
//////////////////////////////////////////////////////////////////////////////

static void msocket_timeout_reset(msocket_t *self)
{
   if (self != NULL) {
      self->inactivity_ms = 0u;
      self->inactivity_call_ms = TIMEOUT_CALL_INTERVAL_MS;
   }
}

static bool msocket_timeout_increase(msocket_t *self)
{
   if (self != NULL) {
      self->inactivity_ms += TIMEOUT_STEP_MS;
      if (self->inactivity_ms >= self->inactivity_call_ms) {
         self->inactivity_call_ms += TIMEOUT_CALL_INTERVAL_MS;
         return true;
      }
   }
   return false;
}

static bool io_read_dgram(msocket_t *self)
{
   char peer_addr[MSOCKET_ADDRSTRLEN];
   uint16_t peer_port = 0;
   int rc;
   uint32_t avail_bytes = 0u;
   uint8_t *buf = adt_streambuffer_write_begin(&self->stream_rx_buf, MSG_BUF_SIZE, &avail_bytes);
   if (buf == NULL) {
      return false;
   }

   if (self->address_family == MSOCKET_ADDR_INET6) {
      struct sockaddr_in6 sock_addr6;
      OS_SOCK_LEN_T len = (OS_SOCK_LEN_T)sizeof(sock_addr6);
      memset(&sock_addr6, 0, sizeof(sock_addr6));
      rc = (int)recvfrom(self->os->udp_sockfd, (char *)buf, avail_bytes, 0, (struct sockaddr *)&sock_addr6, &len);
      if (rc >= 0) {
         peer_port = ntohs(sock_addr6.sin6_port);
         inet_ntop(AF_INET6, &(sock_addr6.sin6_addr), peer_addr, MSOCKET_ADDRSTRLEN);
      }
   } else {
      struct sockaddr_in sock_addr4;
      OS_SOCK_LEN_T len = (OS_SOCK_LEN_T)sizeof(sock_addr4);
      memset(&sock_addr4, 0, sizeof(sock_addr4));
      rc = (int)recvfrom(self->os->udp_sockfd, (char *)buf, avail_bytes, 0, (struct sockaddr *)&sock_addr4, &len);
      if (rc >= 0) {
         peer_port = ntohs(sock_addr4.sin_port);
         inet_ntop(AF_INET, &(sock_addr4.sin_addr), peer_addr, MSOCKET_ADDRSTRLEN);
      }
   }

   if (rc >= 0) {
      msocket_common_on_udp_msg(self, peer_addr, peer_port, buf, (uint32_t)rc);
   }
   return true;
}

static bool io_read_stream(msocket_t *self)
{
   uint32_t avail_bytes = 0u;
   uint8_t *buf = adt_streambuffer_write_begin(&self->stream_rx_buf, MSG_BUF_SIZE, &avail_bytes);
   if (buf == NULL) {
      return false;
   }
   int rc = (int)recv(self->os->tcp_sockfd, (char *)buf, avail_bytes, 0);
   if (rc > 0) {
      if (adt_streambuffer_write_commit(&self->stream_rx_buf, (uint32_t)rc) != ADT_NO_ERROR) {
         return false;
      }
      if (msocket_common_process_stream_data(self) != MSOCKET_NO_ERROR) {
         return false;
      }
   } else {
      msocket_common_on_disconnected(self);
      return false;
   }
   return true;
}

static void io_task(void *arg)
{
   msocket_t *self = (msocket_t *)arg;
   if (self == NULL || self->os == NULL) {
      return;
   }

   bool notify_connected = false;
   msocket_os_mutex_lock(self->os);
   if (self->os->new_connection) {
      self->os->new_connection = false;
      notify_connected = true;
   }
   msocket_os_mutex_unlock(self->os);

   if (notify_connected) {
      msocket_common_on_connected(self);
   }

   while (1) {
      fd_set readfds;
      FD_ZERO(&readfds);
      int max_sd = -1;

      if ((self->socket_mode & MSOCKET_MODE_DGRAM) && OS_SOCKET_IS_VALID(self->os->udp_sockfd)) {
         FD_SET(self->os->udp_sockfd, &readfds);
         max_sd = (int)self->os->udp_sockfd;
      } else if ((self->socket_mode & MSOCKET_MODE_STREAM) && OS_SOCKET_IS_VALID(self->os->tcp_sockfd)) {
         FD_SET(self->os->tcp_sockfd, &readfds);
         max_sd = (int)self->os->tcp_sockfd;
      } else {
         break;
      }

      struct timeval timeout;
      timeout.tv_sec = 0;
      timeout.tv_usec = TIMEOUT_US;

      int activity = select(max_sd + 1, &readfds, NULL, NULL, &timeout);
      if (activity > 0) {
         if ((self->socket_mode & MSOCKET_MODE_DGRAM) && FD_ISSET(self->os->udp_sockfd, &readfds)) {
            if (!io_read_dgram(self)) {
               break;
            }
         } else if ((self->socket_mode & MSOCKET_MODE_STREAM) && FD_ISSET(self->os->tcp_sockfd, &readfds)) {
            if (!io_read_stream(self)) {
               break;
            }
         }
      } else if (activity == 0) {
         msocket_state_t state;
         msocket_os_mutex_lock(self->os);
         state = self->state;
         msocket_os_mutex_unlock(self->os);

         if (state == MSOCKET_STATE_CLOSING) {
            break;
         }
         msocket_common_on_timeout(self);
      } else {
         if (OS_SOCKET_ERRNO_IS_INTR()) {
            continue;
         }
         break;
      }
   }

   msocket_server_t *server = NULL;
   msocket_os_mutex_lock(self->os);
   server = self->server;
   self->server = NULL;
   msocket_os_mutex_unlock(self->os);

   if (server != NULL) {
      msocket_server_reap_connection(server, (void *)self);
   }
}

//////////////////////////////////////////////////////////////////////////////
// ENDPOINT PARSING
//////////////////////////////////////////////////////////////////////////////

static bool verify_ipv4_address(const char *p_begin, const char *p_end)
{
   if ((p_begin == NULL) || (p_end == NULL) || (p_begin >= p_end)) {
      return false;
   }
   const char *p_next = p_begin;
   int number_base = 10;
   int number_in_group = 0;
   int group_length = 0;
   int group_count = 1;

   while (p_next < p_end) {
      char c = *p_next++;
      if (c == '.') {
         if (group_length > 0) {
            ++group_count;
            number_in_group = 0;
            group_length = 0;
         } else {
            return false;
         }
      } else if (c >= '0' && c <= '9') {
         group_length++;
         number_in_group = number_in_group * number_base + (c - '0');
         if (number_in_group > 255) {
            return false;
         }
      } else {
         return false;
      }
   }
   return (group_count == 4 && group_length > 0);
}

static bool verify_name(const char *p_begin, const char *p_end)
{
   if ((p_begin == NULL) || (p_end == NULL) || (p_begin > p_end)) {
      return false;
   }
   if (p_begin == p_end) {
      return true;
   }
   bool first = true;
   const char *p_next = p_begin;
   while (p_next < p_end) {
      char c = *p_next++;
      if (first) {
         first = false;
         if ((c != '.') && (c != '_') && (c != '-') && (c != '~') && !isalpha((unsigned char)c)) {
            return false;
         }
      } else {
         if ((c != '.') && (c != '_') && (c != '-') && (c != '~') && !isalnum((unsigned char)c)) {
            return false;
         }
      }
   }
   return true;
}

msocket_endpoint_type_t msocket_parse_endpoint(const char *text, adt_str_t **address, uint16_t *port)
{
   if (text == NULL || address == NULL) {
      if (port != NULL) {
         *port = 0;
      }
      return MSOCKET_ENDPOINT_ERROR;
   }

   *address = NULL;
   if (port != NULL) {
      *port = 0;
   }

   if (*text == '\0') {
      return MSOCKET_ENDPOINT_ERROR;
   }

   msocket_endpoint_type_t retval = MSOCKET_ENDPOINT_UNKNOWN;
   adt_str_t *parsed_address = NULL;
   unsigned long parsed_port = 0;

   /* 1. Bracketed IPv6: e.g. [::1]:5000 or [::1] */
   if (*text == '[') {
      const char *closing_bracket = strchr(text, ']');
      if (closing_bracket == NULL) {
         return MSOCKET_ENDPOINT_ERROR;
      }
      size_t ip_len = (size_t)(closing_bracket - (text + 1));
      if (ip_len == 0 || ip_len >= MSOCKET_ADDRSTRLEN) {
         return MSOCKET_ENDPOINT_ERROR;
      }
      char ip_buf[MSOCKET_ADDRSTRLEN];
      memcpy(ip_buf, text + 1, ip_len);
      ip_buf[ip_len] = '\0';

      struct in6_addr sin6_addr;
      memset(&sin6_addr, 0, sizeof(sin6_addr));
      if (inet_pton(AF_INET6, ip_buf, &sin6_addr) != 1) {
         return MSOCKET_ENDPOINT_ERROR;
      }

      const char *after = closing_bracket + 1;
      if (*after == '\0') {
         parsed_port = 0;
      } else if (*after == ':') {
         after++;
         if (*after == '\0') {
            return MSOCKET_ENDPOINT_ERROR;
         }
         char *endptr = NULL;
         parsed_port = strtoul(after, &endptr, 10);
         if (endptr == after || *endptr != '\0' || parsed_port > 65535) {
            return MSOCKET_ENDPOINT_ERROR;
         }
      } else {
         return MSOCKET_ENDPOINT_ERROR;
      }

      parsed_address = adt_str_new_cstr(ip_buf);
      if (parsed_address == NULL) {
         return MSOCKET_ENDPOINT_ERROR;
      }
      retval = MSOCKET_ENDPOINT_IPV6;
   }
   /* 2. UNIX socket or file path */
   else if (strchr(text, '/') != NULL || strchr(text, '\\') != NULL) {
      parsed_address = adt_str_new_cstr(text);
      if (parsed_address == NULL) {
         return MSOCKET_ENDPOINT_ERROR;
      }
      parsed_port = 0;
      retval = MSOCKET_ENDPOINT_FILE;
   }
   else {
      /* Count colons to distinguish bare IPv6 from hostname/IPv4 with optional port */
      int colon_count = 0;
      for (const char *p = text; *p != '\0'; p++) {
         if (*p == ':') {
            colon_count++;
         }
      }

      if (colon_count >= 2) {
         /* Bare IPv6 without brackets. Cannot have port. */
         struct in6_addr sin6_addr;
         memset(&sin6_addr, 0, sizeof(sin6_addr));
         if (inet_pton(AF_INET6, text, &sin6_addr) == 1) {
            parsed_address = adt_str_new_cstr(text);
            if (parsed_address == NULL) {
               return MSOCKET_ENDPOINT_ERROR;
            }
            parsed_port = 0;
            retval = MSOCKET_ENDPOINT_IPV6;
         } else {
            return MSOCKET_ENDPOINT_ERROR;
         }
      } else {
         const char *port_begin = NULL;
         if (colon_count == 1) {
            port_begin = strchr(text, ':');
            const char *port_str = port_begin + 1;
            if (*port_str == '\0') {
               return MSOCKET_ENDPOINT_ERROR;
            }
            char *parse_end = NULL;
            parsed_port = strtoul(port_str, &parse_end, 10);
            if (parse_end == port_str || *parse_end != '\0' || parsed_port > 65535) {
               return MSOCKET_ENDPOINT_ERROR;
            }
         } else {
            port_begin = text + strlen(text);
            parsed_port = 0;
         }

         if (verify_ipv4_address(text, port_begin)) {
            parsed_address = adt_str_new_bstr((const uint8_t *)text, (const uint8_t *)port_begin);
            if (parsed_address == NULL) {
               return MSOCKET_ENDPOINT_ERROR;
            }
            retval = MSOCKET_ENDPOINT_IPV4;
         } else if (verify_name(text, port_begin)) {
            parsed_address = adt_str_new_bstr((const uint8_t *)text, (const uint8_t *)port_begin);
            if (parsed_address == NULL) {
               return MSOCKET_ENDPOINT_ERROR;
            }
            retval = MSOCKET_ENDPOINT_NAME;
         } else {
            return MSOCKET_ENDPOINT_ERROR;
         }
      }
   }

   if (retval != MSOCKET_ENDPOINT_ERROR && retval != MSOCKET_ENDPOINT_UNKNOWN) {
      *address = parsed_address;
      if (port != NULL) {
         *port = (uint16_t)parsed_port;
      }
   } else {
      if (parsed_address != NULL) {
         adt_str_delete(parsed_address);
      }
      *address = NULL;
      if (port != NULL) {
         *port = 0;
      }
   }

   return retval;
}

