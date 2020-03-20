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
#ifndef __SPECS_H__
#define __SPECS_H__

#include <stdint.h>
#include "libisulad.h"
#include "host_config.h"
#include "container_config_v2.h"
#include "oci_runtime_hooks.h"
#include "oci_runtime_spec.h"

#ifdef __cplusplus
extern "C" {
#endif

int merge_all_specs(host_config *host_spec,
                    const char *real_rootfs,
                    container_config_v2_common_config *v2_spec, oci_runtime_spec *oci_spec);
int merge_oci_cgroups_path(const char *id, oci_runtime_spec *oci_spec,
                           const host_config *host_spec);
int merge_global_config(oci_runtime_spec *oci_spec);
oci_runtime_spec *load_oci_config(const char *rootpath, const char *name);
oci_runtime_spec *default_spec(bool system_container);
int merge_conf_cgroup(oci_runtime_spec *oci_spec, const host_config *host_spec);
int save_oci_config(const char *id, const char *rootpath, const oci_runtime_spec *oci_spec);

int parse_security_opt(const host_config *host_spec, bool *no_new_privileges,
                       char ***label_opts, size_t *label_opts_len,
                       char **seccomp_profile);
#ifdef __cplusplus
}
#endif

#endif

