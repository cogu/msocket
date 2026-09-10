/*****************************************************************************
* \file      msocket_server.h
* \author    Conny Gustafsson
* \date      2014-12-18
* \brief     msocket server connection and cleanup manager
*
* Copyright (c) 2014-2026 Conny Gustafsson
* SPDX-License-Identifier: MIT
* See LICENSE in project root for full license terms.
******************************************************************************/

#ifndef MSOCKET_SERVER_H
#define MSOCKET_SERVER_H

#ifdef __cplusplus
extern "C" {
#endif

//////////////////////////////////////////////////////////////////////////////
// INCLUDES
//////////////////////////////////////////////////////////////////////////////
#include "msocket.h"
#include "adt_ary.h"

//////////////////////////////////////////////////////////////////////////////
// PUBLIC CONSTANTS AND DATA TYPES
//////////////////////////////////////////////////////////////////////////////
typedef struct msocket_server_os_t msocket_server_os_t;

typedef struct msocket_server_tag {
   msocket_t *accept_socket;
   uint16_t tcp_port;
   uint16_t udp_port;
   char *udp_addr;
   char *socket_path;
   adt_ary_t cleanup_items;
   uint8_t cleanup_stop;
   uint8_t address_family;
   void *handler_arg;
   msocket_handler_t handler_table;
   void (*destructor)(void *arg);
   msocket_server_os_t *os;
} msocket_server_t;

//////////////////////////////////////////////////////////////////////////////
// PUBLIC FUNCTION PROTOTYPES
//////////////////////////////////////////////////////////////////////////////

/**
 * Initializes an existing msocket_server instance.
 *
 * @param self Pointer to msocket_server_t instance.
 * @param address_family Address family (MSOCKET_ADDR_INET, MSOCKET_ADDR_INET6, or MSOCKET_ADDR_UNIX).
 * @param destructor Destructor function for cleanup items, or NULL to use default msocket_vdelete.
 */
void msocket_server_create(msocket_server_t *self, uint8_t address_family, void (*destructor)(void *));

/**
 * Destroys an msocket_server instance, terminating accept and cleanup threads.
 *
 * @param self Pointer to msocket_server_t instance.
 */
void msocket_server_destroy(msocket_server_t *self);

/**
 * Dynamically allocates and initializes a new msocket_server instance.
 *
 * @param address_family Address family (MSOCKET_ADDR_INET, MSOCKET_ADDR_INET6, or MSOCKET_ADDR_UNIX).
 * @param destructor Destructor function for cleanup items, or NULL to use default msocket_vdelete.
 * @return Pointer to newly allocated msocket_server_t, or NULL on failure.
 */
msocket_server_t *msocket_server_new(uint8_t address_family, void (*destructor)(void *));

/**
 * Destroys and frees an msocket_server instance previously allocated with msocket_server_new.
 *
 * @param self Pointer to msocket_server_t instance.
 */
void msocket_server_delete(msocket_server_t *self);

/**
 * Registers the server handler table.
 *
 * The `handler->tcp_accept` callback is called whenever a new client connection is accepted:
 *   void on_accept(void *arg, msocket_server_t *srv, msocket_t *child_socket)
 * Inside that callback, the application should attach per-connection handlers to `child_socket`
 * via msocket_set_handler(), and then call msocket_start_io(child_socket).
 *
 * @param self Pointer to msocket_server_t instance.
 * @param handler Pointer to handler table containing tcp_accept callback.
 * @param handler_arg User context pointer passed to callback functions.
 */
void msocket_server_set_handler(msocket_server_t *self, const msocket_handler_t *handler, void *handler_arg);

/**
 * Binds listening sockets and starts the accept thread (and cleanup thread).
 *
 * @param self Pointer to msocket_server_t instance.
 * @param udp_addr Optional address to bind UDP port to, or NULL for default.
 * @param udp_port UDP port to listen on (0 to disable UDP).
 * @param tcp_port TCP port to listen on (0 to disable TCP).
 */
void msocket_server_start(msocket_server_t *self, const char *udp_addr, uint16_t udp_port, uint16_t tcp_port);

/**
 * Binds a listening UNIX domain socket and starts the server threads (POSIX only).
 *
 * @param self Pointer to msocket_server_t instance.
 * @param socket_path Path to UNIX domain socket.
 */
void msocket_server_unix_start(msocket_server_t *self, const char *socket_path);

/**
 * Disables the automatic background connection cleanup thread.
 *
 * @param self Pointer to msocket_server_t instance.
 */
void msocket_server_disable_cleanup(msocket_server_t *self);

/**
 * Enqueues a closed connection socket or wrapper item for asynchronous deletion by the cleanup thread.
 *
 * Safe to call from within client connection callbacks (e.g. `tcp_disconnected`).
 *
 * @param self Pointer to msocket_server_t instance.
 * @param arg Pointer to connection object (passed to pDestructor).
 */
void msocket_server_cleanup_connection(msocket_server_t *self, void *arg);

/* Backwards compatibility */
#define msocket_server_sethandler(s, t, a) msocket_server_set_handler(s, t, a)

#ifdef __cplusplus
}
#endif

#endif /* MSOCKET_SERVER_H */
