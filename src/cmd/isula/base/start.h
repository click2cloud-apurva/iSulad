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
 * Description: provide container start  definition
 ******************************************************************************/
#ifndef __CMD_START_H
#define __CMD_START_H

#include "arguments.h"
#include "commands.h"
#include <termios.h>

#ifdef __cplusplus
extern "C" {
#endif

extern const char g_cmd_start_desc[];
extern struct client_arguments g_cmd_start_args;

void client_wait_fifo_exit(const struct client_arguments *args);
void client_restore_console(bool reset_tty, const struct termios *oldtios, struct command_fifo_config *console_fifos);

int client_start(const struct client_arguments *args, bool *reset_tty, struct termios *oldtios,
                 struct command_fifo_config **console_fifos);
int cmd_start_main(int argc, const char **argv);

#ifdef __cplusplus
}
#endif

#endif /* __CMD_START_H */

