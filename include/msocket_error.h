/*****************************************************************************
* \file      msocket_error.h
* \author    Conny Gustafsson
* \date      2026-09-13
* \brief     msocket error codes
*
* Copyright (c) 2014-2026 Conny Gustafsson
* SPDX-License-Identifier: MIT
* See LICENSE in project root for full license terms.
******************************************************************************/
#ifndef MSOCKET_ERROR_H
#define MSOCKET_ERROR_H

//////////////////////////////////////////////////////////////////////////////
// INCLUDES
//////////////////////////////////////////////////////////////////////////////
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

//////////////////////////////////////////////////////////////////////////////
// PUBLIC CONSTANTS AND DATA TYPES
//////////////////////////////////////////////////////////////////////////////
#define MSOCKET_GENERIC_ERROR          -1
#define MSOCKET_NO_ERROR                0
#define MSOCKET_INVALID_ARGUMENT_ERROR  1
#define MSOCKET_MEM_ERROR               2
#define MSOCKET_SOCKET_ERROR            3
#define MSOCKET_NOT_IMPLEMENTED_ERROR   4
#define MSOCKET_TIMEOUT_ERROR           5
#define MSOCKET_NOT_CONNECTED_ERROR     6

typedef int8_t msocket_error_t;

//////////////////////////////////////////////////////////////////////////////
// PUBLIC FUNCTION PROTOTYPES
//////////////////////////////////////////////////////////////////////////////
const char *msocket_error_str(msocket_error_t error_code);

#ifdef __cplusplus
}
#endif

#endif // MSOCKET_ERROR_H
