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
 * Description: provide command definition
 ******************************************************************************/

#ifndef __COMMANDER_H_
#define __COMMANDER_H_
#include <unistd.h>
#include <stdio.h>
#include <stdbool.h>

#include "host_config.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef COMMANDER_MAX_OPTIONS
#define COMMANDER_MAX_OPTIONS 64
#endif

typedef enum {
    /* no arguments */
    CMD_OPT_TYPE_BOOL,
    CMD_OPT_TYPE_BOOL_FALSE,
    /* required arguments */
    CMD_OPT_TYPE_STRING,
    CMD_OPT_TYPE_STRING_DUP,
    CMD_OPT_TYPE_CALLBACK
} command_option_type_t;


struct _command;

struct command_option;

typedef int(*command_callback_t)(struct command_option *options, const char *arg);

typedef struct command_option {
    command_option_type_t type;
    bool hasdata;
    const char *large;
    int small;
    void *data;
    const char *description;
    command_callback_t cb;
} command_option_t;

typedef struct _command {
    const char *type;
    const char *usage;
    const char *description;
    const char *name;
    const char *version;
    int option_count;
    command_option_t *options;
    int argc;
    const char **argv;
} command_t;

int command_valid_socket(command_option_t *option, const char *arg);

void command_init(command_t *self, command_option_t *opts, int opts_len, int argc, const char **argv,
                  const char *description, const char *usage);

int compare_options(const void *s1, const void *s2);

void print_options(int options_len, const command_option_t *options);

void command_help(command_t *self);

void command_option(command_t *self, command_option_type_t type, void *data,
                    int small, const char *large, const char *desc, command_callback_t cb);

int command_parse_args(command_t *self, int *argc, char * const **argv);

int have_short_options(command_t *self, char arg);

int command_valid_socket_append_array(command_option_t *option, const char *arg);

int command_append_array(command_option_t *option, const char *arg);

int command_convert_u16(command_option_t *option, const char *arg);

int command_convert_llong(command_option_t *opt, const char *arg);

int command_convert_uint(command_option_t *opt, const char *arg);

int command_convert_int(command_option_t *option, const char *arg);

int command_convert_nanoseconds(command_option_t *option, const char *arg);

int command_convert_membytes(command_option_t *option, const char *arg);

int command_convert_memswapbytes(command_option_t *option, const char *arg);

int command_default_ulimit_append(command_option_t *option, const char *arg);

size_t ulimit_array_len(host_config_ulimits_element **default_ulimit);

int ulimit_array_append(host_config_ulimits_element ***default_ulimit,
                        const host_config_ulimits_element *element,
                        const size_t len);

int check_default_ulimit_type(const char *type);

void free_default_ulimit(host_config_ulimits_element **default_ulimit);

int command_convert_swappiness(command_option_t *option, const char *arg);

#ifdef __cplusplus
}
#endif

#endif /* COMMANDER_H */

