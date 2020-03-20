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
 * Author: gaohuatao
 * Create: 2020-3-9
 * Description: container logs ops
 ******************************************************************************/

#ifndef __SHIM_TERMINAL_H
#define __SHIM_TERMINAL_H

#include <pthread.h>
#include <unistd.h>
#include "logger_json_file.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint64_t log_maxsize;
    char *log_path;
    int fd;
    unsigned int log_maxfile;
    pthread_rwlock_t log_terminal_rwlock;
} log_terminal;

void shim_write_container_log_file(log_terminal *terminal, const char *type, char *buf,
                                   int bytes_read);

int shim_create_container_log_file(log_terminal *terminal);

#ifdef __cplusplus
}
#endif

#endif /* __SHIM_TERMINAL_H */
