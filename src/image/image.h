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
 * Author: weiwei
 * Create: 2017-11-22
 * Description: provide image function definition
 ******************************************************************************/
#ifndef __IMAGE_H
#define __IMAGE_H

#include <stdint.h>

#include "oci_image_manifest.h"
#include "oci_image_index.h"
#include "oci_image_spec.h"
#include "oci_runtime_spec.h"
#include "host_config.h"
#include "container_config.h"
#include "libisulad.h"

#ifdef ENABLE_OCI_IMAGE
#include "oci_image_type.h"
#endif

#include "imagetool_images_list.h"
#include "imagetool_fs_info.h"
#include "imagetool_image_status.h"

#ifdef __cplusplus
extern "C" {
#endif

#define IMAGE_TYPE_OCI "oci"
#ifdef ENABLE_EMBEDDED_IMAGE
#define IMAGE_TYPE_EMBEDDED "embedded"
#endif
#define IMAGE_TYPE_EXTERNAL "external"

#ifndef ENABLE_OCI_IMAGE
typedef struct {
    char *image;
} image_spec;

typedef struct {
    image_spec image;
} image_filter;
#endif

typedef struct {
    char *backing_fs;
    char *status;
} im_storage_status_response;

typedef struct {
    imagetool_fs_info *fs_info;
    char *errmsg;
} im_fs_info_response;

typedef struct {
    // Spec of the image.
    image_spec image;
    // Verbose indicates whether to return extra information about the image.
    bool verbose;
} im_status_request;

typedef struct {
    imagetool_image_status *image_info;
    char *errmsg;
} im_status_response;

typedef struct {
    image_filter filter;
    bool check;
    struct filters_args *image_filters;
} im_list_request;

typedef struct {
    imagetool_images_list *images;
    char *errmsg;
} im_list_response;

typedef struct {
    // Spec of the image.
    image_spec image;

    bool force;
} im_remove_request;

typedef struct {
    char *errmsg;
} im_remove_response;

typedef struct {
    // Spec of the image.
    image_spec image;
} im_inspect_request;

typedef struct {
    char *im_inspect_json;
    char *errmsg;
} im_inspect_response;

typedef struct {
    char *file;
    char *tag;
    char *type;
} im_load_request;

typedef struct {
    char *errmsg;
} im_load_response;

typedef struct {
    char *type;
    char *image;

    /* auth configs */
    char *username;
    char *password;
    char *auth;
    char *server_address;
    char *identity_token;
    char *registry_token;
} im_pull_request;

typedef struct {
    char *image_ref;
    char *errmsg;
} im_pull_response;

typedef struct {
    char *server;
    char *username;
    char *password;
    char *type;
} im_login_request;

typedef struct {
    char *errmsg;
} im_login_response;

typedef struct {
    char *server;
    char *type;
} im_logout_request;

typedef struct {
    char *errmsg;
} im_logout_response;

typedef struct {
    char *type;
} im_image_count_request;

typedef struct {
    char *type;
    char *file;
    char *name_id;
} im_export_request;

typedef struct {
    char *name_id;
    bool force;
} im_umount_request;

typedef struct {
    char *name_id;
} im_mount_request;

typedef struct {
    char *name_id;
} im_delete_request;

typedef struct {
    char *image_name;
    char *container_id;
    char *ext_config_image;
    json_map_string_string *storage_opt;
} im_prepare_request;

typedef struct {
    char *name_id;
} im_container_fs_usage_request;

/* configs of image driver */
struct im_configs {
    char *rootpath;
    char *server_sock;
};

struct bim_ops {
    int (*init)(const struct im_configs *conf);
    void (*clean_resource)(void);

    /* detect whether image is of this bim type */
    bool (*detect)(const char *image_name);

    /* rootfs ops */
    int (*prepare_rf)(const im_prepare_request *request, char **real_rootfs);
    int (*mount_rf)(const im_mount_request *request);
    int (*umount_rf)(const im_umount_request *request);
    int (*delete_rf)(const im_delete_request *request);
    int (*export_rf)(const im_export_request *request);
    char *(*resolve_image_name)(const char *image_name);

    /* merge image config ops */
    int (*merge_conf)(const host_config *host_spec, container_config *container_spec,
                      const im_prepare_request *request, char **real_rootfs);

    /* get user config ops */
    int (*get_user_conf)(const char *basefs, host_config *hc,
                         const char *userstr, defs_process_user *puser);

    /* list images */
    int (*list_ims)(const im_list_request *request, imagetool_images_list **images);

    /* get count of images */
    size_t (*get_image_count)(void);

    /* remove image */
    int (*rm_image)(const im_remove_request *request);

    /* inspect image */
    int (*inspect_image)(const im_inspect_request *request, char **inpected_json);

    int (*container_fs_usage)(const im_container_fs_usage_request *request, imagetool_fs_info **fs_usage);

    int (*image_status)(im_status_request *request, im_status_response **response);

    int (*get_filesystem_info)(im_fs_info_response **response);

    int (*get_storage_status)(im_storage_status_response **response);

    /* load image */
    int (*load_image)(const im_load_request *request);

    /* pull image */
    int (*pull_image)(const im_pull_request *request, im_pull_response **response);

    /* login */
    int (*login)(const im_login_request *request);

    /* logout */
    int (*logout)(const im_logout_request *request);

    /* health check */
    int (*health_check)(void);
};

struct bim {
    /* common arguments */
    const struct bim_ops *ops;
    const char *type;

    char *image_name;
    char *ext_config_image;
    char *container_id;
};

struct bim_type {
    const char *image_type;
    const struct bim_ops *ops;
};

int image_module_init(const char *rootpath);

void image_module_exit();

int im_get_container_filesystem_usage(const char *image_type, const char *id, imagetool_fs_info **fs_usage);

void free_im_container_fs_usage_request(im_container_fs_usage_request *request);

void free_im_prepare_request(im_prepare_request *request);

void free_im_mount_request(im_mount_request *request);

void free_im_umount_request(im_umount_request *request);

void free_im_delete_request(im_delete_request *request);

int im_mount_container_rootfs(const char *image_type, const char *image_name, const char *container_id);

int im_umount_container_rootfs(const char *image_type, const char *image_name, const char *container_id);

int im_remove_container_rootfs(const char *image_type, const char *container_id);

int im_merge_image_config(const char *id, const char *image_type, const char *image_name,
                          const char *ext_config_image,
                          host_config *host_spec, container_config *container_spec,
                          char **real_rootfs);

int im_get_user_conf(const char *image_type, const char *basefs, host_config *hc, const char *userstr,
                     defs_process_user *puser);

int im_list_images(const im_list_request *request, im_list_response **response);

void free_im_list_request(im_list_request *ptr);

void free_im_list_response(im_list_response *ptr);

int im_rm_image(const im_remove_request *request, im_remove_response **response);

void free_im_remove_request(im_remove_request *ptr);

void free_im_remove_response(im_remove_response *ptr);

int im_inspect_image(const im_inspect_request *request, im_inspect_response **response);

void free_im_inspect_request(im_inspect_request *ptr);

void free_im_inspect_response(im_inspect_response *ptr);

int map_to_key_value_string(const json_map_string_string *map, char ***array, size_t *array_len);

int im_load_image(const im_load_request *request, im_load_response **response);

void free_im_load_request(im_load_request *ptr);

void free_im_load_response(im_load_response *ptr);

int im_pull_image(const im_pull_request *request, im_pull_response **response);

void free_im_pull_request(im_pull_request *req);

void free_im_pull_response(im_pull_response *resp);

char *im_get_image_type(const char *image, const char *external_rootfs);

bool im_config_image_exist(const char *image_name);

int im_login(const im_login_request *request, im_login_response **response);

void free_im_login_request(im_login_request *ptr);

void free_im_login_response(im_login_response *ptr);

int im_logout(const im_logout_request *request, im_logout_response **response);

void free_im_logout_request(im_logout_request *ptr);

void free_im_logout_response(im_logout_response *ptr);

void free_im_configs(struct im_configs *conf);

int im_image_status(im_status_request *request, im_status_response **response);

void free_im_status_request(im_status_request *req);

void free_im_status_response(im_status_response *resp);

int im_get_filesystem_info(const char *image_type, im_fs_info_response **response);

void free_im_fs_info_response(im_fs_info_response *ptr);

int im_get_storage_status(const char *image_type, im_storage_status_response **response);

void free_im_storage_status_response(im_storage_status_response *ptr);

size_t im_get_image_count(const im_image_count_request *request);

void free_im_image_count_request(im_image_count_request *ptr);

int im_container_export(const im_export_request *request);

void free_im_export_request(im_export_request *ptr);

int im_resolv_image_name(const char *image_type, const char *image_name, char **resolved_name);

int im_health_check(const char *image_type);

void im_sync_containers_isuladkit(void);

#ifdef __cplusplus
}
#endif

#endif
