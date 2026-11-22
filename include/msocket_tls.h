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

/**
 * TLS and mutual TLS (mTLS) configuration parameters.
 */
struct msocket_tls_config_tag {
   char *ca_cert_path;       /**< Path to trusted CA certificate file (PEM format). */
   char *server_cert_path;   /**< Path to server certificate file (PEM format). */
   char *server_key_path;    /**< Path to server private key file (PEM format). */
   char *client_cert_path;   /**< Path to client certificate file (PEM format, for mTLS). */
   char *client_key_path;    /**< Path to client private key file (PEM format, for mTLS). */
   bool require_client_cert; /**< True to require and verify client certificates (mTLS). */
};

//////////////////////////////////////////////////////////////////////////////
// PUBLIC FUNCTION PROTOTYPES
//////////////////////////////////////////////////////////////////////////////

/**
 * Initializes a TLS configuration structure.
 *
 * @param self Pointer to msocket_tls_config_t instance.
 */
void msocket_tls_config_create(msocket_tls_config_t *self);

/**
 * Frees internal resources of a TLS configuration structure.
 *
 * @param self Pointer to msocket_tls_config_t instance.
 */
void msocket_tls_config_destroy(msocket_tls_config_t *self);

/**
 * Dynamically allocates and initializes a new TLS configuration structure.
 *
 * @return Pointer to newly allocated msocket_tls_config_t, or NULL on failure.
 */
msocket_tls_config_t *msocket_tls_config_new(void);

/**
 * Destroys and frees a TLS configuration structure previously allocated with msocket_tls_config_new.
 *
 * @param self Pointer to msocket_tls_config_t instance.
 */
void msocket_tls_config_delete(msocket_tls_config_t *self);

/**
 * Sets the path to the trusted CA certificate file (PEM format).
 *
 * @param self Pointer to msocket_tls_config_t instance.
 * @param ca_cert_path Path to CA certificate file (copied internally), or NULL to clear.
 * @return MSOCKET_NO_ERROR on success, or error code on failure.
 */
msocket_error_t msocket_tls_config_set_ca_cert(msocket_tls_config_t *self, const char *ca_cert_path);

/**
 * Sets the server certificate and private key file paths (PEM format).
 *
 * @param self Pointer to msocket_tls_config_t instance.
 * @param cert_path Path to server certificate file (copied internally).
 * @param key_path Path to server private key file (copied internally).
 * @return MSOCKET_NO_ERROR on success, or error code on failure.
 */
msocket_error_t msocket_tls_config_set_server_cert(msocket_tls_config_t *self, const char *cert_path, const char *key_path);

/**
 * Sets the client certificate and private key file paths (PEM format, for mTLS).
 *
 * @param self Pointer to msocket_tls_config_t instance.
 * @param cert_path Path to client certificate file (copied internally).
 * @param key_path Path to client private key file (copied internally).
 * @return MSOCKET_NO_ERROR on success, or error code on failure.
 */
msocket_error_t msocket_tls_config_set_client_cert(msocket_tls_config_t *self, const char *cert_path, const char *key_path);

/**
 * Enables or disables mandatory client certificate verification (mTLS).
 *
 * @param self Pointer to msocket_tls_config_t instance.
 * @param require True to require and verify client certificates against the configured CA.
 */
void msocket_tls_config_set_require_client_cert(msocket_tls_config_t *self, bool require);

#ifdef __cplusplus
}
#endif

#endif /* MSOCKET_TLS_H */
