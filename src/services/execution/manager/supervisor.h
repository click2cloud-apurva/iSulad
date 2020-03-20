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
 * Description: provide container supervisor definition
 ******************************************************************************/
#ifndef __ISULAD_SUPERVISOR_H
#define __ISULAD_SUPERVISOR_H
#include <pthread.h>
#include <semaphore.h>
#include "container_unix.h"

extern char *exit_fifo_create(const char *cont_state_path);

extern char *exit_fifo_name(const char *cont_state_path);

extern int exit_fifo_open(const char *cont_exit_fifo);

extern int supervisor_add_exit_monitor(int fd, const container_pid_t *pid_info, const char *name,
                                       const char *runtime);

extern int new_supervisor();

#endif

