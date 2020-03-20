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
 * Author: tanyifeng
 * Create: 2018-11-08
 * Description: provide container callback functions
 ******************************************************************************/
#include "callback.h"

#include "image_cb.h"
#include "execution.h"

service_callback_t g_isulad_servicecallback;

/* service callback */
int service_callback_init(void)
{
    container_callback_init(&g_isulad_servicecallback.container);
    image_callback_init(&g_isulad_servicecallback.image);
    return 0;
}

/* get service callback */
service_callback_t *get_service_callback(void)
{
    return &g_isulad_servicecallback;
}

