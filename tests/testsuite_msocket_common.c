/*****************************************************************************
* \file      testsuite_msocket_common.c
* \author    Conny Gustafsson
* \date      2026-09-10
* \brief     Unit tests for msocket_common buffer framing, state, and timeouts
*
* Copyright (c) 2026 Conny Gustafsson
* SPDX-License-Identifier: MIT
* See LICENSE in project root for full license terms.
******************************************************************************/

#include "test_common.h"
#include "msocket.h"
#include "msocket_internal.h"
#include <string.h>

static int g_msg_count = 0;
static uint32_t g_total_bytes = 0;
static uint32_t g_inactivity_elapsed = 0;

static msocket_error_t fixed_msg_parser(void *arg, const uint8_t *data, const uint32_t num_bytes, uint32_t *consumed_bytes, uint32_t *msg_size_hint)
{
   (void)arg;
   (void)data;
   (void)msg_size_hint;
   /* Consumes in fixed 4-byte frames */
   if (num_bytes >= 4u) {
      *consumed_bytes = 4u;
      g_msg_count++;
      g_total_bytes += 4u;
      return MSOCKET_NO_ERROR;
   }
   *consumed_bytes = 0u;
   return MSOCKET_NO_ERROR;
}

static void on_inactivity(const uint32_t elapsed_ms)
{
   g_inactivity_elapsed = elapsed_ms;
}

static void test_msocket_lifecycle(CuTest *tc)
{
   msocket_t *sock = msocket_new(MSOCKET_ADDR_INET);
   CuAssertPtrNotNull(tc, sock);
   CuAssertIntEquals(tc, MSOCKET_STATE_NONE, msocket_state(sock));
   msocket_delete(sock);

   msocket_t stack_sock;
   msocket_error_t rc = msocket_create(&stack_sock, MSOCKET_ADDR_UNIX);
   CuAssertIntEquals(tc, MSOCKET_NO_ERROR, rc);
   CuAssertIntEquals(tc, MSOCKET_STATE_NONE, msocket_state(&stack_sock));
   msocket_destroy(&stack_sock);
}

static void test_msocket_framing(CuTest *tc)
{
   msocket_t *sock = msocket_new(MSOCKET_ADDR_INET);
   CuAssertPtrNotNull(tc, sock);

   msocket_handler_t handler;
   memset(&handler, 0, sizeof(handler));
   handler.stream_data = fixed_msg_parser;
   msocket_set_handler(sock, &handler, NULL);

   g_msg_count = 0;
   g_total_bytes = 0;

   /* Append 2 bytes -> not enough for 4-byte frame */
   const uint8_t chunk1[] = {1, 2};
   msocket_error_t rc = msocket_common_on_data(sock, chunk1, sizeof(chunk1));
   CuAssertIntEquals(tc, MSOCKET_NO_ERROR, rc);
   CuAssertIntEquals(tc, 0, g_msg_count);
   CuAssertIntEquals(tc, 2, (int)adt_streambuffer_size(&sock->stream_rx_buf));

   /* Append 6 bytes -> total 8 bytes -> should parse exactly 2 frames */
   const uint8_t chunk2[] = {3, 4, 5, 6, 7, 8};
   rc = msocket_common_on_data(sock, chunk2, sizeof(chunk2));
   CuAssertIntEquals(tc, MSOCKET_NO_ERROR, rc);
   CuAssertIntEquals(tc, 2, g_msg_count);
   CuAssertIntEquals(tc, 8, (int)g_total_bytes);
   CuAssertIntEquals(tc, 0, (int)adt_streambuffer_size(&sock->stream_rx_buf));

   /* Append 5 bytes -> should parse 1 frame, 1 byte remains in rx buffer */
   const uint8_t chunk3[] = {1, 2, 3, 4, 5};
   rc = msocket_common_on_data(sock, chunk3, sizeof(chunk3));
   CuAssertIntEquals(tc, MSOCKET_NO_ERROR, rc);
   CuAssertIntEquals(tc, 3, g_msg_count);
   CuAssertIntEquals(tc, 1, (int)adt_streambuffer_size(&sock->stream_rx_buf));

   msocket_delete(sock);
}

static void test_msocket_inactivity(CuTest *tc)
{
   msocket_t *sock = msocket_new(MSOCKET_ADDR_INET);
   CuAssertPtrNotNull(tc, sock);

   msocket_handler_t handler;
   memset(&handler, 0, sizeof(handler));
   handler.stream_inactivity = on_inactivity;
   msocket_set_handler(sock, &handler, NULL);

   sock->state = MSOCKET_STATE_ESTABLISHED;
   g_inactivity_elapsed = 0;

   /* 19 steps of 50ms = 950ms -> should not trigger yet */
   for (int i = 0; i < 19; i++) {
      msocket_common_on_timeout(sock);
   }
   CuAssertIntEquals(tc, 0, (int)g_inactivity_elapsed);

   /* 20th step = 1000ms -> should trigger callback */
   msocket_common_on_timeout(sock);
   CuAssertIntEquals(tc, 1000, (int)g_inactivity_elapsed);

   msocket_delete(sock);
}

static int g_hint_msg_count = 0;
static uint32_t g_hint_last_msg_len = 0;

static msocket_error_t hint_msg_parser(void *arg, const uint8_t *data, const uint32_t num_bytes, uint32_t *consumed_bytes, uint32_t *msg_size_hint)
{
   (void)arg;
   if (num_bytes < 4u) {
      /* Scenario 3: not enough bytes even for header, no size hint */
      *consumed_bytes = 0u;
      *msg_size_hint = 0u;
      return MSOCKET_NO_ERROR;
   }
   /* Read 32-bit big-endian payload length */
   uint32_t payload_len = ((uint32_t)data[0] << 24) |
                          ((uint32_t)data[1] << 16) |
                          ((uint32_t)data[2] << 8)  |
                          ((uint32_t)data[3]);
   uint32_t total_frame_len = 4u + payload_len;
   if (num_bytes < total_frame_len) {
      /* Scenario 2: incomplete payload, hint total required frame size */
      *consumed_bytes = 0u;
      *msg_size_hint = total_frame_len;
      return MSOCKET_NO_ERROR;
   }
   /* Scenario 1: complete frame */
   *consumed_bytes = total_frame_len;
   g_hint_msg_count++;
   g_hint_last_msg_len = total_frame_len;
   return MSOCKET_NO_ERROR;
}

static void test_msocket_framing_msg_size_hint(CuTest *tc)
{
   msocket_t *sock = msocket_new(MSOCKET_ADDR_INET);
   CuAssertPtrNotNull(tc, sock);

   msocket_handler_t handler;
   memset(&handler, 0, sizeof(handler));
   handler.stream_data = hint_msg_parser;
   msocket_set_handler(sock, &handler, NULL);

   g_hint_msg_count = 0;
   g_hint_last_msg_len = 0;

   /* Step 1 (Scenario 3): Ingest 2 bytes (incomplete header).
    * Callback sets consumed_bytes=0, msg_size_hint=0. */
   const uint8_t partial_header[] = {0x00, 0x01};
   msocket_error_t rc = msocket_common_on_data(sock, partial_header, sizeof(partial_header));
   CuAssertIntEquals(tc, MSOCKET_NO_ERROR, rc);
   CuAssertIntEquals(tc, 0, g_hint_msg_count);
   CuAssertIntEquals(tc, 2, (int)adt_streambuffer_size(&sock->stream_rx_buf));

   /* Step 2 (Scenario 2): Complete the 4-byte header for a 64KB (65536 bytes) payload.
    * Remaining 2 header bytes: 0x00, 0x00 -> payload length = 0x00010000 = 65536 bytes.
    * Total frame = 65540 bytes.
    * Callback sets consumed_bytes=0, msg_size_hint=65540.
    * msocket should invoke adt_streambuffer_reserve(65540). */
   const uint8_t remaining_header[] = {0x00, 0x00};
   rc = msocket_common_on_data(sock, remaining_header, sizeof(remaining_header));
   CuAssertIntEquals(tc, MSOCKET_NO_ERROR, rc);
   CuAssertIntEquals(tc, 0, g_hint_msg_count);
   CuAssertIntEquals(tc, 4, (int)adt_streambuffer_size(&sock->stream_rx_buf));
   CuAssertTrue(tc, adt_streambuffer_allocated_bytes(&sock->stream_rx_buf) >= 65540u);

   /* Step 3 (Scenario 1): Ingest the 65536 payload bytes.
    * Now all 65540 bytes are present contiguously.
    * Callback parses the full frame, consumed_bytes=65540.
    * Buffer should be fully drained! */
   uint8_t *payload = (uint8_t *)calloc(1, 65536);
   CuAssertPtrNotNull(tc, payload);
   rc = msocket_common_on_data(sock, payload, 65536);
   CuAssertIntEquals(tc, MSOCKET_NO_ERROR, rc);
   CuAssertIntEquals(tc, 1, g_hint_msg_count);
   CuAssertIntEquals(tc, 65540, (int)g_hint_last_msg_len);
   CuAssertIntEquals(tc, 0, (int)adt_streambuffer_size(&sock->stream_rx_buf));
   free(payload);

   msocket_delete(sock);
}

static void test_parse_endpoint_file(CuTest *tc)
{
   adt_str_t *address = NULL;
   uint16_t port = 12345;

   CuAssertIntEquals(tc, MSOCKET_ENDPOINT_FILE, msocket_parse_endpoint("./test.socket", &address, &port));
   CuAssertPtrNotNull(tc, address);
   CuAssertStrEquals(tc, "./test.socket", adt_str_cstr(address));
   CuAssertIntEquals(tc, 0, (int)port);
   adt_str_delete(address);
   address = NULL;

   CuAssertIntEquals(tc, MSOCKET_ENDPOINT_FILE, msocket_parse_endpoint("sockets/test", &address, &port));
   CuAssertPtrNotNull(tc, address);
   CuAssertStrEquals(tc, "sockets/test", adt_str_cstr(address));
   CuAssertIntEquals(tc, 0, (int)port);
   adt_str_delete(address);
   address = NULL;

   CuAssertIntEquals(tc, MSOCKET_ENDPOINT_FILE, msocket_parse_endpoint("/tmp/apx_server.socket", &address, &port));
   CuAssertPtrNotNull(tc, address);
   CuAssertStrEquals(tc, "/tmp/apx_server.socket", adt_str_cstr(address));
   CuAssertIntEquals(tc, 0, (int)port);
   adt_str_delete(address);
   address = NULL;

   CuAssertIntEquals(tc, MSOCKET_ENDPOINT_FILE, msocket_parse_endpoint("test\\socket", &address, &port));
   CuAssertPtrNotNull(tc, address);
   CuAssertStrEquals(tc, "test\\socket", adt_str_cstr(address));
   CuAssertIntEquals(tc, 0, (int)port);
   adt_str_delete(address);
   address = NULL;

   CuAssertIntEquals(tc, MSOCKET_ENDPOINT_FILE, msocket_parse_endpoint("C:\\path\\to\\socket", &address, &port));
   CuAssertPtrNotNull(tc, address);
   CuAssertStrEquals(tc, "C:\\path\\to\\socket", adt_str_cstr(address));
   CuAssertIntEquals(tc, 0, (int)port);
   adt_str_delete(address);
}

static void test_parse_endpoint_ipv4_without_port(CuTest *tc)
{
   adt_str_t *address = NULL;
   uint16_t port = 999;

   CuAssertIntEquals(tc, MSOCKET_ENDPOINT_IPV4, msocket_parse_endpoint("127.0.0.1", &address, &port));
   CuAssertPtrNotNull(tc, address);
   CuAssertStrEquals(tc, "127.0.0.1", adt_str_cstr(address));
   CuAssertIntEquals(tc, 0, (int)port);
   adt_str_delete(address);
   address = NULL;

   CuAssertIntEquals(tc, MSOCKET_ENDPOINT_IPV4, msocket_parse_endpoint("192.168.1.1", &address, &port));
   CuAssertPtrNotNull(tc, address);
   CuAssertStrEquals(tc, "192.168.1.1", adt_str_cstr(address));
   CuAssertIntEquals(tc, 0, (int)port);
   adt_str_delete(address);
   address = NULL;

   CuAssertIntEquals(tc, MSOCKET_ENDPOINT_IPV4, msocket_parse_endpoint("10.0.0.1", &address, &port));
   CuAssertPtrNotNull(tc, address);
   CuAssertStrEquals(tc, "10.0.0.1", adt_str_cstr(address));
   CuAssertIntEquals(tc, 0, (int)port);
   adt_str_delete(address);
}

static void test_parse_endpoint_ipv4_with_port(CuTest *tc)
{
   adt_str_t *address = NULL;
   uint16_t port = 0;

   CuAssertIntEquals(tc, MSOCKET_ENDPOINT_IPV4, msocket_parse_endpoint("127.0.0.1:8080", &address, &port));
   CuAssertPtrNotNull(tc, address);
   CuAssertStrEquals(tc, "127.0.0.1", adt_str_cstr(address));
   CuAssertIntEquals(tc, 8080, (int)port);
   adt_str_delete(address);
   address = NULL;

   CuAssertIntEquals(tc, MSOCKET_ENDPOINT_IPV4, msocket_parse_endpoint("192.168.1.1:5000", &address, &port));
   CuAssertPtrNotNull(tc, address);
   CuAssertStrEquals(tc, "192.168.1.1", adt_str_cstr(address));
   CuAssertIntEquals(tc, 5000, (int)port);
   adt_str_delete(address);
   address = NULL;

   CuAssertIntEquals(tc, MSOCKET_ENDPOINT_IPV4, msocket_parse_endpoint("10.0.0.1:65535", &address, &port));
   CuAssertPtrNotNull(tc, address);
   CuAssertStrEquals(tc, "10.0.0.1", adt_str_cstr(address));
   CuAssertIntEquals(tc, 65535, (int)port);
   adt_str_delete(address);
   address = NULL;

   CuAssertIntEquals(tc, MSOCKET_ENDPOINT_IPV4, msocket_parse_endpoint("127.0.0.1:0", &address, &port));
   CuAssertPtrNotNull(tc, address);
   CuAssertStrEquals(tc, "127.0.0.1", adt_str_cstr(address));
   CuAssertIntEquals(tc, 0, (int)port);
   adt_str_delete(address);
}

static void test_parse_endpoint_ipv4_invalid(CuTest *tc)
{
   adt_str_t *address = NULL;
   uint16_t port = 1234;

   CuAssertIntEquals(tc, MSOCKET_ENDPOINT_ERROR, msocket_parse_endpoint("381.1.1.0", &address, &port));
   CuAssertPtrEquals(tc, NULL, address);
   CuAssertIntEquals(tc, 0, (int)port);

   CuAssertIntEquals(tc, MSOCKET_ENDPOINT_ERROR, msocket_parse_endpoint("127.0.01", &address, &port));
   CuAssertPtrEquals(tc, NULL, address);
   CuAssertIntEquals(tc, 0, (int)port);

   CuAssertIntEquals(tc, MSOCKET_ENDPOINT_ERROR, msocket_parse_endpoint("192.168..1.1", &address, &port));
   CuAssertPtrEquals(tc, NULL, address);
   CuAssertIntEquals(tc, 0, (int)port);

   CuAssertIntEquals(tc, MSOCKET_ENDPOINT_ERROR, msocket_parse_endpoint("127.0.0.1:70000", &address, &port));
   CuAssertPtrEquals(tc, NULL, address);
   CuAssertIntEquals(tc, 0, (int)port);

   CuAssertIntEquals(tc, MSOCKET_ENDPOINT_ERROR, msocket_parse_endpoint("127.0.0.1:abc", &address, &port));
   CuAssertPtrEquals(tc, NULL, address);
   CuAssertIntEquals(tc, 0, (int)port);

   CuAssertIntEquals(tc, MSOCKET_ENDPOINT_ERROR, msocket_parse_endpoint("127.0.0.1:", &address, &port));
   CuAssertPtrEquals(tc, NULL, address);
   CuAssertIntEquals(tc, 0, (int)port);
}

static void test_parse_endpoint_ipv6_bracketed(CuTest *tc)
{
   adt_str_t *address = NULL;
   uint16_t port = 0;

   CuAssertIntEquals(tc, MSOCKET_ENDPOINT_IPV6, msocket_parse_endpoint("[::1]:5000", &address, &port));
   CuAssertPtrNotNull(tc, address);
   CuAssertStrEquals(tc, "::1", adt_str_cstr(address));
   CuAssertIntEquals(tc, 5000, (int)port);
   adt_str_delete(address);
   address = NULL;

   CuAssertIntEquals(tc, MSOCKET_ENDPOINT_IPV6, msocket_parse_endpoint("[2001:db8::1]:8080", &address, &port));
   CuAssertPtrNotNull(tc, address);
   CuAssertStrEquals(tc, "2001:db8::1", adt_str_cstr(address));
   CuAssertIntEquals(tc, 8080, (int)port);
   adt_str_delete(address);
   address = NULL;

   CuAssertIntEquals(tc, MSOCKET_ENDPOINT_IPV6, msocket_parse_endpoint("[::1]", &address, &port));
   CuAssertPtrNotNull(tc, address);
   CuAssertStrEquals(tc, "::1", adt_str_cstr(address));
   CuAssertIntEquals(tc, 0, (int)port);
   adt_str_delete(address);
   address = NULL;

   CuAssertIntEquals(tc, MSOCKET_ENDPOINT_IPV6, msocket_parse_endpoint("[fe80::1ff:fe23:4567:890a]:1234", &address, &port));
   CuAssertPtrNotNull(tc, address);
   CuAssertStrEquals(tc, "fe80::1ff:fe23:4567:890a", adt_str_cstr(address));
   CuAssertIntEquals(tc, 1234, (int)port);
   adt_str_delete(address);
}

static void test_parse_endpoint_ipv6_bare(CuTest *tc)
{
   adt_str_t *address = NULL;
   uint16_t port = 999;

   CuAssertIntEquals(tc, MSOCKET_ENDPOINT_IPV6, msocket_parse_endpoint("::1", &address, &port));
   CuAssertPtrNotNull(tc, address);
   CuAssertStrEquals(tc, "::1", adt_str_cstr(address));
   CuAssertIntEquals(tc, 0, (int)port);
   adt_str_delete(address);
   address = NULL;

   CuAssertIntEquals(tc, MSOCKET_ENDPOINT_IPV6, msocket_parse_endpoint("::", &address, &port));
   CuAssertPtrNotNull(tc, address);
   CuAssertStrEquals(tc, "::", adt_str_cstr(address));
   CuAssertIntEquals(tc, 0, (int)port);
   adt_str_delete(address);
   address = NULL;

   CuAssertIntEquals(tc, MSOCKET_ENDPOINT_IPV6, msocket_parse_endpoint("2001:db8::1", &address, &port));
   CuAssertPtrNotNull(tc, address);
   CuAssertStrEquals(tc, "2001:db8::1", adt_str_cstr(address));
   CuAssertIntEquals(tc, 0, (int)port);
   adt_str_delete(address);
   address = NULL;

   CuAssertIntEquals(tc, MSOCKET_ENDPOINT_IPV6, msocket_parse_endpoint("fe80::1ff:fe23:4567:890a", &address, &port));
   CuAssertPtrNotNull(tc, address);
   CuAssertStrEquals(tc, "fe80::1ff:fe23:4567:890a", adt_str_cstr(address));
   CuAssertIntEquals(tc, 0, (int)port);
   adt_str_delete(address);
}

static void test_parse_endpoint_ipv6_invalid(CuTest *tc)
{
   adt_str_t *address = NULL;
   uint16_t port = 1234;

   CuAssertIntEquals(tc, MSOCKET_ENDPOINT_ERROR, msocket_parse_endpoint("[:::1]:5000", &address, &port));
   CuAssertPtrEquals(tc, NULL, address);
   CuAssertIntEquals(tc, 0, (int)port);

   CuAssertIntEquals(tc, MSOCKET_ENDPOINT_ERROR, msocket_parse_endpoint("[::1]:99999", &address, &port));
   CuAssertPtrEquals(tc, NULL, address);
   CuAssertIntEquals(tc, 0, (int)port);

   CuAssertIntEquals(tc, MSOCKET_ENDPOINT_ERROR, msocket_parse_endpoint("[::1", &address, &port));
   CuAssertPtrEquals(tc, NULL, address);
   CuAssertIntEquals(tc, 0, (int)port);

   CuAssertIntEquals(tc, MSOCKET_ENDPOINT_ERROR, msocket_parse_endpoint("[::1]:", &address, &port));
   CuAssertPtrEquals(tc, NULL, address);
   CuAssertIntEquals(tc, 0, (int)port);

   CuAssertIntEquals(tc, MSOCKET_ENDPOINT_ERROR, msocket_parse_endpoint("[::1]extra", &address, &port));
   CuAssertPtrEquals(tc, NULL, address);
   CuAssertIntEquals(tc, 0, (int)port);

   CuAssertIntEquals(tc, MSOCKET_ENDPOINT_ERROR, msocket_parse_endpoint("[]", &address, &port));
   CuAssertPtrEquals(tc, NULL, address);
   CuAssertIntEquals(tc, 0, (int)port);

   CuAssertIntEquals(tc, MSOCKET_ENDPOINT_ERROR, msocket_parse_endpoint("1:2:3:4:5:6:7:8:9", &address, &port));
   CuAssertPtrEquals(tc, NULL, address);
   CuAssertIntEquals(tc, 0, (int)port);

   CuAssertIntEquals(tc, MSOCKET_ENDPOINT_ERROR, msocket_parse_endpoint("foo:bar:baz", &address, &port));
   CuAssertPtrEquals(tc, NULL, address);
   CuAssertIntEquals(tc, 0, (int)port);
}

static void test_parse_endpoint_name(CuTest *tc)
{
   adt_str_t *address = NULL;
   uint16_t port = 0;

   CuAssertIntEquals(tc, MSOCKET_ENDPOINT_NAME, msocket_parse_endpoint("localhost", &address, &port));
   CuAssertPtrNotNull(tc, address);
   CuAssertStrEquals(tc, "localhost", adt_str_cstr(address));
   CuAssertIntEquals(tc, 0, (int)port);
   adt_str_delete(address);
   address = NULL;

   CuAssertIntEquals(tc, MSOCKET_ENDPOINT_NAME, msocket_parse_endpoint("localhost:5000", &address, &port));
   CuAssertPtrNotNull(tc, address);
   CuAssertStrEquals(tc, "localhost", adt_str_cstr(address));
   CuAssertIntEquals(tc, 5000, (int)port);
   adt_str_delete(address);
   address = NULL;

   CuAssertIntEquals(tc, MSOCKET_ENDPOINT_NAME, msocket_parse_endpoint("my-server.local:80", &address, &port));
   CuAssertPtrNotNull(tc, address);
   CuAssertStrEquals(tc, "my-server.local", adt_str_cstr(address));
   CuAssertIntEquals(tc, 80, (int)port);
   adt_str_delete(address);
   address = NULL;

   CuAssertIntEquals(tc, MSOCKET_ENDPOINT_NAME, msocket_parse_endpoint(":5000", &address, &port));
   CuAssertPtrNotNull(tc, address);
   CuAssertStrEquals(tc, "", adt_str_cstr(address));
   CuAssertIntEquals(tc, 5000, (int)port);
   adt_str_delete(address);
   address = NULL;

   CuAssertIntEquals(tc, MSOCKET_ENDPOINT_NAME, msocket_parse_endpoint(":8080", &address, &port));
   CuAssertPtrNotNull(tc, address);
   CuAssertStrEquals(tc, "", adt_str_cstr(address));
   CuAssertIntEquals(tc, 8080, (int)port);
   adt_str_delete(address);
   address = NULL;

   CuAssertIntEquals(tc, MSOCKET_ENDPOINT_ERROR, msocket_parse_endpoint("localhost:70000", &address, &port));
   CuAssertPtrEquals(tc, NULL, address);
   CuAssertIntEquals(tc, 0, (int)port);

   CuAssertIntEquals(tc, MSOCKET_ENDPOINT_ERROR, msocket_parse_endpoint("localhost:abc", &address, &port));
   CuAssertPtrEquals(tc, NULL, address);
   CuAssertIntEquals(tc, 0, (int)port);

   CuAssertIntEquals(tc, MSOCKET_ENDPOINT_ERROR, msocket_parse_endpoint("localhost:", &address, &port));
   CuAssertPtrEquals(tc, NULL, address);
   CuAssertIntEquals(tc, 0, (int)port);

   CuAssertIntEquals(tc, MSOCKET_ENDPOINT_ERROR, msocket_parse_endpoint(":", &address, &port));
   CuAssertPtrEquals(tc, NULL, address);
   CuAssertIntEquals(tc, 0, (int)port);
}

static void test_parse_endpoint_null_handling(CuTest *tc)
{
   adt_str_t *address = NULL;
   uint16_t port = 1234;

   CuAssertIntEquals(tc, MSOCKET_ENDPOINT_ERROR, msocket_parse_endpoint(NULL, &address, &port));
   CuAssertPtrEquals(tc, NULL, address);
   CuAssertIntEquals(tc, 0, (int)port);

   CuAssertIntEquals(tc, MSOCKET_ENDPOINT_ERROR, msocket_parse_endpoint("localhost:5000", NULL, &port));
   CuAssertIntEquals(tc, 0, (int)port);

   CuAssertIntEquals(tc, MSOCKET_ENDPOINT_NAME, msocket_parse_endpoint("localhost:5000", &address, NULL));
   CuAssertPtrNotNull(tc, address);
   CuAssertStrEquals(tc, "localhost", adt_str_cstr(address));
   adt_str_delete(address);
   address = NULL;

   CuAssertIntEquals(tc, MSOCKET_ENDPOINT_ERROR, msocket_parse_endpoint("", &address, &port));
   CuAssertPtrEquals(tc, NULL, address);
   CuAssertIntEquals(tc, 0, (int)port);
}

static void test_msocket_error_str(CuTest *tc)
{
   CuAssertStrEquals(tc, "No error", msocket_error_str(MSOCKET_NO_ERROR));
   CuAssertStrEquals(tc, "Generic error", msocket_error_str(MSOCKET_GENERIC_ERROR));
   CuAssertStrEquals(tc, "Invalid argument", msocket_error_str(MSOCKET_INVALID_ARGUMENT_ERROR));
   CuAssertStrEquals(tc, "Out of memory", msocket_error_str(MSOCKET_MEM_ERROR));
   CuAssertStrEquals(tc, "Socket error", msocket_error_str(MSOCKET_SOCKET_ERROR));
   CuAssertStrEquals(tc, "Not implemented", msocket_error_str(MSOCKET_NOT_IMPLEMENTED_ERROR));
   CuAssertStrEquals(tc, "Timeout", msocket_error_str(MSOCKET_TIMEOUT_ERROR));
   CuAssertStrEquals(tc, "Not connected", msocket_error_str(MSOCKET_NOT_CONNECTED_ERROR));
   CuAssertStrEquals(tc, "Unknown error", msocket_error_str((msocket_error_t)99));
}

CuSuite *testsuite_msocket_common(void)
{
   CuSuite *suite = CuSuiteNew();
   SUITE_ADD_TEST(suite, test_msocket_error_str);
   SUITE_ADD_TEST(suite, test_msocket_lifecycle);
   SUITE_ADD_TEST(suite, test_msocket_framing);
   SUITE_ADD_TEST(suite, test_msocket_framing_msg_size_hint);
   SUITE_ADD_TEST(suite, test_msocket_inactivity);
   SUITE_ADD_TEST(suite, test_parse_endpoint_file);
   SUITE_ADD_TEST(suite, test_parse_endpoint_ipv4_without_port);
   SUITE_ADD_TEST(suite, test_parse_endpoint_ipv4_with_port);
   SUITE_ADD_TEST(suite, test_parse_endpoint_ipv4_invalid);
   SUITE_ADD_TEST(suite, test_parse_endpoint_ipv6_bracketed);
   SUITE_ADD_TEST(suite, test_parse_endpoint_ipv6_bare);
   SUITE_ADD_TEST(suite, test_parse_endpoint_ipv6_invalid);
   SUITE_ADD_TEST(suite, test_parse_endpoint_name);
   SUITE_ADD_TEST(suite, test_parse_endpoint_null_handling);
   return suite;
}
