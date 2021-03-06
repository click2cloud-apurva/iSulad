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
 * Description: provide container kill definition
 ******************************************************************************/
#ifndef __CMD_KILL_H
#define __CMD_KILL_H

#include "arguments.h"
#include "wait.h"

#ifdef __cplusplus
extern "C" {
#endif

#define KILL_OPTIONS(cmdargs) \
    { CMD_OPT_TYPE_STRING, false, "signal", 's', &(cmdargs).signal, \
        "Signal to send to the container (default \"SIGKILL\")", NULL }

extern const char g_cmd_kill_desc[];
extern const char g_cmd_kill_usage[];
extern struct client_arguments g_cmd_kill_args;
int cmd_kill_main(int argc, const char **argv);

#ifdef __cplusplus
}
#endif

#endif

