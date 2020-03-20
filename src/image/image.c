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

#include "image.h"
#include "libisulad.h"
#include "log.h"
#include "utils.h"

#include "ext_image.h"
#include "filters.h"
#include "collector.h"

#ifdef ENABLE_OCI_IMAGE
#include "isula_image.h"
#include "oci_images_store.h"
#endif

#ifdef ENABLE_EMBEDDED_IMAGE
#include "embedded_image.h"
#include "db_all.h"

/* embedded */
static const struct bim_ops g_embedded_ops = {
    .init = embedded_init,
    .clean_resource = NULL,
    .detect = embedded_detect,

    .prepare_rf = embedded_prepare_rf,
    .mount_rf = embedded_mount_rf,
    .umount_rf = embedded_umount_rf,
    .delete_rf = embedded_delete_rf,
    .export_rf = NULL,

    .merge_conf = embedded_merge_conf,
    .get_user_conf = embedded_get_user_conf,

    .list_ims = embedded_list_images,
    .get_image_count = NULL,
    .rm_image = embedded_remove_image,
    .inspect_image = embedded_inspect_image,
    .resolve_image_name = embedded_resolve_image_name,
    .container_fs_usage = embedded_filesystem_usage,
    .get_filesystem_info = NULL,
    .get_storage_status = NULL,
    .image_status = NULL,
    .load_image = embedded_load_image,
    .pull_image = NULL,

    .login = NULL,
    .logout = NULL,

    .health_check = NULL,
};
#endif

/* isula image server */
#ifdef ENABLE_OCI_IMAGE
static const struct bim_ops g_isula_ops = {
    .init = isula_init,
    .clean_resource = isula_exit,
    .detect = oci_detect,

    .prepare_rf = isula_prepare_rf,
    .mount_rf = isula_mount_rf,
    .umount_rf = isula_umount_rf,
    .delete_rf = isula_delete_rf,
    .export_rf = isula_export_rf,

    .merge_conf = isula_merge_conf_rf,
    .get_user_conf = oci_get_user_conf,

    .list_ims = oci_list_images,
    .get_image_count = oci_images_store_size,
    .rm_image = isula_rmi,
    .inspect_image = oci_inspect_image,
    .resolve_image_name = oci_resolve_image_name,
    .container_fs_usage = isula_container_filesystem_usage,
    .get_filesystem_info = isula_get_filesystem_info,
    .get_storage_status = isula_get_storage_status,
    .image_status = oci_status_image,
    .load_image = isual_load_image,
    .pull_image = isula_pull_rf,
    .login = isula_login,
    .logout = isula_logout,

    .health_check = isula_health_check,
};
#endif

/* external */
static const struct bim_ops g_ext_ops = {
    .init = ext_init,
    .clean_resource = NULL,
    .detect = ext_detect,

    .prepare_rf = ext_prepare_rf,
    .mount_rf = ext_mount_rf,
    .umount_rf = ext_umount_rf,
    .delete_rf = ext_delete_rf,
    .export_rf = NULL,

    .merge_conf = ext_merge_conf,
    .get_user_conf = ext_get_user_conf,

    .list_ims = ext_list_images,
    .get_image_count = NULL,
    .rm_image = ext_remove_image,
    .inspect_image = ext_inspect_image,
    .resolve_image_name = ext_resolve_image_name,
    .container_fs_usage = ext_filesystem_usage,
    .image_status = NULL,
    .get_filesystem_info = NULL,
    .get_storage_status = NULL,
    .load_image = ext_load_image,
    .pull_image = NULL,
    .login = ext_login,
    .logout = ext_logout,

    .health_check = NULL,
};

static const struct bim_type g_bims[] = {
#ifdef ENABLE_OCI_IMAGE
    {
        .image_type = IMAGE_TYPE_OCI,
        .ops = &g_isula_ops,
    },
#endif
    { .image_type = IMAGE_TYPE_EXTERNAL, .ops = &g_ext_ops },
#ifdef ENABLE_EMBEDDED_IMAGE
    { .image_type = IMAGE_TYPE_EMBEDDED, .ops = &g_embedded_ops },
#endif
};

static const size_t g_numbims = sizeof(g_bims) / sizeof(struct bim_type);

static const struct bim_type *bim_query(const char *image_name)
{
    size_t i;
    char *temp = NULL;

    for (i = 0; i < g_numbims; i++) {
        if (g_bims[i].ops->resolve_image_name == NULL) {
            WARN("Unimplements resolve image name in %s", g_bims[i].image_type);
            continue;
        }
        if (g_bims[i].ops->detect == NULL) {
            WARN("Unimplements detect in %s", g_bims[i].image_type);
            continue;
        }
        temp = g_bims[i].ops->resolve_image_name(image_name);
        if (temp == NULL) {
            isulad_append_error_message("Failed to resovle image name%s", image_name);
            return NULL;
        }
        int r = g_bims[i].ops->detect(temp);

        free(temp);
        temp = NULL;

        if (r != 0) {
            break;
        }
    }

    if (i == g_numbims) {
        return NULL;
    }
    return &g_bims[i];
}

static const struct bim_type *get_bim_by_type(const char *image_type)
{
    size_t i;

    for (i = 0; i < g_numbims; i++) {
        if (strcmp(g_bims[i].image_type, image_type) == 0) {
            return &g_bims[i];
        }
    }

    ERROR("Backing store %s unknown but not caught earlier\n", image_type);
    return NULL;
}

static void bim_put(struct bim *bim)
{
    if (bim == NULL) {
        return;
    }

    free(bim->image_name);
    bim->image_name = NULL;
    free(bim->ext_config_image);
    bim->ext_config_image = NULL;
    free(bim->container_id);
    bim->container_id = NULL;
    free(bim);
}

static struct bim *bim_get(const char *image_type, const char *image_name, const char *ext_config_image,
                           const char *container_id)
{
    struct bim *bim = NULL;
    const struct bim_type *q = NULL;

    if (image_type == NULL) {
        return NULL;
    }

    q = get_bim_by_type(image_type);
    if (q == NULL) {
        return NULL;
    }

    bim = util_common_calloc_s(sizeof(struct bim));
    if (bim == NULL) {
        return NULL;
    }

    bim->ops = q->ops;
    bim->type = q->image_type;

    if (bim->ops->resolve_image_name == NULL) {
        ERROR("Unimplements resolve image name");
        bim_put(bim);
        return NULL;
    }

    if (image_name != NULL) {
        bim->image_name = bim->ops->resolve_image_name(image_name);
        if (bim->image_name == NULL) {
            isulad_append_error_message("Failed to resovle image name%s", image_name);
            bim_put(bim);
            return NULL;
        }
    }
    if (ext_config_image != NULL) {
        bim->ext_config_image = util_strdup_s(ext_config_image);
    }
    if (container_id != NULL) {
        bim->container_id = util_strdup_s(container_id);
    }
    return bim;
}

int im_resolv_image_name(const char *image_type, const char *image_name, char **resolved_name)
{
    int ret = -1;
    const struct bim_type *q = NULL;

    if (image_type == NULL) {
        ERROR("Image type is required");
        goto out;
    }
    q = get_bim_by_type(image_type);
    if (q == NULL) {
        goto out;
    }
    if (q->ops->resolve_image_name == NULL) {
        ERROR("Get resolve image name umimplements");
        goto out;
    }

    *resolved_name = q->ops->resolve_image_name(image_name);
    if (*resolved_name == NULL) {
        goto out;
    }

    ret = 0;
out:
    return ret;
}

int im_get_storage_status(const char *image_type, im_storage_status_response **response)
{
    int ret = -1;
    const struct bim_type *q = NULL;

    if (image_type == NULL) {
        ERROR("Image type is required");
        goto out;
    }
    q = get_bim_by_type(image_type);
    if (q == NULL) {
        goto out;
    }
    if (q->ops->get_storage_status == NULL) {
        ERROR("Get storage status umimplements");
        goto out;
    }

    ret = q->ops->get_storage_status(response);
    if (ret != 0) {
        ERROR("Get storage status failed");
        free_im_storage_status_response(*response);
        *response = NULL;
        goto out;
    }

out:
    return ret;
}

int im_get_filesystem_info(const char *image_type, im_fs_info_response **response)
{
    int ret = -1;
    const struct bim_type *q = NULL;

    if (image_type == NULL) {
        ERROR("Image type is required");
        goto out;
    }

    q = get_bim_by_type(image_type);
    if (q == NULL) {
        goto out;
    }
    if (q->ops->get_filesystem_info == NULL) {
        ERROR("Get filesystem info umimplements");
        goto out;
    }

    EVENT("Event: {Object: get image filesystem info, Type: inspecting}");
    ret = q->ops->get_filesystem_info(response);
    if (ret != 0) {
        if (response != NULL && *response != NULL) {
            ERROR("Get filesystem info failed: %s", (*response)->errmsg);
        } else {
            ERROR("Get filesystem info failed");
        }
        goto out;
    }
    EVENT("Event: {Object: get image filesystem info, Type: inspected}");

out:
    return ret;
}

int im_health_check(const char *image_type)
{
    int ret = -1;
    const struct bim_type *q = NULL;

    if (image_type == NULL) {
        ERROR("Image type is required");
        goto out;
    }

    q = get_bim_by_type(image_type);
    if (q == NULL) {
        ERROR("Get bim failed");
        goto out;
    }

    if (q->ops->health_check == NULL) {
        ERROR("Health check umimplement");
        goto out;
    }

    ret = q->ops->health_check();

out:
    return ret;
}

int im_get_container_filesystem_usage(const char *image_type, const char *id, imagetool_fs_info **fs_usage)
{
    int ret = 0;
    imagetool_fs_info *filesystemusage = NULL;
    const struct bim_type *q = NULL;
    im_container_fs_usage_request *request = NULL;

    if (image_type == NULL || id == NULL) {
        ERROR("Invalid input arguments");
        ret = -1;
        goto out;
    }

    q = get_bim_by_type(image_type);
    if (q == NULL) {
        ret = -1;
        goto out;
    }
    if (q->ops->container_fs_usage == NULL) {
        ERROR("Unimplements filesystem usage in %s", image_type);
        goto out;
    }

    request = util_common_calloc_s(sizeof(im_container_fs_usage_request));
    if (request == NULL) {
        ERROR("Out of memory");
        ret = -1;
        goto out;
    }

    if (id != NULL) {
        request->name_id = util_strdup_s(id);
    }

    EVENT("Event: {Object: container \'%s\' filesystem info, Type: inspecting}", id != NULL ? id : "");
    ret = q->ops->container_fs_usage(request, &filesystemusage);
    if (ret != 0) {
        ERROR("Failed to get filesystem usage for container %s", id);
        ret = -1;
        goto out;
    }

    *fs_usage = filesystemusage;
    EVENT("Event: {Object: container \'%s\' filesystem info, Type: inspected}", id != NULL ? id : "");

out:
    free_im_container_fs_usage_request(request);
    return ret;
}

int im_remove_container_rootfs(const char *image_type, const char *container_id)
{
    int ret = 0;
    im_delete_request *request = NULL;
    struct bim *bim = NULL;

    if (container_id == NULL || image_type == NULL) {
        ERROR("Invalid input arguments");
        ret = -1;
        goto out;
    }

    bim = bim_get(image_type, NULL, NULL, container_id);
    if (bim == NULL) {
        ERROR("Failed to init bim for container %s", container_id);
        ret = -1;
        goto out;
    }
    if (bim->ops->delete_rf == NULL) {
        ERROR("Unimplements delete in %s", bim->type);
        goto out;
    }

    request = util_common_calloc_s(sizeof(im_delete_request));
    if (request == NULL) {
        ERROR("Out of memory");
        ret = -1;
        goto out;
    }
    request->name_id = util_strdup_s(container_id);

    EVENT("Event: {Object: %s, Type: removeing rootfs}", container_id);
    ret = bim->ops->delete_rf(request);
    if (ret != 0) {
        ERROR("Failed to delete rootfs for container %s", container_id);
        ret = -1;
        goto out;
    }
    EVENT("Event: {Object: %s, Type: removed rootfs}", container_id);

out:
    bim_put(bim);
    free_im_delete_request(request);
    return ret;
}

void free_im_container_fs_usage_request(im_container_fs_usage_request *request)
{
    if (request == NULL) {
        return;
    }

    free(request->name_id);
    request->name_id = NULL;
    free(request);
}

void free_im_prepare_request(im_prepare_request *request)
{
    if (request == NULL) {
        return;
    }

    free(request->image_name);
    request->image_name = NULL;
    free(request->container_id);
    request->container_id = NULL;
    free(request->ext_config_image);
    request->ext_config_image = NULL;

    free_json_map_string_string(request->storage_opt);
    request->storage_opt = NULL;

    free(request);
}

void free_im_mount_request(im_mount_request *request)
{
    if (request == NULL) {
        return;
    }

    free(request->name_id);
    request->name_id = NULL;
    free(request);
}

void free_im_delete_request(im_delete_request *request)
{
    if (request == NULL) {
        return;
    }

    free(request->name_id);
    request->name_id = NULL;
    free(request);
}

void free_im_umount_request(im_umount_request *request)
{
    if (request == NULL) {
        return;
    }

    free(request->name_id);
    request->name_id = NULL;
    free(request);
}

int im_umount_container_rootfs(const char *image_type, const char *image_name, const char *container_id)
{
    int ret = 0;
    struct bim *bim = NULL;
    im_umount_request *request = NULL;

    if (container_id == NULL || image_type == NULL || image_name == NULL) {
        ERROR("Invalid input arguments");
        ret = -1;
        goto out;
    }

    bim = bim_get(image_type, image_name, NULL, container_id);
    if (bim == NULL) {
        ERROR("Failed to init bim for container %s", container_id);
        ret = -1;
        goto out;
    }
    if (bim->ops->umount_rf == NULL) {
        ERROR("Unimplements umount in %s", bim->type);
        goto out;
    }

    request = (im_umount_request *)util_common_calloc_s(sizeof(im_umount_request));
    if (request == NULL) {
        ERROR("Out of memory");
        ret = -1;
        goto out;
    }
    request->force = false;
    request->name_id = bim->container_id;
    bim->container_id = NULL;

    EVENT("Event: {Object: %s, Type: umounting rootfs}", container_id);
    ret = bim->ops->umount_rf(request);
    if (ret != 0) {
        ERROR("Failed to umount rootfs for container %s", container_id);
        ret = -1;
        goto out;
    }
    EVENT("Event: {Object: %s, Type: umounted rootfs}", container_id);

out:
    bim_put(bim);
    free_im_umount_request(request);
    return ret;
}

int im_mount_container_rootfs(const char *image_type, const char *image_name, const char *container_id)
{
    int ret = 0;
    struct bim *bim = NULL;
    im_mount_request *request = NULL;

    if (image_name == NULL || container_id == NULL || image_type == NULL) {
        ERROR("Invalid input arguments");
        ret = -1;
        goto out;
    }

    bim = bim_get(image_type, image_name, NULL, container_id);
    if (bim == NULL) {
        ERROR("Failed to init bim for container %s", container_id);
        ret = -1;
        goto out;
    }
    if (bim->ops->mount_rf == NULL) {
        ERROR("Unimplements mount in %s", bim->type);
        goto out;
    }

    request = util_common_calloc_s(sizeof(im_mount_request));
    if (request == NULL) {
        ERROR("Out of memory");
        ret = -1;
        goto out;
    }
    request->name_id = util_strdup_s(container_id);

    EVENT("Event: {Object: %s, Type: mounting rootfs}", container_id);
    ret = bim->ops->mount_rf(request);
    if (ret != 0) {
        ERROR("Failed to mount rootfs for container %s", container_id);
        ret = -1;
        goto out;
    }
    EVENT("Event: {Object: %s, Type: mounted rootfs}", container_id);

out:
    bim_put(bim);
    free_im_mount_request(request);
    return ret;
}

char *im_get_image_type(const char *image, const char *external_rootfs)
{
    const char *image_name = NULL;
    const struct bim_type *bim_type = NULL;

    image_name = (external_rootfs != NULL) ? external_rootfs : image;
    if (image_name == NULL) {
        ERROR("Should specify the image name or external rootfs");
        return NULL;
    }

    bim_type = bim_query(image_name);
    if (bim_type == NULL) {
        ERROR("Failed to query type of image %s", image_name);
        isulad_set_error_message("No such image:%s", image_name);
        return NULL;
    }

    return util_strdup_s(bim_type->image_type);
}

bool im_config_image_exist(const char *image_name)
{
    const struct bim_type *bim_type = NULL;

    bim_type = bim_query(image_name);
    if (bim_type == NULL) {
        ERROR("Config image %s not exist", image_name);
        isulad_set_error_message("Image %s not exist", image_name);
        return false;
    }

    return true;
}

int im_merge_image_config(const char *id, const char *image_type, const char *image_name,
                          const char *ext_config_image,
                          host_config *host_spec, container_config *container_spec,
                          char **real_rootfs)
{
    int ret = 0;
    struct bim *bim = NULL;
    im_prepare_request *request = NULL;

    if (real_rootfs == NULL || image_type == NULL) {
        ERROR("Invalid input arguments");
        ret = -1;
        goto out;
    }

    bim = bim_get(image_type, image_name, ext_config_image, id);
    if (bim == NULL) {
        ERROR("Failed to init bim of image %s", image_name);
        ret = -1;
        goto out;
    }
    if (bim->ops->merge_conf == NULL) {
        ERROR("Unimplements merge config in %s", bim->type);
        goto out;
    }

    request = util_common_calloc_s(sizeof(im_prepare_request));
    if (request == NULL) {
        ERROR("Out of memory");
        ret = -1;
        goto out;
    }
    request->container_id = util_strdup_s(id);
    request->image_name = util_strdup_s(image_name);
    request->ext_config_image = util_strdup_s(ext_config_image);
    if (host_spec != NULL) {
        request->storage_opt = host_spec->storage_opt;
    }

    EVENT("Event: {Object: %s, Type: preparing rootfs with image %s}", id, image_name);

    ret = bim->ops->merge_conf(host_spec, container_spec, request, real_rootfs);
    request->storage_opt = NULL;
    if (ret != 0) {
        ERROR("Failed to merge image %s config, config image is %s", image_name, ext_config_image);
        ret = -1;
        goto out;
    }
    EVENT("Event: {Object: %s, Type: prepared rootfs with image %s}", id, image_name);

    INFO("Use real rootfs: %s with type: %s", *real_rootfs, image_type);

out:
    bim_put(bim);
    free_im_prepare_request(request);
    return ret;
}

int im_get_user_conf(const char *image_type, const char *basefs, host_config *hc, const char *userstr,
                     defs_process_user *puser)
{
    int ret = 0;
    struct bim *bim = NULL;

    if (basefs == NULL || hc == NULL || image_type == NULL || puser == NULL) {
        ERROR("Invalid input arguments");
        ret = -1;
        goto out;
    }

    bim = bim_get(image_type, NULL, NULL, NULL);
    if (bim == NULL) {
        ERROR("Failed to init bim for image type: %s", image_type);
        ret = -1;
        goto out;
    }
    if (bim->ops->get_user_conf == NULL) {
        ERROR("Unimplements get user config in %s", bim->type);
        goto out;
    }

    ret = bim->ops->get_user_conf(basefs, hc, userstr, puser);
    if (ret != 0) {
        ERROR("Failed to get user config");
        ret = -1;
        goto out;
    }

out:
    bim_put(bim);
    return ret;
}

static int append_images_to_response(im_list_response *response, imagetool_images_list *images_in)
{
    int ret = 0;
    size_t images_num = 0;
    size_t old_num = 0;
    imagetool_image **tmp = NULL;
    size_t i = 0;
    size_t new_size = 0;
    size_t old_size = 0;

    if (images_in == NULL || response == NULL) {
        ERROR("Invalid input arguments");
        ret = -1;
        goto out;
    }

    if (response->images == NULL) {
        response->images = util_common_calloc_s(sizeof(imagetool_images_list));
        if (response->images == NULL) {
            ERROR("Memeory out");
            ret = -1;
            goto out;
        }
    }

    images_num = images_in->images_len;
    // no images need to append
    if (images_num == 0) {
        goto out;
    }
    if (images_num > SIZE_MAX / sizeof(imagetool_image *) - response->images->images_len) {
        ERROR("Too many images to append!");
        ret = -1;
        goto out;
    }

    old_num = response->images->images_len;

    new_size = (old_num + images_num) * sizeof(imagetool_image *);
    old_size = old_num * sizeof(imagetool_image *);
    ret = mem_realloc((void **)(&tmp), new_size, response->images->images, old_size);
    if (ret != 0) {
        ERROR("Failed to realloc memory for append images");
        ret = -1;
        goto out;
    }
    response->images->images = tmp;
    for (i = 0; i < images_num; i++) {
        response->images->images[old_num + i] = images_in->images[i];
        images_in->images[i] = NULL;
        images_in->images_len--;
        response->images->images_len++;
    }

out:
    return ret;
}
int im_list_images(const im_list_request *ctx, im_list_response **response)
{
    size_t i;
    imagetool_images_list *images_tmp = NULL;

    *response = util_common_calloc_s(sizeof(im_list_response));
    if (*response == NULL) {
        ERROR("Out of memory");
        return -1;
    }

    EVENT("Event: {Object: list images, Type: listing}");

    for (i = 0; i < g_numbims; i++) {
        if (g_bims[i].ops->list_ims == NULL) {
            DEBUG("bim %s umimplements list images operator", g_bims[i].image_type);
            continue;
        }
        int ret = g_bims[i].ops->list_ims(ctx, &images_tmp);
        if (ret != 0) {
            ERROR("Failed to list all images with type:%s", g_bims[i].image_type);
            continue;
        }
        ret = append_images_to_response(*response, images_tmp);
        if (ret != 0) {
            ERROR("Failed to append images with type:%s", g_bims[i].image_type);
        }
        free_imagetool_images_list(images_tmp);
        images_tmp = NULL;
    }

    EVENT("Event: {Object: list images, Type: listed}");

    if (g_isulad_errmsg != NULL) {
        (*response)->errmsg = util_strdup_s(g_isulad_errmsg);
    }

    return 0;
}

void free_im_list_request(im_list_request *ptr)
{
    if (ptr == NULL) {
        return;
    }
    free(ptr->filter.image.image);
    ptr->filter.image.image = NULL;

    filters_args_free(ptr->image_filters);
    ptr->image_filters = NULL;
    free(ptr);
}

void free_im_list_response(im_list_response *ptr)
{
    if (ptr == NULL) {
        return;
    }
    free_imagetool_images_list(ptr->images);
    ptr->images = NULL;
    free(ptr->errmsg);
    ptr->errmsg = NULL;

    free(ptr);
}

static bool check_im_pull_args(const im_pull_request *req, im_pull_response * const *resp)
{
    if (req == NULL || resp == NULL) {
        ERROR("Request or response is NULL");
        return false;
    }
    if (req->image == NULL) {
        ERROR("Empty image required");
        isulad_set_error_message("Empty image required");
        return false;
    }
    return true;
}

int im_pull_image(const im_pull_request *request, im_pull_response **response)
{
    int ret = -1;
    struct bim *bim = NULL;

    if (!check_im_pull_args(request, response)) {
        return ret;
    }

    bim = bim_get(request->type, NULL, NULL, NULL);
    if (bim == NULL) {
        ERROR("Failed to init bim, image type: %s", request->type);
        goto out;
    }

    if (bim->ops->pull_image == NULL) {
        ERROR("Unimplements pull image in %s", bim->type);
        goto out;
    }

    EVENT("Event: {Object: %s, Type: Pulling}", request->image);
    ret = bim->ops->pull_image(request, response);
    if (ret != 0) {
        ERROR("Pull image %s failed", request->image);
        ret = -1;
        goto out;
    }
    EVENT("Event: {Object: %s, Type: Pulled}", request->image);
    (void)isulad_monitor_send_image_event(request->image, IM_PULL);

out:
    bim_put(bim);
    return ret;
}

void free_im_pull_request(im_pull_request *req)
{
    if (req == NULL) {
        return;
    }
    free(req->type);
    req->type = NULL;
    free(req->image);
    req->image = NULL;
    free_sensitive_string(req->username);
    req->username = NULL;
    free_sensitive_string(req->password);
    req->password = NULL;
    free_sensitive_string(req->auth);
    req->auth = NULL;
    free_sensitive_string(req->server_address);
    req->server_address = NULL;
    free_sensitive_string(req->registry_token);
    req->registry_token = NULL;
    free_sensitive_string(req->identity_token);
    req->identity_token = NULL;
    free(req);
}

void free_im_pull_response(im_pull_response *resp)
{
    if (resp == NULL) {
        return;
    }
    free(resp->image_ref);
    resp->image_ref = NULL;
    free(resp->errmsg);
    resp->errmsg = NULL;
    free(resp);
}

int im_load_image(const im_load_request *request, im_load_response **response)
{
    int ret = -1;
    struct bim *bim = NULL;

    if (request == NULL || response == NULL) {
        ERROR("Invalid input arguments");
        return -1;
    }

    *response = util_common_calloc_s(sizeof(im_load_response));
    if (*response == NULL) {
        ERROR("Out of memory");
        return -1;
    }

    if (request->file == NULL) {
        ERROR("Load image requires image tarball file path");
        isulad_set_error_message("Load image requires image tarball file path");
        goto pack_response;
    }

    if (request->type == NULL) {
        ERROR("Missing image type");
        isulad_set_error_message("Missing image type");
        goto pack_response;
    }

    bim = bim_get(request->type, NULL, NULL, NULL);
    if (bim == NULL) {
        ERROR("Failed to init bim, image type:%s", request->type);
        goto pack_response;
    }
    if (bim->ops->load_image == NULL) {
        ERROR("Unimplements load image in %s", bim->type);
        goto pack_response;
    }

    EVENT("Event: {Object: %s, Type: loading}", request->file);

    ret = bim->ops->load_image(request);
    if (ret != 0) {
        ERROR("Failed to load image from %s with tag %s and type %s", request->file, request->tag, request->type);
        ret = -1;
        goto pack_response;
    }

    EVENT("Event: {Object: %s, Type: loaded}", request->file);

pack_response:
    if (g_isulad_errmsg != NULL) {
        (*response)->errmsg = util_strdup_s(g_isulad_errmsg);
    }

    bim_put(bim);
    return ret;
}

void free_im_load_request(im_load_request *ptr)
{
    if (ptr == NULL) {
        return;
    }

    free(ptr->file);
    ptr->file = NULL;

    free(ptr->tag);
    ptr->file = NULL;

    free(ptr->type);
    ptr->type = NULL;

    free(ptr);
}

void free_im_load_response(im_load_response *ptr)
{
    if (ptr == NULL) {
        return;
    }

    free(ptr->errmsg);
    ptr->errmsg = NULL;

    free(ptr);
}

static bool check_login_request(const im_login_request *request)
{
    if (request == NULL) {
        ERROR("Invalid input arguments");
        return false;
    }

    if (request->server == NULL) {
        ERROR("Login requires server address");
        isulad_set_error_message("Login requires server address");
        return false;
    }

    if (request->type == NULL) {
        ERROR("Login requires image type");
        isulad_set_error_message("Login requires image type");
        return false;
    }

    if (request->username == NULL || request->password == NULL) {
        ERROR("Missing username or password");
        isulad_set_error_message("Missing username or password");
        return false;
    }
    return true;
}

int im_login(const im_login_request *request, im_login_response **response)
{
    int ret = -1;
    struct bim *bim = NULL;

    if (response == NULL) {
        ERROR("Empty response");
        return -1;
    }

    *response = util_common_calloc_s(sizeof(im_login_response));
    if (*response == NULL) {
        ERROR("Out of memory");
        return -1;
    }

    if (!check_login_request(request)) {
        goto pack_response;
    }

    bim = bim_get(request->type, NULL, NULL, NULL);
    if (bim == NULL) {
        ERROR("Failed to init bim, image type:%s", request->type);
        goto pack_response;
    }

    if (bim->ops->login == NULL) {
        ERROR("Unimplements login in %s", bim->type);
        goto pack_response;
    }

    EVENT("Event: {Object: %s, Type: logining}", request->server);

    ret = bim->ops->login(request);
    if (ret != 0) {
        ERROR("Failed to login %s", request->server);
        ret = -1;
        goto pack_response;
    }

    EVENT("Event: {Object: %s, Type: logined}", request->server);

pack_response:
    if (g_isulad_errmsg != NULL) {
        (*response)->errmsg = util_strdup_s(g_isulad_errmsg);
    }

    bim_put(bim);
    return ret;
}

void free_im_login_request(im_login_request *ptr)
{
    if (ptr == NULL) {
        return;
    }

    free_sensitive_string(ptr->username);
    ptr->username = NULL;

    free_sensitive_string(ptr->password);
    ptr->password = NULL;

    free(ptr->type);
    ptr->type = NULL;

    free_sensitive_string(ptr->server);
    ptr->server = NULL;

    free(ptr);
}

void free_im_login_response(im_login_response *ptr)
{
    if (ptr == NULL) {
        return;
    }

    free(ptr->errmsg);
    ptr->errmsg = NULL;

    free(ptr);
}

static bool check_logout_request(const im_logout_request *request)
{
    if (request == NULL) {
        ERROR("Invalid input arguments");
        return false;
    }

    if (request->server == NULL) {
        ERROR("Logout requires server address");
        isulad_set_error_message("Logout requires server address");
        return false;
    }

    if (request->type == NULL) {
        ERROR("Logout requires image type");
        isulad_set_error_message("Logout requires image type");
        return false;
    }
    return true;
}

int im_logout(const im_logout_request *request, im_logout_response **response)
{
    int ret = -1;
    struct bim *bim = NULL;

    if (response == NULL) {
        ERROR("Empty response");
        return -1;
    }

    *response = util_common_calloc_s(sizeof(im_logout_response));
    if (*response == NULL) {
        ERROR("Out of memory");
        return -1;
    }

    if (!check_logout_request(request)) {
        goto pack_response;
    }

    bim = bim_get(request->type, NULL, NULL, NULL);
    if (bim == NULL) {
        ERROR("Failed to init bim, image type:%s", request->type);
        goto pack_response;
    }

    if (bim->ops->logout == NULL) {
        ERROR("Unimplements logout in %s", bim->type);
        goto pack_response;
    }

    EVENT("Event: {Object: %s, Type: logouting}", request->server);

    ret = bim->ops->logout(request);
    if (ret != 0) {
        ERROR("Failed to logout %s", request->server);
        ret = -1;
        goto pack_response;
    }

    EVENT("Event: {Object: %s, Type: logouted}", request->server);

pack_response:
    if (g_isulad_errmsg != NULL) {
        (*response)->errmsg = util_strdup_s(g_isulad_errmsg);
    }

    bim_put(bim);
    return ret;
}

void free_im_logout_request(im_logout_request *ptr)
{
    if (ptr == NULL) {
        return;
    }

    free(ptr->type);
    ptr->type = NULL;

    free(ptr->server);
    ptr->server = NULL;

    free(ptr);
}

void free_im_logout_response(im_logout_response *ptr)
{
    if (ptr == NULL) {
        return;
    }

    free(ptr->errmsg);
    ptr->errmsg = NULL;

    free(ptr);
}

int im_image_status(im_status_request *request, im_status_response **response)
{
    int ret = -1;
    char *image_ref = NULL;
    const struct bim_type *bim_type = NULL;
    struct bim *bim = NULL;

    if (request == NULL || response == NULL) {
        ERROR("Invalid input arguments");
        return -1;
    }

    *response = (im_status_response *)util_common_calloc_s(sizeof(im_status_response));
    if (*response == NULL) {
        ERROR("Out of memory");
        return -1;
    }

    if (request->image.image == NULL) {
        ERROR("get image status requires image ref");
        isulad_set_error_message("get image status requires image ref");
        goto pack_response;
    }

    image_ref = util_strdup_s(request->image.image);

    bim_type = bim_query(image_ref);
    if (bim_type == NULL) {
        ERROR("No such image:%s", image_ref);
        isulad_set_error_message("No such image:%s", image_ref);
        goto pack_response;
    }

    bim = bim_get(bim_type->image_type, image_ref, NULL, NULL);
    if (bim == NULL) {
        ERROR("Failed to init bim for image %s", image_ref);
        goto pack_response;
    }
    if (bim->ops->image_status == NULL) {
        ERROR("Unimplements image status in %s", bim->type);
        goto pack_response;
    }

    ret = bim->ops->image_status(request, response);
    if (ret != 0) {
        ERROR("Failed to get status of image %s", image_ref);
        ret = -1;
    }

pack_response:
    if (g_isulad_errmsg != NULL) {
        free((*response)->errmsg);
        (*response)->errmsg = util_strdup_s(g_isulad_errmsg);
    }
    free(image_ref);
    bim_put(bim);
    return ret;
}

int im_rm_image(const im_remove_request *request, im_remove_response **response)
{
    int ret = -1;
    char *image_ref = NULL;
    const struct bim_type *bim_type = NULL;
    struct bim *bim = NULL;

    if (request == NULL || response == NULL) {
        ERROR("Invalid input arguments");
        return -1;
    }

    *response = util_common_calloc_s(sizeof(im_remove_response));
    if (*response == NULL) {
        ERROR("Out of memory");
        return -1;
    }

    if (request->image.image == NULL) {
        ERROR("remove image requires image ref");
        isulad_set_error_message("remove image requires image ref");
        goto pack_response;
    }

    image_ref = util_strdup_s(request->image.image);

    EVENT("Event: {Object: %s, Type: image removing}", image_ref);

    bim_type = bim_query(image_ref);
    if (bim_type == NULL) {
        ERROR("No such image:%s", image_ref);
        isulad_set_error_message("No such image:%s", image_ref);
        goto pack_response;
    }

    bim = bim_get(bim_type->image_type, image_ref, NULL, NULL);
    if (bim == NULL) {
        ERROR("Failed to init bim for image %s", image_ref);
        goto pack_response;
    }
    if (bim->ops->rm_image == NULL) {
        ERROR("Unimplements rm image in %s", bim->type);
        goto pack_response;
    }

    ret = bim->ops->rm_image(request);
    if (ret != 0) {
        ERROR("Failed to remove image %s", image_ref);
        ret = -1;
        goto pack_response;
    }

    EVENT("Event: {Object: %s, Type: image removed}", image_ref);
    (void)isulad_monitor_send_image_event(image_ref, IM_REMOVE);

pack_response:
    if (g_isulad_errmsg != NULL) {
        (*response)->errmsg = util_strdup_s(g_isulad_errmsg);
    }
    free(image_ref);
    bim_put(bim);
    return ret;
}

void free_im_remove_request(im_remove_request *ptr)
{
    if (ptr == NULL) {
        return;
    }
    free(ptr->image.image);
    ptr->image.image = NULL;

    free(ptr);
}

void free_im_remove_response(im_remove_response *ptr)
{
    if (ptr == NULL) {
        return;
    }

    free(ptr->errmsg);
    ptr->errmsg = NULL;

    free(ptr);
}

static int do_im_inspect_image(struct bim *bim, char **inspected_json)
{
    int ret = 0;
    im_inspect_request *update_request = NULL;

    if (bim->ops->inspect_image == NULL) {
        ERROR("Unimplements inspect image in %s", bim->type);
        ret = -1;
        goto out;
    }
    update_request = util_common_calloc_s(sizeof(im_inspect_request));
    if (update_request == NULL) {
        ERROR("Out of memory");
        ret = -1;
        goto out;
    }
    update_request->image.image = bim->image_name;
    bim->image_name = NULL;

    ret = bim->ops->inspect_image(update_request, inspected_json);
    if (ret != 0) {
        ERROR("Failed to inspect image %s", update_request->image.image);
        ret = -1;
    }

out:
    free_im_inspect_request(update_request);
    return ret;
}

int im_inspect_image(const im_inspect_request *request, im_inspect_response **response)
{
    int ret = 0;
    char *image_ref = NULL;
    char *inspected_json = NULL;
    const struct bim_type *bim_type = NULL;
    struct bim *bim = NULL;

    if (request == NULL || response == NULL) {
        ERROR("Invalid input arguments");
        return -1;
    }

    *response = util_common_calloc_s(sizeof(im_inspect_response));
    if (*response == NULL) {
        ERROR("Out of memory");
        return -1;
    }

    if (request->image.image == NULL) {
        ERROR("inspect image requires image ref");
        isulad_set_error_message("inspect image requires image ref");
        ret = -1;
        goto pack_response;
    }

    image_ref = util_strdup_s(request->image.image);

    EVENT("Event: {Object: %s, Type: image inspecting}", image_ref);

    bim_type = bim_query(image_ref);
    if (bim_type == NULL) {
        ERROR("No such image:%s", image_ref);
        isulad_set_error_message("No such image:%s", image_ref);
        ret = -1;
        goto pack_response;
    }

    bim = bim_get(bim_type->image_type, image_ref, NULL, NULL);
    if (bim == NULL) {
        ERROR("Failed to init bim for image %s", image_ref);
        ret = -1;
        goto pack_response;
    }

    ret = do_im_inspect_image(bim, &inspected_json);
    if (ret != 0) {
        goto pack_response;
    }

    EVENT("Event: {Object: %s, Type: image inspected}", image_ref);

pack_response:
    if (g_isulad_errmsg != NULL) {
        (*response)->errmsg = util_strdup_s(g_isulad_errmsg);
    }
    if (inspected_json != NULL) {
        (*response)->im_inspect_json = util_strdup_s(inspected_json);
    }
    free(image_ref);
    free(inspected_json);
    bim_put(bim);
    return ret;
}

void free_im_inspect_request(im_inspect_request *ptr)
{
    if (ptr == NULL) {
        return;
    }
    free(ptr->image.image);
    ptr->image.image = NULL;

    free(ptr);
}

void free_im_inspect_response(im_inspect_response *ptr)
{
    if (ptr == NULL) {
        return;
    }

    free(ptr->im_inspect_json);
    ptr->im_inspect_json = NULL;

    free(ptr->errmsg);
    ptr->errmsg = NULL;

    free(ptr);
}

/*
 * parameters: image type
 * return: error return 0, success return image count
 * */
size_t im_get_image_count(const im_image_count_request *request)
{
    size_t ret = 0;
    const struct bim_type *q = NULL;

    if (request == NULL || request->type == NULL) {
        ERROR("Image type is required");
        goto out;
    }
    q = get_bim_by_type(request->type);
    if (q == NULL) {
        goto out;
    }
    if (q->ops->get_image_count == NULL) {
        ERROR("Get image count umimplements");
        goto out;
    }

    ret = q->ops->get_image_count();

out:
    return ret;
}

void free_im_image_count_request(im_image_count_request *ptr)
{
    if (ptr == NULL) {
        return;
    }
    free(ptr->type);
    ptr->type = NULL;
    free(ptr);
}

int im_container_export(const im_export_request *request)
{
    int ret = 0;
    int nret = 0;
    struct bim *bim = NULL;

    if (request == NULL) {
        ERROR("Invalid input arguments");
        return -1;
    }

    if (request->file == NULL) {
        ERROR("Container export requires output file path");
        ret = -1;
        goto out;
    }
    if (request->name_id == NULL) {
        ERROR("Container export requires container id");
        ret = -1;
        goto out;
    }

    if (request->type == NULL) {
        ERROR("Missing image type");
        ret = -1;
        goto out;
    }

    bim = bim_get(request->type, NULL, NULL, NULL);
    if (bim == NULL) {
        ERROR("Failed to init bim, image type:%s", request->type);
        ret = -1;
        goto out;
    }
    if (bim->ops->export_rf == NULL) {
        ERROR("Unimplements container export in %s", bim->type);
        ret = -1;
        goto out;
    }

    EVENT("Event: {Object: %s, Type: exporting}", request->file);

    nret = bim->ops->export_rf(request);
    if (nret != 0) {
        ERROR("Failed to export container from %s into file %s and type %s",
              request->name_id, request->file, request->type);
        ret = -1;
        goto out;
    }

    EVENT("Event: {Object: %s, Type: exported}", request->name_id);

out:
    bim_put(bim);
    return ret;
}

void free_im_export_request(im_export_request *ptr)
{
    if (ptr == NULL) {
        return;
    }
    free(ptr->type);
    ptr->type = NULL;
    free(ptr->file);
    ptr->file = NULL;
    free(ptr->name_id);
    ptr->name_id = NULL;
    free(ptr);
}

void free_im_configs(struct im_configs *conf)
{
    if (conf == NULL) {
        return;
    }
    free(conf->rootpath);
    conf->rootpath = NULL;
    free(conf->server_sock);
    conf->server_sock = NULL;
    free(conf);
}

static int bims_init(const struct im_configs *conf)
{
    int ret = 0;
    size_t i;

    for (i = 0; i < g_numbims; i++) {
        if (g_bims[i].ops->init == NULL) {
            WARN("Unimplements init in %s", g_bims[i].image_type);
            continue;
        }
        ret = g_bims[i].ops->init(conf);
        if (ret != 0) {
            ERROR("Failed to init bim %s", g_bims[i].image_type);
            break;
        }
    }

    return ret;
}

int image_module_init(const char *rootpath)
{
    struct im_configs *conf;
    int ret = -1;

    conf = (struct im_configs *)util_common_calloc_s(sizeof(struct im_configs));
    if (conf == NULL) {
        ERROR("Out of memory");
        return ret;
    }
    conf->rootpath = util_strdup_s(rootpath);
    ret = bims_init(conf);

    free_im_configs(conf);
    return ret;
}

void image_module_exit()
{
    size_t i;

    for (i = 0; i < g_numbims; i++) {
        if (g_bims[i].ops->clean_resource == NULL) {
            continue;
        }
        g_bims[i].ops->clean_resource();
    }
}

int map_to_key_value_string(const json_map_string_string *map, char ***array, size_t *array_len)
{
    char **strings = NULL;
    size_t strings_len = 0;
    size_t i;
    int ret;

    if (map == NULL) {
        return 0;
    }
    for (i = 0; i < map->len; i++) {
        char *str = NULL;
        size_t len;
        if (strlen(map->keys[i]) > (SIZE_MAX - strlen(map->values[i])) - 2) {
            ERROR("Invalid keys/values");
            goto cleanup;
        }
        len = strlen(map->keys[i]) + strlen(map->values[i]) + 2;
        str = util_common_calloc_s(len);
        if (str == NULL) {
            ERROR("Out of memory");
            goto cleanup;
        }
        ret = snprintf(str, len, "%s=%s", map->keys[i], map->values[i]);
        if (ret < 0 || (size_t)ret >= len) {
            ERROR("Failed to print string");
            free(str);
            goto cleanup;
        }
        ret = util_array_append(&strings, str);
        free(str);
        if (ret != 0) {
            ERROR("Failed to append array");
            goto cleanup;
        }
        strings_len++;
    }
    *array = strings;
    *array_len = strings_len;
    return 0;

cleanup:
    util_free_array(strings);
    return -1;
}

void free_im_status_request(im_status_request *req)
{
    if (req == NULL) {
        return;
    }
    free(req->image.image);
    req->image.image = NULL;

    free(req);
}

void free_im_status_response(im_status_response *req)
{
    if (req == NULL) {
        return;
    }
    free_imagetool_image_status(req->image_info);
    req->image_info = NULL;
    free(req->errmsg);
    req->errmsg = NULL;

    free(req);
}

void free_im_fs_info_response(im_fs_info_response *ptr)
{
    if (ptr == NULL) {
        return;
    }
    free_imagetool_fs_info(ptr->fs_info);
    ptr->fs_info = NULL;
    free(ptr->errmsg);
    ptr->errmsg = NULL;

    free(ptr);
}

void free_im_storage_status_response(im_storage_status_response *ptr)
{
    if (ptr == NULL) {
        return;
    }
    free(ptr->backing_fs);
    ptr->backing_fs = NULL;
    free(ptr->status);
    ptr->status = NULL;
    free(ptr);
}

void im_sync_containers_isuladkit(void)
{
    DEBUG("Sync containers...");
#ifdef ENABLE_OCI_IMAGE
    if (isula_sync_containers() != 0) {
        WARN("Sync containers with remote failed!!");
    }
#endif
}
