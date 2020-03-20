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
 * Description: provide grpc service definition
 ******************************************************************************/
#ifndef __GRPC_SERVICE_H
#define __GRPC_SERVICE_H

#include <stdbool.h>
#include "service_common.h"

#ifdef __cplusplus
extern "C" {
#endif

int grpc_server_init(const struct service_arguments *args);

void grpc_server_wait(void);

void grpc_server_shutdown(void);

#ifdef __cplusplus
}
#endif

#endif /* __GRPC_SERVICE_H */

