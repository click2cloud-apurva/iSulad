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
 * Description: provide container load definition
 ******************************************************************************/
#ifndef __CMD_LOAD_H
#define __CMD_LOAD_H

#include "arguments.h"

#ifdef __cplusplus
extern "C" {
#endif

#define LOAD_OPTIONS(cmdargs) \
    { CMD_OPT_TYPE_STRING, false, "input", 'i', &(cmdargs).file, "Read from a manifest or an archive", NULL }, \
    { CMD_OPT_TYPE_STRING, false, "tag", 0, &(cmdargs).tag, \
      "Name and optionally a tag in the 'name:tag' format, valid if type is docker", NULL }

#define EMBEDDED_OPTIONS(cmdargs) \
    { CMD_OPT_TYPE_STRING, false, "type", 't', &(cmdargs).type, "Image type, embedded or docker(default)", NULL }

extern const char g_cmd_load_desc[];
extern struct client_arguments g_cmd_load_args;
int cmd_load_main(int argc, const char **argv);

#ifdef __cplusplus
}
#endif

#endif /* __CMD_LOAD_H */

