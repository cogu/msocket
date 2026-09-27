/*****************************************************************************
* \file      msocket_tls.h
* \author    Conny Gustafsson
* \date      2026-09-27
* \brief     msocket TLS internal declarations and Mbed TLS wrappers
*
* Copyright (c) 2026 Conny Gustafsson
* SPDX-License-Identifier: MIT
* See LICENSE in project root for full license terms.
******************************************************************************/

#ifndef MSOCKET_TLS_INTERNAL_H
#define MSOCKET_TLS_INTERNAL_H

#ifdef __cplusplus
extern "C" {
#endif

//////////////////////////////////////////////////////////////////////////////
// INCLUDES
//////////////////////////////////////////////////////////////////////////////
#include "msocket_tls.h"
#include "msocket_internal.h"

#if defined(MSOCKET_ENABLE_TLS)

#include "mbedtls/ssl.h"
#include "mbedtls/entropy.h"
#include "mbedtls/ctr_drbg.h"
#include "mbedtls/x509_crt.h"
#include "mbedtls/pk.h"
#include "mbedtls/error.h"

//////////////////////////////////////////////////////////////////////////////
// PUBLIC CONSTANTS AND DATA TYPES
//////////////////////////////////////////////////////////////////////////////
struct msocket_tls_server_tag {
   mbedtls_ssl_config conf;
   mbedtls_x509_crt srvcert;
   mbedtls_pk_context pkey;
   mbedtls_x509_crt cacert;
   mbedtls_entropy_context entropy;
   mbedtls_ctr_drbg_context ctr_drbg;
   bool has_ca;
   bool require_client_cert;
};

struct msocket_tls_client_tag {
   mbedtls_ssl_config conf;
   mbedtls_x509_crt clicert;
   mbedtls_pk_context pkey;
   mbedtls_x509_crt cacert;
   mbedtls_entropy_context entropy;
   mbedtls_ctr_drbg_context ctr_drbg;
   bool has_ca;
   bool has_cert;
};

struct msocket_tls_tag {
   mbedtls_ssl_context ssl;
   os_socket_t fd;
   bool is_server;
   bool handshake_done;
   struct msocket_tls_client_tag *client_ctx;
};

//////////////////////////////////////////////////////////////////////////////
// PUBLIC FUNCTION PROTOTYPES
//////////////////////////////////////////////////////////////////////////////

/* Server TLS lifecycle */
msocket_tls_server_t *msocket_tls_server_new(const msocket_tls_config_t *config);
void msocket_tls_server_delete(msocket_tls_server_t *self);
msocket_tls_t *msocket_tls_server_accept(msocket_tls_server_t *srv, os_socket_t fd);

/* Client TLS lifecycle */
msocket_tls_client_t *msocket_tls_client_new(const msocket_tls_config_t *config);
void msocket_tls_client_delete(msocket_tls_client_t *self);
msocket_tls_t *msocket_tls_client_attach(msocket_tls_client_t *cli, os_socket_t fd, const char *hostname);

/* Connection TLS I/O */
msocket_error_t msocket_tls_handshake(msocket_tls_t *self);
bool msocket_tls_has_pending(const msocket_tls_t *self);
int msocket_tls_read(msocket_tls_t *self, uint8_t *buf, uint32_t len);
int msocket_tls_write(msocket_tls_t *self, const uint8_t *buf, uint32_t len);
void msocket_tls_close(msocket_tls_t *self);
void msocket_tls_delete(msocket_tls_t *self);

#endif /* MSOCKET_ENABLE_TLS */

#ifdef __cplusplus
}
#endif

#endif /* MSOCKET_TLS_INTERNAL_H */
