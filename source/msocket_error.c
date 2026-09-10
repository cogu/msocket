/*****************************************************************************
* \file      msocket_error.c
* \author    Conny Gustafsson
* \date      2026-09-13
* \brief     msocket error code utilities
*
* Copyright (c) 2014-2026 Conny Gustafsson
* SPDX-License-Identifier: MIT
* See LICENSE in project root for full license terms.
******************************************************************************/
//////////////////////////////////////////////////////////////////////////////
// INCLUDES
//////////////////////////////////////////////////////////////////////////////
#include "msocket_error.h"

//////////////////////////////////////////////////////////////////////////////
// PUBLIC FUNCTIONS
//////////////////////////////////////////////////////////////////////////////
const char *msocket_error_str(msocket_error_t error_code)
{
   switch (error_code)
   {
   case MSOCKET_NO_ERROR:
      return "No error";
   case MSOCKET_GENERIC_ERROR:
      return "Generic error";
   case MSOCKET_INVALID_ARGUMENT_ERROR:
      return "Invalid argument";
   case MSOCKET_MEM_ERROR:
      return "Out of memory";
   case MSOCKET_SOCKET_ERROR:
      return "Socket error";
   case MSOCKET_NOT_IMPLEMENTED_ERROR:
      return "Not implemented";
   case MSOCKET_TIMEOUT_ERROR:
      return "Timeout";
   case MSOCKET_NOT_CONNECTED_ERROR:
      return "Not connected";
   default:
      return "Unknown error";
   }
}
