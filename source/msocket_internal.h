/*****************************************************************************
* \file:    msocket_internal.h
* \author:  Conny Gustafsson
* \date:    2026-09-10
* \brief:   Internal interface between platform-independent and platform-specific layers
*
* Copyright (c) 2014-2026 Conny Gustafsson
* SPDX-License-Identifier: MIT
* See LICENSE in project root for full license terms.
******************************************************************************/

#ifndef MSOCKET_INTERNAL_H
#define MSOCKET_INTERNAL_H

//////////////////////////////////////////////////////////////////////////////
// INCLUDES
//////////////////////////////////////////////////////////////////////////////
#include "msocket.h"
#include "msocket_server.h"

//////////////////////////////////////////////////////////////////////////////
// PLATFORM HEADERS & MACROS
//////////////////////////////////////////////////////////////////////////////

#if defined(_WIN32)
# ifndef WIN32_LEAN_AND_MEAN
#  define WIN32_LEAN_AND_MEAN
# endif
# include <winsock2.h>
# include <ws2tcpip.h>
# include <process.h>
# include <windows.h>

/* Sockets */
typedef SOCKET os_socket_t;
# define OS_SOCKET_INVALID           INVALID_SOCKET
# define OS_SOCKET_IS_INVALID(s)     ((s) == INVALID_SOCKET)
# define OS_SOCKET_IS_VALID(s)       ((s) != INVALID_SOCKET)
# define OS_SOCKET_CLOSE(s)          closesocket(s)
# define OS_SOCKET_SHUTDOWN(s)       shutdown((s), SD_BOTH)
# define OS_SOCKOPT_CAST(ptr)        ((const char *)(ptr))
# define OS_SOCK_LEN_T               int
# define OS_SOCKET_ERRNO_IS_INTR()   (WSAGetLastError() == WSAEINTR)

/* Mutex */
# define MUTEX_T                     CRITICAL_SECTION
# define MUTEX_INIT(m)               InitializeCriticalSection(&(m))
# define MUTEX_LOCK(m)               EnterCriticalSection(&(m))
# define MUTEX_UNLOCK(m)             LeaveCriticalSection(&(m))
# define MUTEX_DESTROY(m)            DeleteCriticalSection(&(m))

/* Semaphore */
# define SEMAPHORE_T                 HANDLE
# define SEMAPHORE_CREATE(s, n)      ((s) = CreateSemaphore(NULL, (LONG)(n), 1000, NULL))
# define SEMAPHORE_POST(s)           ReleaseSemaphore((s), 1, NULL)
# define SEMAPHORE_DESTROY(s)        CloseHandle(s)

/* Thread */
# define THREAD_T                    HANDLE
# define THREAD_JOIN(t)              WaitForSingleObject((t), INFINITE)
# define THREAD_DESTROY(t)           CloseHandle(t)

/* Sleep */
# define SLEEP_MS(ms)                Sleep(ms)

#else
# include <sys/types.h>
# include <sys/socket.h>
# include <sys/un.h>
# include <sys/time.h>
# include <netinet/in.h>
# include <netinet/tcp.h>
# include <arpa/inet.h>
# include <unistd.h>
# include <pthread.h>
# include <semaphore.h>

/* Sockets */
typedef int os_socket_t;
# define OS_SOCKET_INVALID           (-1)
# define OS_SOCKET_IS_INVALID(s)     ((s) < 0)
# define OS_SOCKET_IS_VALID(s)       ((s) >= 0)
# define OS_SOCKET_CLOSE(s)          close(s)
# define OS_SOCKET_SHUTDOWN(s)       shutdown((s), SHUT_RDWR)
# define OS_SOCKOPT_CAST(ptr)        ((const void *)(ptr))
# define OS_SOCK_LEN_T               socklen_t
# define OS_SOCKET_ERRNO_IS_INTR()   (errno == EINTR)

/* Mutex */
# define MUTEX_T                     pthread_mutex_t
# define MUTEX_INIT(m)               pthread_mutex_init(&(m), NULL)
# define MUTEX_LOCK(m)               pthread_mutex_lock(&(m))
# define MUTEX_UNLOCK(m)             pthread_mutex_unlock(&(m))
# define MUTEX_DESTROY(m)            pthread_mutex_destroy(&(m))

/* Semaphore */
# define SEMAPHORE_T                 sem_t
# define SEMAPHORE_CREATE(s, n)      sem_init(&(s), 0, (unsigned int)(n))
# define SEMAPHORE_POST(s)           sem_post(&(s))
# define SEMAPHORE_DESTROY(s)        sem_destroy(&(s))

/* Thread */
# define THREAD_T                    pthread_t
# define THREAD_JOIN(t)              pthread_join((t), NULL)
# define THREAD_DESTROY(t)           /* no-op for pthread */

/* Sleep */
# define SLEEP_MS(ms)                usleep((ms) * 1000u)

#endif

//////////////////////////////////////////////////////////////////////////////
// PLATFORM THREADING & SYNC
//////////////////////////////////////////////////////////////////////////////
typedef struct msocket_thread_t msocket_thread_t;
typedef struct msocket_sem_t msocket_sem_t;

struct msocket_server_os_t {
   msocket_thread_t *accept_thread;
   msocket_thread_t *cleanup_thread;
   MUTEX_T mutex;
   msocket_sem_t *sem;
};

struct msocket_os_t {
   os_socket_t tcp_sockfd;
   os_socket_t udp_sockfd;
   msocket_thread_t *io_thread;
   MUTEX_T mutex;
   bool thread_running;
   bool new_connection;
};

/* Threading API */
msocket_thread_t *msocket_thread_create(void (*func)(void *arg), void *arg);
void msocket_thread_join(msocket_thread_t *thread);
void msocket_thread_delete(msocket_thread_t *thread);
bool msocket_thread_is_current(msocket_thread_t *thread);

/* Semaphore API (object wrapper) */
msocket_sem_t *msocket_sem_new(uint32_t initial_count);
void msocket_sem_delete(msocket_sem_t *sem);
void msocket_sem_post(msocket_sem_t *sem);
int8_t msocket_sem_test(msocket_sem_t *sem); /* returns 1 if decremented, 0 if would block, -1 on error */

static inline void msocket_sleep_ms(uint32_t ms)
{
   SLEEP_MS(ms);
}

//////////////////////////////////////////////////////////////////////////////
// PLATFORM SOCKET API
//////////////////////////////////////////////////////////////////////////////
void msocket_os_init(void);
void msocket_os_cleanup(void);

msocket_os_t *msocket_os_new(void);
void msocket_os_delete(msocket_os_t *os);

void msocket_os_mutex_lock(msocket_os_t *os);
void msocket_os_mutex_unlock(msocket_os_t *os);

msocket_error_t msocket_os_unix_listen(msocket_t *self, const char *socket_path);
msocket_error_t msocket_os_unix_connect(msocket_t *self, const char *socket_path);

//////////////////////////////////////////////////////////////////////////////
// COMMON CALLBACKS FROM PLATFORM
//////////////////////////////////////////////////////////////////////////////
void msocket_common_on_connected(msocket_t *self);
void msocket_common_on_disconnected(msocket_t *self);
msocket_error_t msocket_common_on_data(msocket_t *self, const uint8_t *data_buf, uint32_t data_len);
msocket_error_t msocket_common_process_stream_data(msocket_t *self);
void msocket_common_on_udp_msg(msocket_t *self, const char *addr, uint16_t port, const uint8_t *data_buf, uint32_t data_len);
void msocket_common_on_timeout(msocket_t *self);
void msocket_common_reset(msocket_t *self);

#endif /* MSOCKET_INTERNAL_H */
