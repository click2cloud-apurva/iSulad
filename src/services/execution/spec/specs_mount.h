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
 * Author: maoweiyong
 * Create: 2017-11-22
 * Description: provide specs definition
 ******************************************************************************/
#ifndef __SPECS_MOUNT_H__
#define __SPECS_MOUNT_H__

#include <stdint.h>
#include "libisulad.h"
#include "host_config.h"
#include "container_config_v2.h"
#include "oci_runtime_hooks.h"
#include "oci_runtime_spec.h"

int adapt_settings_for_mounts(oci_runtime_spec *oci_spec, container_config *container_spec);

typedef defs_mount *(*parse_mount_cb)(const char *mount);

int merge_volumes(oci_runtime_spec *oci_spec, char **volumes,
                  size_t volumes_len, container_config_v2_common_config *common_config,
                  parse_mount_cb parse_mount);

defs_mount *parse_mount(const char *mount);

defs_mount *parse_volume(const char *volume);

int merge_conf_mounts(oci_runtime_spec *oci_spec, host_config *host_spec,
                      container_config_v2_common_config *common_config);

int add_rootfs_mount(const container_config *container_spec);

int set_mounts_readwrite_option(const oci_runtime_spec *oci_spec);

int merge_all_devices_and_all_permission(oci_runtime_spec *oci_spec);

bool mount_run_tmpfs(oci_runtime_spec *container, const host_config *host_spec, const char *path);

int merge_conf_device(oci_runtime_spec *oci_spec, host_config *host_spec);

#endif

