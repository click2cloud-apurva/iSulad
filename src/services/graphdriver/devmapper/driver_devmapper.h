/******************************************************************************
* Copyright (c) Huawei Technologies Co., Ltd. 2020. All rights reserved.
* iSulad licensed under the Mulan PSL v1.
* You can use this software according to the terms and conditions of the Mulan PSL v1.
* You may obtain a copy of Mulan PSL v1 at:
*     http://license.coscl.org.cn/MulanPSL
* THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR
* PURPOSE.
* See the Mulan PSL v1 for more details.
* Author: wangfengtu
* Create: 2020-01-19
* Description: provide devicemapper graphdriver function definition
******************************************************************************/
#ifndef __GRAPHDRIVER_DEVMAPPER_H
#define __GRAPHDRIVER_DEVMAPPER_H

#include "driver.h"

#ifdef __cplusplus
extern "C" {
#endif

int devmapper_init(struct graphdriver *driver);

int devmapper_parse_options(struct graphdriver *driver, const char **options, size_t options_len);

bool devmapper_is_quota_options(struct graphdriver *driver, const char *option);

#ifdef __cplusplus
}
#endif

#endif
