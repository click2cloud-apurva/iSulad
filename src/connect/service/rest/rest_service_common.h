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
 * Author: lifeng
 * Create: 2018-11-08
 * Description: provide container restful service common definition
 ******************************************************************************/
#ifndef __REST_SERVICE_COMMON_H
#define __REST_SERVICE_COMMON_H

#include <evhtp.h>

#include "rest_common.h"

#ifdef __cplusplus
extern "C" {
#endif

int get_body(const evhtp_request_t *req, size_t *size_out, char **record_out);

void put_body(char *body);

void evhtp_send_response(evhtp_request_t *req, const char *responsedata, int rescode);

#ifdef __cplusplus
}
#endif

#endif

