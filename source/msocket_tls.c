/*****************************************************************************
* \file      msocket_tls.c
* \author    Conny Gustafsson
* \date      2026-09-27
* \brief     msocket TLS transport implementation using Mbed TLS
*
* Copyright (c) 2026 Conny Gustafsson
* SPDX-License-Identifier: MIT
* See LICENSE in project root for full license terms.
******************************************************************************/

//////////////////////////////////////////////////////////////////////////////
// INCLUDES
//////////////////////////////////////////////////////////////////////////////
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include "msocket_tls.h"
#include "msocket_tls_internal.h"
#include "mbedtls/net_sockets.h"

#if defined(MSOCKET_ENABLE_TLS)

//////////////////////////////////////////////////////////////////////////////
// PRIVATE CONSTANTS AND DATA TYPES
//////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////
// PRIVATE FUNCTION PROTOTYPES
//////////////////////////////////////////////////////////////////////////////
static int bio_send(void *ctx, const unsigned char *buf, size_t len);
static int bio_recv(void *ctx, unsigned char *buf, size_t len);

//////////////////////////////////////////////////////////////////////////////
// PUBLIC FUNCTIONS: CONFIGURATION
//////////////////////////////////////////////////////////////////////////////

void msocket_tls_config_create(msocket_tls_config_t *self)
{
   if (self != NULL) {
      self->ca_cert_path = NULL;
      self->server_cert_path = NULL;
      self->server_key_path = NULL;
      self->client_cert_path = NULL;
      self->client_key_path = NULL;
      self->require_client_cert = false;
   }
}

void msocket_tls_config_destroy(msocket_tls_config_t *self)
{
   if (self != NULL) {
      if (self->ca_cert_path != NULL) {
         free(self->ca_cert_path);
         self->ca_cert_path = NULL;
      }
      if (self->server_cert_path != NULL) {
         free(self->server_cert_path);
         self->server_cert_path = NULL;
      }
      if (self->server_key_path != NULL) {
         free(self->server_key_path);
         self->server_key_path = NULL;
      }
      if (self->client_cert_path != NULL) {
         free(self->client_cert_path);
         self->client_cert_path = NULL;
      }
      if (self->client_key_path != NULL) {
         free(self->client_key_path);
         self->client_key_path = NULL;
      }
   }
}

msocket_tls_config_t *msocket_tls_config_new(void)
{
   msocket_tls_config_t *self = (msocket_tls_config_t *)malloc(sizeof(msocket_tls_config_t));
   if (self != NULL) {
      msocket_tls_config_create(self);
   }
   return self;
}

void msocket_tls_config_delete(msocket_tls_config_t *self)
{
   if (self != NULL) {
      msocket_tls_config_destroy(self);
      free(self);
   }
}

msocket_error_t msocket_tls_config_set_ca_cert(msocket_tls_config_t *self, const char *ca_cert_path)
{
   if (self == NULL || ca_cert_path == NULL) {
      return MSOCKET_INVALID_ARGUMENT_ERROR;
   }
   if (self->ca_cert_path != NULL) {
      free(self->ca_cert_path);
   }
   size_t len = strlen(ca_cert_path) + 1u;
   self->ca_cert_path = (char *)malloc(len);
   if (self->ca_cert_path == NULL) {
      return MSOCKET_MEM_ERROR;
   }
   memcpy(self->ca_cert_path, ca_cert_path, len);
   return MSOCKET_NO_ERROR;
}

msocket_error_t msocket_tls_config_set_server_cert(msocket_tls_config_t *self, const char *cert_path, const char *key_path)
{
   if (self == NULL || cert_path == NULL || key_path == NULL) {
      return MSOCKET_INVALID_ARGUMENT_ERROR;
   }
   if (self->server_cert_path != NULL) {
      free(self->server_cert_path);
   }
   if (self->server_key_path != NULL) {
      free(self->server_key_path);
   }
   size_t cert_len = strlen(cert_path) + 1u;
   size_t key_len = strlen(key_path) + 1u;
   self->server_cert_path = (char *)malloc(cert_len);
   self->server_key_path = (char *)malloc(key_len);
   if (self->server_cert_path == NULL || self->server_key_path == NULL) {
      if (self->server_cert_path != NULL) { free(self->server_cert_path); self->server_cert_path = NULL; }
      if (self->server_key_path != NULL) { free(self->server_key_path); self->server_key_path = NULL; }
      return MSOCKET_MEM_ERROR;
   }
   memcpy(self->server_cert_path, cert_path, cert_len);
   memcpy(self->server_key_path, key_path, key_len);
   return MSOCKET_NO_ERROR;
}

msocket_error_t msocket_tls_config_set_client_cert(msocket_tls_config_t *self, const char *cert_path, const char *key_path)
{
   if (self == NULL || cert_path == NULL || key_path == NULL) {
      return MSOCKET_INVALID_ARGUMENT_ERROR;
   }
   if (self->client_cert_path != NULL) {
      free(self->client_cert_path);
   }
   if (self->client_key_path != NULL) {
      free(self->client_key_path);
   }
   size_t cert_len = strlen(cert_path) + 1u;
   size_t key_len = strlen(key_path) + 1u;
   self->client_cert_path = (char *)malloc(cert_len);
   self->client_key_path = (char *)malloc(key_len);
   if (self->client_cert_path == NULL || self->client_key_path == NULL) {
      if (self->client_cert_path != NULL) { free(self->client_cert_path); self->client_cert_path = NULL; }
      if (self->client_key_path != NULL) { free(self->client_key_path); self->client_key_path = NULL; }
      return MSOCKET_MEM_ERROR;
   }
   memcpy(self->client_cert_path, cert_path, cert_len);
   memcpy(self->client_key_path, key_path, key_len);
   return MSOCKET_NO_ERROR;
}

void msocket_tls_config_set_require_client_cert(msocket_tls_config_t *self, bool require)
{
   if (self != NULL) {
      self->require_client_cert = require;
   }
}

//////////////////////////////////////////////////////////////////////////////
// PUBLIC FUNCTIONS: SERVER & CLIENT LIFECYCLE
//////////////////////////////////////////////////////////////////////////////

msocket_tls_server_t *msocket_tls_server_new(const msocket_tls_config_t *config)
{
   if (config == NULL || config->server_cert_path == NULL || config->server_key_path == NULL) {
      return NULL;
   }

   msocket_tls_server_t *self = (msocket_tls_server_t *)calloc(1, sizeof(msocket_tls_server_t));
   if (self == NULL) {
      return NULL;
   }

   mbedtls_ssl_config_init(&self->conf);
   mbedtls_x509_crt_init(&self->srvcert);
   mbedtls_pk_init(&self->pkey);
   mbedtls_x509_crt_init(&self->cacert);
   mbedtls_entropy_init(&self->entropy);
   mbedtls_ctr_drbg_init(&self->ctr_drbg);

   const char *pers = "msocket_tls_server";
   int ret = mbedtls_ctr_drbg_seed(&self->ctr_drbg, mbedtls_entropy_func, &self->entropy,
                                   (const unsigned char *)pers, strlen(pers));
   if (ret != 0) {
      msocket_tls_server_delete(self);
      return NULL;
   }

   ret = mbedtls_x509_crt_parse_file(&self->srvcert, config->server_cert_path);
   if (ret != 0) {
      msocket_tls_server_delete(self);
      return NULL;
   }

   ret = mbedtls_pk_parse_keyfile(&self->pkey, config->server_key_path, NULL,
                                  mbedtls_ctr_drbg_random, &self->ctr_drbg);
   if (ret != 0) {
      msocket_tls_server_delete(self);
      return NULL;
   }

   ret = mbedtls_ssl_config_defaults(&self->conf,
                                     MBEDTLS_SSL_IS_SERVER,
                                     MBEDTLS_SSL_TRANSPORT_STREAM,
                                     MBEDTLS_SSL_PRESET_DEFAULT);
   if (ret != 0) {
      msocket_tls_server_delete(self);
      return NULL;
   }

   mbedtls_ssl_conf_rng(&self->conf, mbedtls_ctr_drbg_random, &self->ctr_drbg);

   ret = mbedtls_ssl_conf_own_cert(&self->conf, &self->srvcert, &self->pkey);
   if (ret != 0) {
      msocket_tls_server_delete(self);
      return NULL;
   }

   self->require_client_cert = config->require_client_cert;
   if (config->ca_cert_path != NULL) {
      ret = mbedtls_x509_crt_parse_file(&self->cacert, config->ca_cert_path);
      if (ret == 0) {
         self->has_ca = true;
         mbedtls_ssl_conf_ca_chain(&self->conf, &self->cacert, NULL);
         if (config->require_client_cert) {
            mbedtls_ssl_conf_authmode(&self->conf, MBEDTLS_SSL_VERIFY_REQUIRED);
         } else {
            mbedtls_ssl_conf_authmode(&self->conf, MBEDTLS_SSL_VERIFY_OPTIONAL);
         }
      }
   } else {
      mbedtls_ssl_conf_authmode(&self->conf, MBEDTLS_SSL_VERIFY_NONE);
   }

   return self;
}

void msocket_tls_server_delete(msocket_tls_server_t *self)
{
   if (self != NULL) {
      mbedtls_ssl_config_free(&self->conf);
      mbedtls_x509_crt_free(&self->srvcert);
      mbedtls_pk_free(&self->pkey);
      mbedtls_x509_crt_free(&self->cacert);
      mbedtls_ctr_drbg_free(&self->ctr_drbg);
      mbedtls_entropy_free(&self->entropy);
      free(self);
   }
}

msocket_tls_t *msocket_tls_server_accept(msocket_tls_server_t *srv, os_socket_t fd)
{
   if (srv == NULL || OS_SOCKET_IS_INVALID(fd)) {
      return NULL;
   }

   msocket_tls_t *self = (msocket_tls_t *)calloc(1, sizeof(msocket_tls_t));
   if (self == NULL) {
      return NULL;
   }

   self->fd = fd;
   self->is_server = true;
   self->handshake_done = false;

   mbedtls_ssl_init(&self->ssl);
   int ret = mbedtls_ssl_setup(&self->ssl, &srv->conf);
   if (ret != 0) {
      mbedtls_ssl_free(&self->ssl);
      free(self);
      return NULL;
   }

   mbedtls_ssl_set_bio(&self->ssl, &self->fd, bio_send, bio_recv, NULL);
   return self;
}

msocket_tls_client_t *msocket_tls_client_new(const msocket_tls_config_t *config)
{
   msocket_tls_client_t *self = (msocket_tls_client_t *)calloc(1, sizeof(msocket_tls_client_t));
   if (self == NULL) {
      return NULL;
   }

   mbedtls_ssl_config_init(&self->conf);
   mbedtls_x509_crt_init(&self->clicert);
   mbedtls_pk_init(&self->pkey);
   mbedtls_x509_crt_init(&self->cacert);
   mbedtls_entropy_init(&self->entropy);
   mbedtls_ctr_drbg_init(&self->ctr_drbg);

   const char *pers = "msocket_tls_client";
   int ret = mbedtls_ctr_drbg_seed(&self->ctr_drbg, mbedtls_entropy_func, &self->entropy,
                                   (const unsigned char *)pers, strlen(pers));
   if (ret != 0) {
      msocket_tls_client_delete(self);
      return NULL;
   }

   ret = mbedtls_ssl_config_defaults(&self->conf,
                                     MBEDTLS_SSL_IS_CLIENT,
                                     MBEDTLS_SSL_TRANSPORT_STREAM,
                                     MBEDTLS_SSL_PRESET_DEFAULT);
   if (ret != 0) {
      msocket_tls_client_delete(self);
      return NULL;
   }

   mbedtls_ssl_conf_rng(&self->conf, mbedtls_ctr_drbg_random, &self->ctr_drbg);

   if (config != NULL) {
      if (config->client_cert_path != NULL && config->client_key_path != NULL) {
         if (mbedtls_x509_crt_parse_file(&self->clicert, config->client_cert_path) == 0 &&
             mbedtls_pk_parse_keyfile(&self->pkey, config->client_key_path, NULL,
                                      mbedtls_ctr_drbg_random, &self->ctr_drbg) == 0) {
            self->has_cert = true;
            mbedtls_ssl_conf_own_cert(&self->conf, &self->clicert, &self->pkey);
         }
      }
      if (config->ca_cert_path != NULL) {
         if (mbedtls_x509_crt_parse_file(&self->cacert, config->ca_cert_path) == 0) {
            self->has_ca = true;
            mbedtls_ssl_conf_ca_chain(&self->conf, &self->cacert, NULL);
            mbedtls_ssl_conf_authmode(&self->conf, MBEDTLS_SSL_VERIFY_REQUIRED);
         }
      } else {
         mbedtls_ssl_conf_authmode(&self->conf, MBEDTLS_SSL_VERIFY_NONE);
      }
   } else {
      mbedtls_ssl_conf_authmode(&self->conf, MBEDTLS_SSL_VERIFY_NONE);
   }

   return self;
}

void msocket_tls_client_delete(msocket_tls_client_t *self)
{
   if (self != NULL) {
      mbedtls_ssl_config_free(&self->conf);
      mbedtls_x509_crt_free(&self->clicert);
      mbedtls_pk_free(&self->pkey);
      mbedtls_x509_crt_free(&self->cacert);
      mbedtls_ctr_drbg_free(&self->ctr_drbg);
      mbedtls_entropy_free(&self->entropy);
      free(self);
   }
}

msocket_tls_t *msocket_tls_client_attach(msocket_tls_client_t *cli, os_socket_t fd, const char *hostname)
{
   if (cli == NULL || OS_SOCKET_IS_INVALID(fd)) {
      return NULL;
   }

   msocket_tls_t *self = (msocket_tls_t *)calloc(1, sizeof(msocket_tls_t));
   if (self == NULL) {
      return NULL;
   }

   self->fd = fd;
   self->is_server = false;
   self->handshake_done = false;

   mbedtls_ssl_init(&self->ssl);
   int ret = mbedtls_ssl_setup(&self->ssl, &cli->conf);
   if (ret != 0) {
      mbedtls_ssl_free(&self->ssl);
      free(self);
      return NULL;
   }

   if (hostname != NULL && *hostname != '\0') {
      mbedtls_ssl_set_hostname(&self->ssl, hostname);
   }

   mbedtls_ssl_set_bio(&self->ssl, &self->fd, bio_send, bio_recv, NULL);
   self->client_ctx = cli;
   return self;
}

//////////////////////////////////////////////////////////////////////////////
// PUBLIC FUNCTIONS: CONNECTION I/O
//////////////////////////////////////////////////////////////////////////////

msocket_error_t msocket_tls_handshake(msocket_tls_t *self)
{
   if (self == NULL) {
      return MSOCKET_INVALID_ARGUMENT_ERROR;
   }
   if (self->handshake_done) {
      return MSOCKET_NO_ERROR;
   }
   int ret;
   while ((ret = mbedtls_ssl_handshake(&self->ssl)) != 0) {
      if (ret != MBEDTLS_ERR_SSL_WANT_READ && ret != MBEDTLS_ERR_SSL_WANT_WRITE) {
         return MSOCKET_TLS_HANDSHAKE_ERROR;
      }
   }
   self->handshake_done = true;
   return MSOCKET_NO_ERROR;
}

bool msocket_tls_has_pending(const msocket_tls_t *self)
{
   if (self != NULL && self->handshake_done) {
      return (mbedtls_ssl_get_bytes_avail(&self->ssl) > 0);
   }
   return false;
}

int msocket_tls_read(msocket_tls_t *self, uint8_t *buf, uint32_t len)
{
   if (self == NULL || buf == NULL) {
      return -1;
   }
   int ret;
   do {
      ret = mbedtls_ssl_read(&self->ssl, buf, len);
   } while (ret == MBEDTLS_ERR_SSL_WANT_READ || ret == MBEDTLS_ERR_SSL_WANT_WRITE);

   if (ret <= 0) {
      if (ret == 0 || ret == MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY) {
         return 0; // Peer closed connection
      }
      return -1;
   }
   return ret;
}

int msocket_tls_write(msocket_tls_t *self, const uint8_t *buf, uint32_t len)
{
   if (self == NULL || buf == NULL) {
      return -1;
   }
   int written = 0;
   while ((uint32_t)written < len) {
      int ret = mbedtls_ssl_write(&self->ssl, buf + written, len - (uint32_t)written);
      if (ret <= 0) {
         if (ret == MBEDTLS_ERR_SSL_WANT_READ || ret == MBEDTLS_ERR_SSL_WANT_WRITE) {
            continue;
         }
         return -1;
      }
      written += ret;
   }
   return written;
}

void msocket_tls_close(msocket_tls_t *self)
{
   if (self != NULL && self->handshake_done) {
      mbedtls_ssl_close_notify(&self->ssl);
   }
}

void msocket_tls_delete(msocket_tls_t *self)
{
   if (self != NULL) {
      mbedtls_ssl_free(&self->ssl);
      if (self->client_ctx != NULL) {
         msocket_tls_client_delete(self->client_ctx);
         self->client_ctx = NULL;
      }
      free(self);
   }
}

//////////////////////////////////////////////////////////////////////////////
// PRIVATE FUNCTIONS
//////////////////////////////////////////////////////////////////////////////

static int bio_send(void *ctx, const unsigned char *buf, size_t len)
{
   os_socket_t fd = *(os_socket_t *)ctx;
   int ret = (int)send(fd, (const char *)buf, (int)len, MSG_NOSIGNAL);
   if (ret < 0) {
      if (OS_SOCKET_ERRNO_IS_INTR()) {
         return MBEDTLS_ERR_SSL_WANT_WRITE;
      }
      return MBEDTLS_ERR_NET_SEND_FAILED;
   }
   return ret;
}

static int bio_recv(void *ctx, unsigned char *buf, size_t len)
{
   os_socket_t fd = *(os_socket_t *)ctx;
   int ret = (int)recv(fd, (char *)buf, (int)len, 0);
   if (ret < 0) {
      if (OS_SOCKET_ERRNO_IS_INTR()) {
         return MBEDTLS_ERR_SSL_WANT_READ;
      }
      return MBEDTLS_ERR_NET_RECV_FAILED;
   }
   if (ret == 0) {
      return MBEDTLS_ERR_SSL_PEER_CLOSE_NOTIFY;
   }
   return ret;
}

#endif /* MSOCKET_ENABLE_TLS */
