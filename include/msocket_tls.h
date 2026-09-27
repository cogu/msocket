/*****************************************************************************
* \file      msocket_tls.h
* \author    Conny Gustafsson
* \date      2026-09-27
* \brief     msocket TLS configuration and transport interface
*
* Copyright (c) 2026 Conny Gustafsson
* SPDX-License-Identifier: MIT
* See LICENSE in project root for full license terms.
******************************************************************************/

#ifndef MSOCKET_TLS_H
#define MSOCKET_TLS_H

#ifdef __cplusplus
extern "C" {
#endif

//////////////////////////////////////////////////////////////////////////////
// INCLUDES
//////////////////////////////////////////////////////////////////////////////
#include <stdint.h>
#include <stdbool.h>
#include "msocket_error.h"

//////////////////////////////////////////////////////////////////////////////
// PUBLIC CONSTANTS AND DATA TYPES
//////////////////////////////////////////////////////////////////////////////
struct msocket_tls_config_tag;
typedef struct msocket_tls_config_tag msocket_tls_config_t;

struct msocket_tls_tag;
typedef struct msocket_tls_tag msocket_tls_t;

struct msocket_tls_server_tag;
typedef struct msocket_tls_server_tag msocket_tls_server_t;

struct msocket_tls_client_tag;
typedef struct msocket_tls_client_tag msocket_tls_client_t;

struct msocket_tls_config_tag {
   char *ca_cert_path;
   char *server_cert_path;
   char *server_key_path;
   char *client_cert_path;
   char *client_key_path;
   bool require_client_cert;
};

//////////////////////////////////////////////////////////////////////////////
// PUBLIC FUNCTION PROTOTYPES
//////////////////////////////////////////////////////////////////////////////

/**
 * Initializes a TLS configuration structure.
 */
void msocket_tls_config_create(msocket_tls_config_t *self);

/**
 * Frees internal resources of a TLS configuration structure.
 */
void msocket_tls_config_destroy(msocket_tls_config_t *self);

/**
 * Dynamically allocates and initializes a new TLS configuration structure.
 */
msocket_tls_config_t *msocket_tls_config_new(void);

/**
 * Destroys and frees a TLS configuration structure.
 */
void msocket_tls_config_delete(msocket_tls_config_t *self);

/**
 * Sets path to trusted CA certificate file (PEM format).
 */
msocket_error_t msocket_tls_config_set_ca_cert(msocket_tls_config_t *self, const char *ca_cert_path);

/**
 * Sets server certificate and private key paths (PEM format).
 */
msocket_error_t msocket_tls_config_set_server_cert(msocket_tls_config_t *self, const char *cert_path, const char *key_path);

/**
 * Sets client certificate and private key paths (PEM format, for mTLS).
 */
msocket_error_t msocket_tls_config_set_client_cert(msocket_tls_config_t *self, const char *cert_path, const char *key_path);

/**
 * Enables or disables required client certificate verification (mTLS).
 */
void msocket_tls_config_set_require_client_cert(msocket_tls_config_t *self, bool require);

#ifdef __cplusplus
}
#endif

#endif /* MSOCKET_TLS_H */
