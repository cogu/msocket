/*****************************************************************************
* \file      TestMain.c
* \author    Conny Gustafsson
* \date      2026-09-10
* \brief     Unit test runner main
*
* Copyright (c) 2026 Conny Gustafsson
* SPDX-License-Identifier: MIT
* See LICENSE in project root for full license terms.
******************************************************************************/

#include <stdio.h>
#include "test_common.h"

void vfree(void *p)
{
   free(p);
}

CuSuite *testsuite_testsocket(void);
CuSuite *testsuite_msocket_common(void);
CuSuite *testsuite_msocket_loopback(void);
CuSuite *testsuite_msocket_server(void);
CuSuite *testsuite_cpp_socket(void);

void RunAllTests(void)
{
   CuString *output = CuStringNew();
   CuSuite *suite = CuSuiteNew();

   CuSuiteAddSuite(suite, testsuite_testsocket());
   CuSuiteAddSuite(suite, testsuite_msocket_common());
   CuSuiteAddSuite(suite, testsuite_msocket_loopback());
   CuSuiteAddSuite(suite, testsuite_msocket_server());
   CuSuiteAddSuite(suite, testsuite_cpp_socket());

   CuSuiteRun(suite);
   CuSuiteSummary(suite, output);
   CuSuiteDetails(suite, output);
   printf("%s\n", output->buffer);

   CuSuiteDelete(suite);
   CuStringDelete(output);
}

int main(void)
{
   RunAllTests();
   return 0;
}
