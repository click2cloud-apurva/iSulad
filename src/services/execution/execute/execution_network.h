/******************************************************************************
 * Copyright (c) Huawei Technologies Co., Ltd. 2017-2019. All rights reserved.
 * iSulad licensed under the Mulan PSL v1.
 * You can use this software according to the terms and conditions of the Mulan PSL v1.
 * You may obtain a copy of Mulan PSL v1 at:
 *     http://license.coscl.org.cn/MulanPSL
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR
 * PURPOSE.
 * See the Mulan PSL v1 for more details.
 * Author: tanyifeng
 * Create: 2017-11-22
 * Description: provide container list callback function definition
 *******************************************************************************/

#ifndef __EXECUTION_NET_WORK_H_
#define __EXECUTION_NET_WORK_H_

#include "callback.h"
#include "container_unix.h"

#ifdef __cplusplus
extern "C" {
#endif

int merge_network(const host_config *host_spec, const char *rootfs, const char *runtime_root,
                  const char *id, const char *hostname);

int container_initialize_networking(const container_t *cont);

int init_container_network_confs(const char *id, const char *rootpath, const host_config *hc,
                                 container_config_v2_common_config *common_config);

#ifdef __cplusplus
}
#endif

#endif

