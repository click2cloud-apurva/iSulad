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
#ifndef __SPECS_SECURITY_H__
#define __SPECS_SECURITY_H__

#include <stdint.h>
#include "libisulad.h"
#include "host_config.h"
#include "container_config_v2.h"
#include "oci_runtime_spec.h"
int merge_default_seccomp_spec(oci_runtime_spec *oci_spec,
                               const defs_process_capabilities *capabilites);
int merge_caps(oci_runtime_spec *oci_spec, const char **adds, size_t adds_len, const char **drops,
               size_t drops_len);
int refill_oci_process_capabilities(defs_process_capabilities **caps,
                                    const char **src_caps, size_t src_caps_len);
int merge_sysctls(oci_runtime_spec *oci_spec, const json_map_string_string *sysctls);
int merge_no_new_privileges(oci_runtime_spec *oci_spec, bool value);
int adapt_settings_for_system_container(oci_runtime_spec *oci_spec, const host_config *host_spec);
int merge_seccomp(oci_runtime_spec *oci_spec, const char *seccomp_profile);
int merge_selinux(oci_runtime_spec *oci_spec, container_config_v2_common_config *v2_spec,
                  const char **label_opts, size_t label_opts_len);

#endif

