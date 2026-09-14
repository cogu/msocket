/*****************************************************************************
* \file:    msocket.h
* \author:  Conny Gustafsson
* \date:    2014-10-01
* \brief:   Event-driven socket library for Linux and Windows
*
* Copyright (c) 2014-2026 Conny Gustafsson
* SPDX-License-Identifier: MIT
* See LICENSE in project root for full license terms.
******************************************************************************/

#ifndef MSOCKET_H
#define MSOCKET_H

#ifdef __cplusplus
extern "C" {
#endif

//////////////////////////////////////////////////////////////////////////////
// INCLUDES
//////////////////////////////////////////////////////////////////////////////
#include <stdint.h>
#include <stdbool.h>
#include "adt_streambuffer.h"
#include "adt_str.h"
#include "msocket_error.h"

//////////////////////////////////////////////////////////////////////////////
// PUBLIC CONSTANTS AND DATA TYPES
//////////////////////////////////////////////////////////////////////////////

#define MSOCKET_ENDPOINT_UNKNOWN    0
#define MSOCKET_ENDPOINT_IPV4       1
#define MSOCKET_ENDPOINT_IPV6       2
#define MSOCKET_ENDPOINT_FILE       3
#define MSOCKET_ENDPOINT_NAME       4
#define MSOCKET_ENDPOINT_ERROR      5

typedef int8_t msocket_endpoint_type_t;


#define MSOCKET_STATE_NONE          0u
#define MSOCKET_STATE_LISTENING     1u
#define MSOCKET_STATE_ACCEPTING     2u
#define MSOCKET_STATE_PENDING       3u
#define MSOCKET_STATE_ESTABLISHED   4u
#define MSOCKET_STATE_CLOSING       5u
#define MSOCKET_STATE_CLOSED        6u

typedef uint8_t msocket_state_t;

#define MSOCKET_MODE_NONE           0u
#define MSOCKET_MODE_DGRAM          1u
#define MSOCKET_MODE_STREAM         2u

#define MSOCKET_ADDR_INET           0u
#define MSOCKET_ADDR_UNIX           1u
#define MSOCKET_ADDR_INET6          3u

#define MSOCKET_ADDRSTRLEN 46u

struct msocket_tag;
struct msocket_server_tag;
struct msocket_os_tag;

/**
 * Event handler callback table for stream and datagram socket events.
 */
typedef struct msocket_handler_tag {
   /**
    * Callback invoked when a new incoming stream connection is accepted by a server.
    *
    * @param arg User context pointer registered via msocket_server_set_handler.
    * @param srv Pointer to the listening msocket_server_t instance.
    * @param socket Pointer to the newly accepted child socket (msocket_t*).
    */
   void (*stream_accept)(void *arg, struct msocket_server_tag *srv, void *socket);

   /**
    * Callback invoked when an outgoing stream connection has been established.
    *
    * @param arg User context pointer registered via msocket_set_handler.
    * @param socket Pointer to the connected msocket_t instance.
    * @param addr Remote peer IP address or socket path string.
    * @param port Remote peer TCP port (0 for UNIX sockets).
    */
   void (*stream_connected)(void *arg, void *socket, const char *addr, uint16_t port);

   /**
    * Callback invoked when a stream connection is closed or disconnected.
    *
    * @param arg User context pointer registered via msocket_set_handler.
    * @param socket Pointer to the disconnected msocket_t instance.
    */
   void (*stream_disconnected)(void *arg, void *socket);

   /**
    * Callback invoked when stream data arrives in the receive buffer.
    *
    * @param arg User context pointer registered via msocket_set_handler.
    * @param socket Pointer to the msocket_t instance receiving data.
    * @param data Pointer to raw received byte buffer.
    * @param num_bytes Total number of bytes available in the buffer.
    * @param consumed_bytes Output parameter: number of bytes parsed and consumed by the handler.
    * @param msg_size_hint Output parameter: optional hint for expected total message length.
    * @return MSOCKET_NO_ERROR on success, or error code on protocol/parsing failure.
    */
   msocket_error_t (*stream_data)(void *arg, void *socket, const uint8_t *data, const uint32_t num_bytes, uint32_t *consumed_bytes, uint32_t *msg_size_hint);

   /**
    * Callback periodically invoked during periods of socket inactivity.
    *
    * @param arg User context pointer registered via msocket_set_handler.
    * @param socket Pointer to the idle msocket_t instance.
    * @param elapsed_ms Milliseconds elapsed since last activity.
    */
   void (*stream_inactivity)(void *arg, void *socket, const uint32_t elapsed_ms);

   /**
    * Callback invoked when a datagram (UDP packet) is received.
    *
    * @param arg User context pointer registered via msocket_set_handler.
    * @param socket Pointer to the receiving msocket_t instance.
    * @param addr Sender IP address string.
    * @param port Sender UDP port.
    * @param data Pointer to received datagram payload.
    * @param num_bytes Length of received payload in bytes.
    */
   void (*datagram_msg)(void *arg, void *socket, const char *addr, uint16_t port, const uint8_t *data, const uint32_t num_bytes);
} msocket_handler_t;

typedef struct msocket_addr_info_tag {
   uint16_t port;
   char addr[MSOCKET_ADDRSTRLEN];
} msocket_addr_info_t;

typedef msocket_addr_info_t msocketAddrInfo_t;

typedef struct msocket_tag {
   msocket_addr_info_t stream_info;
   msocket_addr_info_t udp_info;
   adt_streambuffer_t stream_rx_buf;
   msocket_handler_t *handler_table;
   void *handler_arg;
   msocket_state_t state;
   uint8_t socket_mode;
   uint8_t address_family;
   uint32_t inactivity_ms;
   uint32_t inactivity_call_ms;
   struct msocket_server_tag *server;
   struct msocket_os_tag *os;
} msocket_t;

//////////////////////////////////////////////////////////////////////////////
// PUBLIC FUNCTION PROTOTYPES
//////////////////////////////////////////////////////////////////////////////

/**
 * Initializes an existing msocket instance.
 *
 * @param self Pointer to msocket_t instance.
 * @param address_family Address family (MSOCKET_ADDR_INET, MSOCKET_ADDR_INET6, or MSOCKET_ADDR_UNIX).
 * @return MSOCKET_NO_ERROR on success, or error code on failure.
 */
msocket_error_t msocket_create(msocket_t *self, uint8_t address_family);

/**
 * Destroys an msocket instance, closing any active connection and releasing internal resources.
 *
 * @param self Pointer to msocket_t instance.
 */
void msocket_destroy(msocket_t *self);

/**
 * Dynamically allocates and initializes a new msocket instance.
 *
 * @param address_family Address family (MSOCKET_ADDR_INET, MSOCKET_ADDR_INET6, or MSOCKET_ADDR_UNIX).
 * @return Pointer to newly allocated msocket_t, or NULL on failure.
 */
msocket_t *msocket_new(uint8_t address_family);

/**
 * Destroys and frees an msocket instance previously allocated with msocket_new.
 *
 * @param self Pointer to msocket_t instance.
 */
void msocket_delete(msocket_t *self);

/**
 * Generic void-pointer destructor compatible with container callbacks (e.g. adt_ary_t).
 *
 * @param arg Pointer to msocket_t instance.
 */
void msocket_vdelete(void *arg);

/**
 * Closes active socket descriptors and terminates the background I/O thread.
 * Safe to call from within handler callbacks (prevents self-joining deadlocks).
 *
 * @param self Pointer to msocket_t instance.
 */
void msocket_close(msocket_t *self);

/**
 * Binds and configures a listening TCP socket or receiving UDP socket.
 *
 * @param self Pointer to msocket_t instance.
 * @param mode MSOCKET_MODE_STREAM or MSOCKET_MODE_DGRAM.
 * @param port Port number to bind (0 for OS-assigned ephemeral port).
 * @param addr IP address string to bind to, or NULL for INADDR_ANY.
 * @return MSOCKET_NO_ERROR on success, or error code on failure.
 */
msocket_error_t msocket_listen(msocket_t *self, uint8_t mode, uint16_t port, const char *addr);

/**
 * Binds and configures a listening UNIX domain socket (POSIX only).
 *
 * @param self Pointer to msocket_t instance.
 * @param socket_path Path to UNIX domain socket (abstract namespace supported if starting with \0).
 * @return MSOCKET_NO_ERROR on success, or error code on failure.
 */
msocket_error_t msocket_unix_listen(msocket_t *self, const char *socket_path);

/**
 * Accepts an incoming client connection on a listening TCP or UNIX socket.
 *
 * @param self Listening msocket_t instance.
 * @param child Optional pre-allocated child socket, or NULL to dynamically allocate one.
 * @return Pointer to accepted child socket, or NULL on error / no pending connection.
 */
msocket_t *msocket_accept(msocket_t *self, msocket_t *child);

/**
 * Registers the event handler callback table and optional user context argument.
 *
 * @param self Pointer to msocket_t instance.
 * @param handler_table Pointer to handler callback table (copied internally).
 * @param handler_arg User context pointer passed to callback functions.
 */
void msocket_set_handler(msocket_t *self, const msocket_handler_t *handler_table, void *handler_arg);

/**
 * Associates a parent msocket_server instance with this socket for automatic reaping upon disconnection.
 *
 * @param self Pointer to msocket_t instance.
 * @param server Pointer to parent msocket_server instance, or NULL to detach.
 */
void msocket_set_server(msocket_t *self, struct msocket_server_tag *server);

/**
 * Starts the background I/O event thread for this socket.
 *
 * NOTE: For outgoing client connections, msocket_connect() and msocket_unix_connect()
 * start the I/O thread automatically.
 *
 * This function is primarily intended for server connection sockets accepted via
 * msocket_accept() / tcp_accept callback: once per-client callbacks and context have
 * been registered via msocket_set_handler(), msocket_start_io() must be called to
 * start receiving data on that connection.
 *
 * @param self Pointer to msocket_t instance.
 * @return MSOCKET_NO_ERROR on success, or error code on failure.
 */
msocket_error_t msocket_start_io(msocket_t *self);

/**
 * Initiates an outgoing TCP connection to a remote host.
 *
 * Requires a handler table to be registered beforehand via msocket_set_handler().
 * Upon connection, the background I/O event loop is launched automatically.
 *
 * @param self Pointer to msocket_t instance.
 * @param addr Target IP address string (IPv4 or IPv6).
 * @param port Target TCP port.
 * @return MSOCKET_NO_ERROR on success, or error code on failure.
 */
msocket_error_t msocket_connect(msocket_t *self, const char *addr, uint16_t port);

/**
 * Initiates an outgoing UNIX domain socket connection.
 *
 * Requires a handler table to be registered beforehand via msocket_set_handler().
 * Upon connection, the background I/O event loop is launched automatically.
 *
 * @param self Pointer to msocket_t instance.
 * @param socket_path Path to remote UNIX socket.
 * @return MSOCKET_NO_ERROR on success, or error code on failure.
 */
msocket_error_t msocket_unix_connect(msocket_t *self, const char *socket_path);

/**
 * Sends data to a specific destination over UDP.
 *
 * @param self Pointer to msocket_t instance (configured in MSOCKET_MODE_DGRAM).
 * @param addr Destination IP address string.
 * @param port Destination UDP port.
 * @param msg_data Pointer to data payload.
 * @param msg_len Length of payload in bytes.
 * @return MSOCKET_NO_ERROR on success, or error code on failure.
 */
msocket_error_t msocket_send_to(msocket_t *self, const char *addr, uint16_t port, const void *msg_data, uint32_t msg_len);

/**
 * Sends stream data over an established TCP connection.
 *
 * @param self Pointer to msocket_t instance (configured in MSOCKET_MODE_STREAM).
 * @param msg_data Pointer to data payload.
 * @param msg_len Length of payload in bytes.
 * @return MSOCKET_NO_ERROR on success, or error code on failure.
 */
msocket_error_t msocket_send(msocket_t *self, const void *msg_data, uint32_t msg_len);

/**
 * Returns the current state of the socket (e.g. MSOCKET_STATE_ESTABLISHED, MSOCKET_STATE_CLOSING).
 *
 * @param self Pointer to msocket_t instance.
 * @return Socket state code.
 */
msocket_state_t msocket_state(msocket_t *self);

/**
 * Parses an endpoint string and determines whether it is an IPv4 address,
 * IPv6 address, UNIX domain socket path, or hostname.
 *
 * If the string ends with a port number (e.g. ":5000" or "[::1]:5000"),
 * it parses the port. For file paths and bare IPv6 addresses without ports,
 * port is set to 0.
 *
 * The caller is responsible for freeing the returned address string via adt_str_delete().
 *
 * @param text The input string to parse.
 * @param address Pointer to adt_str_t* where the parsed address string is stored.
 * @param port Optional pointer to uint16_t where the port number is stored (can be NULL).
 * @return Endpoint type (MSOCKET_ENDPOINT_*). Returns MSOCKET_ENDPOINT_ERROR on parse failure.
 */
msocket_endpoint_type_t msocket_parse_endpoint(const char *text, adt_str_t **address, uint16_t *port);

/* Backwards compatibility */
#define msocket_sethandler(s, t, a) msocket_set_handler(s, t, a)
#define msocket_sendto(s, a, p, d, l) msocket_send_to(s, a, p, d, l)
#define MSOCKET_MODE_UDP            MSOCKET_MODE_DGRAM
#define MSOCKET_MODE_TCP            MSOCKET_MODE_STREAM
#define tcp_accept                  stream_accept
#define tcp_connected               stream_connected
#define tcp_disconnected            stream_disconnected
#define tcp_data                    stream_data
#define tcp_inactivity              stream_inactivity
#define udp_msg                     datagram_msg
#define tcp_rx_buf                  stream_rx_buf
#define tcp_info                    stream_info

#ifdef __cplusplus
}
#endif

#endif /* MSOCKET_H */
