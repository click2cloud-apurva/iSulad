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
 * Create: 2018-11-1
 * Description: provide container sha256 functions
 ********************************************************************************/

#ifndef __UTILS_VERIFY_H
#define __UTILS_VERIFY_H

#include <stdbool.h>
#include <stdint.h>
#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

extern const char *g_all_caps[];

bool util_valid_cmd_arg(const char *arg);

bool util_valid_signal(int sig);

int util_validate_absolute_path(const char *path);

bool util_validate_unix_socket(const char *socket);

bool util_validate_socket(const char *socket);

bool util_valid_device_mode(const char *mode);

bool util_valid_str(const char *str);

size_t util_get_all_caps_len();

bool util_valid_cap(const char *cap);

bool util_valid_time_tz(const char *time);

bool util_valid_embedded_image_name(const char *name);

bool util_valid_image_name(const char *name);

char *util_tag_pos(const char *ref);

bool util_valid_file(const char *path, uint32_t fmod);

bool util_valid_digest(const char *digest);

bool util_valid_digest_file(const char *path, const char *digest);

bool util_valid_key_type(const char *key);

bool util_valid_key_src(const char *key);

bool util_valid_key_dst(const char *key);

bool util_valid_key_ro(const char *key);

bool util_valid_key_propagation(const char *key);

bool util_valid_key_selinux(const char *key);

bool util_valid_value_true(const char *value);

bool util_valid_value_false(const char *value);

bool util_valid_rw_mode(const char *mode);

bool util_valid_label_mode(const char *mode);

bool util_valid_copy_mode(const char *mode);

bool util_valid_propagation_mode(const char *mode);

bool util_valid_mount_mode(const char *mode);

bool util_valid_container_id(const char *id);

bool util_valid_container_name(const char *name);

bool util_valid_container_id_or_name(const char *id_or_name);

bool util_valid_host_name(const char *name);

bool util_valid_runtime_name(const char *name);

bool util_valid_short_sha256_id(const char *id);

bool util_valid_exec_suffix(const char *suffix);

#ifdef __cplusplus
}
#endif

#endif /* __UTILS_H */

