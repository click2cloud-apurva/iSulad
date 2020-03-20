/******************************************************************************
* Copyright (c) Huawei Technologies Co., Ltd. 2019. All rights reserved.
 * iSulad licensed under the Mulan PSL v1.
 * You can use this software according to the terms and conditions of the Mulan PSL v1.
 * You may obtain a copy of Mulan PSL v1 at:
 *     http://license.coscl.org.cn/MulanPSL
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR
 * PURPOSE.
 * See the Mulan PSL v1 for more details.
* Author: liuhao
* Create: 2019-07-15
* Description: helper functions for isula image
*******************************************************************************/
#ifndef __IMAGE_ISULA_HELPER_H
#define __IMAGE_ISULA_HELPER_H

#include "connect.h"

#ifdef __cplusplus
extern "C" {
#endif

int get_isula_image_connect_config(client_connect_config_t *conf);

void free_client_connect_config_value(client_connect_config_t *conf);

#ifdef __cplusplus
}
#endif

#endif
