#ifndef __CHECKHOS_CLIENT_H__
#define __CHECKHOS_CLIENT_H__

#include "gtest/gtest.h"
#include "../src/hos_client.h"
#include "../src/hos_common.h"

void CheckStructHosConfigT(hos_config_t *actual, hos_config_t *expect);
void CheckStructHosFunc(hos_func_thread_t *actual, hos_func_thread_t *expect);
void CheckStructGHosHandle(hos_client_handle_t *actual, hos_client_handle_t *expect);
void CheckStructGHosFdContext(hos_fd_context_t *actual, hos_fd_context_t *expect);
void CheckHosInstance(hos_instance actual, hos_instance expect);

#endif