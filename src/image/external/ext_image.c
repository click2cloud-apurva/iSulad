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
 * Create: 2018-11-08
 * Description: provide image functions
 ******************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdarg.h>
#include <string.h>
#include <limits.h>
#include <sys/utsname.h>
#include <ctype.h>

#include "log.h"
#include "utils.h"
#include "specs_extend.h"
#include "ext_image.h"

#ifdef ENABLE_OCI_IMAGE
#include "oci_common_operators.h"
#include "oci_images_store.h"
#include "oci_config_merge.h"
#endif

bool ext_detect(const char *image_name)
{
    if (image_name == NULL) {
        return false;
    }

    if (image_name[0] != '/') {
        INFO("Rootfs should be absolutely path");
        return false;
    }

    return util_file_exists(image_name);
}
int ext_filesystem_usage(const im_container_fs_usage_request *request, imagetool_fs_info **fs_usage)
{
    return 0;
}

int ext_prepare_rf(const im_prepare_request *request, char **real_rootfs)
{
    int ret = 0;

    if (request == NULL) {
        ERROR("Invalid arguments");
        return -1;
    }

    if (real_rootfs != NULL) {
        if (request->image_name != NULL) {
            char real_path[PATH_MAX] = { 0 };
            if (request->image_name[0] != '/') {
                ERROR("Rootfs should be absolutely path");
                isulad_set_error_message("Rootfs should be absolutely path");
                return -1;
            }
            if (realpath(request->image_name, real_path) == NULL) {
                ERROR("Failed to clean rootfs path '%s': %s", request->image_name, strerror(errno));
                isulad_set_error_message("Failed to clean rootfs path '%s': %s", request->image_name, strerror(errno));
                return -1;
            }
            *real_rootfs = util_strdup_s(real_path);
        } else {
            ERROR("Failed to get external rootfs");
            ret = -1;
        }
    }
    return ret;
}

int ext_mount_rf(const im_mount_request *request)
{
    return 0;
}

int ext_umount_rf(const im_umount_request *request)
{
    return 0;
}

int ext_delete_rf(const im_delete_request *request)
{
    return 0;
}

char *ext_resolve_image_name(const char *image_name)
{
    return util_strdup_s(image_name);
}

int ext_merge_conf(const host_config *host_spec, container_config *container_spec,
                   const im_prepare_request *request, char **real_rootfs)
#ifdef ENABLE_OCI_IMAGE
{
    int ret = 0;
    char *resolved_name = NULL;
    oci_image_t *image_info = NULL;
    im_umount_request umount_request = { 0 };

    if (request == NULL) {
        ERROR("Invalid arguments");
        return -1;
    }
    // Ensure rootfs is valid.
    ret = ext_prepare_rf(request, real_rootfs);
    if (ret != 0) {
        return ret;
    }

    umount_request.name_id = request->container_id;
    umount_request.force = false;
    ret = ext_umount_rf(&umount_request);
    if (ret != 0) {
        return ret;
    }

    // No config neeed merge if NULL.
    if (request->ext_config_image == NULL) {
        ret = 0;
        goto out;
    }

    // Get image's config and merge configs.
    resolved_name = oci_resolve_image_name(request->ext_config_image);
    if (resolved_name == NULL) {
        ERROR("Resolve external config image name failed, image name is %s", request->ext_config_image);
        ret = -1;
        goto out;
    }

    image_info = oci_images_store_get(resolved_name);
    if (image_info == NULL) {
        ERROR("Get image from image store failed, image name is %s", resolved_name);
        ret = -1;
        goto out;
    }

    ret = oci_image_merge_config(image_info->info, container_spec);

out:
    free(resolved_name);
    resolved_name = NULL;

    oci_image_unref(image_info);
    image_info = NULL;

    return ret;
}
#else
{
    int ret = 0;
    im_umount_request umount_request = { 0 };

    // Ensure rootfs is valid.
    ret = ext_prepare_rf(request, real_rootfs);
    if (ret != 0) {
        return ret;
    }

    umount_request.name_id = request->container_id;
    umount_request.force = false;
    ret = ext_umount_rf(&umount_request);
    if (ret != 0) {
        return ret;
    }

    return ret;
}
#endif

int ext_get_user_conf(const char *basefs, host_config *hc, const char *userstr, defs_process_user *puser)
{
    if (basefs == NULL || puser == NULL) {
        ERROR("Empty basefs or puser");
        return -1;
    }
    return get_user(basefs, hc, userstr, puser);
}

int ext_list_images(const im_list_request *request, imagetool_images_list **list)
{
    int ret = 0;

    *list = util_common_calloc_s(sizeof(imagetool_images_list));
    if (*list == NULL) {
        ERROR("Memory out");
        ret = -1;
        goto out;
    }
out:
    return ret;
}

int ext_remove_image(const im_remove_request *request)
{
    return 0;
}

int ext_inspect_image(const im_inspect_request *request, char **inspected_json)
{
    return 0;
}

int ext_load_image(const im_load_request *request)
{
    return 0;
}

int ext_login(const im_login_request *request)
{
    return 0;
}

int ext_logout(const im_logout_request *request)
{
    return 0;
}

int ext_init(const struct im_configs *conf)
{
    return 0;
}

