/******************************************************************************
 * Copyright (c) Huawei Technologies Co., Ltd. 2019-2020. All rights reserved.
 * iSulad licensed under the Mulan PSL v1.
 * You can use this software according to the terms and conditions of the Mulan PSL v1.
 * You may obtain a copy of Mulan PSL v1 at:
 *     http://license.coscl.org.cn/MulanPSL
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR
 * PURPOSE.
 * See the Mulan PSL v1 for more details.
 * Author: wujing
 * Create: 2019-12-15
 * Description: provide selinux label handle function definition
 ******************************************************************************/

#ifndef __SELINUX_LABLE_H
#define __SELINUX_LABLE_H

#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

int selinux_state_init(void);
void selinux_set_disabled();
bool selinux_get_enable();
int init_label(const char **label_opts, size_t label_opts_len, char **process_label, char **mount_label);
int relabel(const char *path, const char *file_label, bool shared);
int get_disable_security_opt(char ***labels, size_t *labels_len);
int dup_security_opt(const char *src, char ***dst, size_t *len);

#ifdef __cplusplus
}
#endif

#endif /* __SELINUX_LABLE_H */
