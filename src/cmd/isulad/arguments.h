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
 * Author: lifeng
 * Create: 2017-11-22
 * Description: provide container server arguments definition
 ******************************************************************************/
#ifndef __ISULAD_ARGUMENTS_H
#define __ISULAD_ARGUMENTS_H

#include <stdbool.h>
#include <stdio.h>
#include "isulad_daemon_configs.h"
#include "oci_runtime_hooks.h"
#include "commander.h"

#ifdef ENABLE_OCI_IMAGE
#include "driver.h"
#endif

typedef void(*service_arguments_help_t)(void);

struct service_arguments {
    char *progname;
    service_arguments_help_t print_help;

    bool quiet;
    bool help;
    bool version;
    char **hosts;
    size_t hosts_len;

    // struct service_arguments *server_conf;
    isulad_daemon_configs *json_confs;
#ifdef ENABLE_OCI_IMAGE
    struct graphdriver *driver;
#endif
    /* parsed configs */
    oci_runtime_spec_hooks *hooks;

    unsigned int start_timeout;
    unsigned int image_opt_timeout;

    /* log-opts */
    unsigned int log_file_mode;
    char *logpath;
    int64_t max_size;
    int max_file;

    /* default configs */
    host_config_ulimits_element **default_ulimit;
    size_t default_ulimit_len;
    unsigned int websocket_server_listening_port;

    // remaining arguments
    char * const *argv;

    int argc;
};

int service_arguments_init(struct service_arguments *args);
void service_arguments_free(struct service_arguments *args);
int server_log_opt_parser(struct service_arguments *args, const char *option);

#endif /* __ISULAD_ARGUMENTS_H */

