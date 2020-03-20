/******************************************************************************
 * Copyright (c) Huawei Technologies Co., Ltd. 2018-2019. All rights reserved.
 * iSulad licensed under the Mulan PSL v1.
 * You can use this software according to the terms and conditions of the Mulan PSL v1.
 * You may obtain a copy of Mulan PSL v1 at:
 *     http://license.coscl.org.cn/MulanPSL
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR
 * PURPOSE.
 * See the Mulan PSL v1 for more details.
 * Author: maoweiyong
 * Create: 2018-11-08
 * Description: provide common service definition
 ******************************************************************************/
#ifndef __SERVICE_COMMON_H
#define __SERVICE_COMMON_H

#include "arguments.h"
#include "libisulad.h"

#ifdef __cplusplus
extern "C" {
#endif

int server_common_init(const struct service_arguments *args);

void server_common_start(void);

void server_common_shutdown(void);

void event_monitor_exit_callback(void *arg);

#ifdef __cplusplus
}
#endif

#endif

