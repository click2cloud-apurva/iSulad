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
 * Description: provide container specs functions
 ******************************************************************************/
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <inttypes.h>
#include <unistd.h>
#include <stdbool.h>
#include <sys/sysmacros.h>
#include <sys/types.h>
#include <sys/mount.h>
#include <pwd.h>
#include <grp.h>
#include <sys/stat.h>
#include <dirent.h>
#include <sys/utsname.h>
#include <sched.h>
#include <ctype.h>

#include "error.h"
#include "log.h"
#include "oci_runtime_spec.h"
#include "oci_runtime_hooks.h"
#include "host_config.h"
#include "utils.h"
#include "config.h"
#include "path.h"
#include "isulad_config.h"
#include "namespace.h"
#include "parse_common.h"
#include "specs_mount.h"
#include "specs_extend.h"
#include "containers_store.h"
#include "selinux_label.h"

static int get_devices(const char *dir, char ***devices, size_t *device_len,
                       int recursive_depth);


static int append_additional_mounts(oci_runtime_spec *oci_spec, const char *type)
{
    int ret = 0;
    size_t i = 0;
    size_t j = 0;
    size_t new_size = 0;
    size_t old_size = 0;
    defs_mount **spec_mounts = NULL;
    defs_mount *tmp_mount = NULL;
    char **files = NULL;
    size_t files_len = 0;
    char *mount_options[] = {"nosuid", "noexec", "nodev"};
    size_t mount_options_len = 0;
    char *net_files[] = {"/proc/sys/net"};
    char *ipc_files[] = {
        "/proc/sys/kernel/shmmax",
        "/proc/sys/kernel/shmmni",
        "/proc/sys/kernel/shmall",
        "/proc/sys/kernel/shm_rmid_forced",
        "/proc/sys/kernel/msgmax",
        "/proc/sys/kernel/msgmni",
        "/proc/sys/kernel/msgmnb",
        "/proc/sys/kernel/sem",
        "/proc/sys/fs/mqueue"
    };

    if (strcmp(type, "net") == 0) {
        files = &net_files[0];
        files_len = sizeof(net_files) / sizeof(net_files[0]);
    } else if (strcmp(type, "ipc") == 0) {
        files = &ipc_files[0];
        files_len = sizeof(ipc_files) / sizeof(ipc_files[0]);
    } else {
        return -1;
    }

    if (files_len > (SIZE_MAX / sizeof(defs_mount *)) - oci_spec->mounts_len) {
        ERROR("Too many mounts to append!");
        ret = -1;
        goto out;
    }

    mount_options_len = sizeof(mount_options) / sizeof(mount_options[0]);
    new_size = (oci_spec->mounts_len + files_len) * sizeof(defs_mount *);
    old_size = oci_spec->mounts_len * sizeof(defs_mount *);

    ret = mem_realloc((void **)&spec_mounts, new_size, oci_spec->mounts, old_size);
    if (ret != 0) {
        ERROR("Out of memory");
        ret = -1;
        goto out;
    }

    oci_spec->mounts = spec_mounts;
    for (i = 0; i < files_len; i++) {
        tmp_mount = util_common_calloc_s(sizeof(defs_mount));
        if (tmp_mount == NULL) {
            ERROR("Out of memory");
            ret = -1;
            goto out;
        }
        tmp_mount->source = util_strdup_s(files[i]);
        tmp_mount->destination = util_strdup_s(files[i]);
        tmp_mount->type = util_strdup_s("proc");
        for (j = 0; j < mount_options_len; j++) {
            ret = util_array_append(&(tmp_mount->options), mount_options[j]);
            if (ret != 0) {
                ERROR("append mount options to array failed");
                ret = -1;
                goto out;
            }
            tmp_mount->options_len++;
        }

        spec_mounts[oci_spec->mounts_len++] = tmp_mount;
        tmp_mount = NULL;
    }

out:
    free_defs_mount(tmp_mount);
    return ret;
}

int adapt_settings_for_mounts(oci_runtime_spec *oci_spec, container_config *container_spec)
{
    size_t i, array_str_len;
    int ret = 0;
    char **array_str = NULL;

    if (container_spec == NULL) {
        return -1;
    }

    array_str = util_string_split(container_spec->ns_change_opt, ',');
    if (array_str == NULL) {
        ERROR("Out of memory");
        ret = -1;
        goto out;
    }
    array_str_len = util_array_len((const char **)array_str);

    for (i = 0; i < array_str_len; i++) {
        if (strcmp(array_str[i], "net") == 0) {
            ret = append_additional_mounts(oci_spec, "net");
        } else {
            ret = append_additional_mounts(oci_spec, "ipc");
        }
        if (ret != 0) {
            goto out;
        }
    }

out:
    util_free_array(array_str);
    return ret;
}

static bool valid_dirent_info(const char *dir, const struct dirent *info_archivo)
{
    size_t i = 0;
    int nret = 0;
    char *pdot = NULL;
    char fullpath[PATH_MAX] = {0};
    char *skip_files[] = { "/dev/pts", "/dev/shm", "/dev/fd", "/dev/mqueue", "/dev/console" };

    if (dir == NULL || info_archivo == NULL) {
        return false;
    }

    // skip . ..
    if (strncmp(info_archivo->d_name, ".", PATH_MAX) == 0 ||
        strncmp(info_archivo->d_name, "..", PATH_MAX) == 0) {
        return false;
    }

    // do not map device which name containes ":"
    pdot = strstr(info_archivo->d_name, ":");
    if (pdot != NULL) {
        INFO("Skipping device %s include \":\"", info_archivo->d_name);
        return false;
    }

    nret = snprintf(fullpath, PATH_MAX, "%s/%s", dir, info_archivo->d_name);
    if (nret < 0 || nret >= PATH_MAX) {
        ERROR("get_devices: Failed to combine device path");
        return false;
    }

    for (i = 0; i < sizeof(skip_files) / sizeof(skip_files[0]); i++) {
        if (strncmp(fullpath, skip_files[i], PATH_MAX) == 0) {
            INFO("Skipping device: %s", fullpath);
            return false;
        }
    }

    return true;
}

static int pack_devices(const char *fullpath, char ***devices, size_t *device_len)
{
    int ret = 0;
    size_t tmp_length, new_size = 0, old_size = 0;
    char **tmp_device = NULL;

    tmp_length = *device_len;
    if (tmp_length > (SIZE_MAX / sizeof(char *)) - 1) {
        ERROR("Too many devices");
        ret = -1;
        goto out;
    }

    old_size = sizeof(char *) * tmp_length;
    tmp_length += 1;
    new_size = sizeof(char *) * tmp_length;

    ret = mem_realloc((void **)&tmp_device, new_size, *devices, old_size);
    if (ret != 0) {
        ERROR("get_devices: Failed to realloc memory");
        ret = -1;
        goto out;
    }

    tmp_device[tmp_length - 1] = util_strdup_s(fullpath);
    *devices = tmp_device;
    *device_len = tmp_length;

out:
    return ret;
}

static int get_devices_in_path(const char *fullpath, int recursive_depth, char ***devices, size_t *device_len)
{
    int ret = 0;
    struct stat fileStat;

    if (lstat(fullpath, &fileStat) != 0) {
        ERROR("Failed to get(%s) file stat.", fullpath);
        ret = 0;
        goto out;
    }

    // scan device recursively
    if (S_ISDIR(fileStat.st_mode)) {
        ret = get_devices(fullpath, devices, device_len, (recursive_depth + 1));
        if (ret != 0) {
            INFO("get_devices: Failed in path: %s", fullpath);
            ret = -1;
            goto out;
        }
    } else if (S_ISCHR(fileStat.st_mode) || S_ISBLK(fileStat.st_mode)) {
        if (pack_devices(fullpath, devices, device_len) != 0) {
            ret = -1;
            goto out;
        }
    } else {
        INFO("Skip device %s, mode(%o) not support to merge.", fullpath, fileStat.st_mode & S_IFMT);
    }

out:
    return ret;
}

/* get_devices: retrieve all devices under dir
 * when errors occurs, caller should free memory pointing to *devices
 */
static int get_devices(const char *dir, char ***devices, size_t *device_len,
                       int recursive_depth)
{
    int nret = 0;
    char *fullpath = NULL;
    DIR *midir = NULL;
    struct dirent *info_archivo = NULL;

    if ((recursive_depth + 1) > MAX_PATH_DEPTH) {
        ERROR("Reach the max dev path depth:%s", dir);
        return -1;
    }
    midir = opendir(dir);
    if (midir  == NULL) {
        ERROR("get_devices: Error in opendir");
        return -1;
    }
    info_archivo = readdir(midir);
    for (; info_archivo  != NULL; info_archivo = readdir(midir)) {
        if (!valid_dirent_info(dir, info_archivo)) {
            continue;
        }

        fullpath = util_common_calloc_s(PATH_MAX);
        if (fullpath == NULL) {
            ERROR("Memory out");
            closedir(midir);
            return -1;
        }
        nret = snprintf(fullpath, PATH_MAX, "%s/%s", dir, info_archivo->d_name);
        if (nret < 0 || nret >= PATH_MAX) {
            ERROR("get_devices: Failed to combine device path");
            closedir(midir);
            free(fullpath);
            return -1;
        }

        if (get_devices_in_path(fullpath, recursive_depth, devices, device_len) != 0) {
            closedir(midir);
            free(fullpath);
            return -1;
        }

        free(fullpath);
        fullpath = NULL;
    }

    closedir(midir);
    return 0;
}

#define DefaultPropagationMode "rprivate"
#define DefaultROMode "rw"
#define DefaultRBind "rbind"

static int fill_mounts_readonly_item(const char *value, defs_mount *mount_element)
{
    char *romode = DefaultROMode;

    if (mount_element == NULL) {
        return 2;
    }

    if (value != NULL) {
        if (util_valid_value_true(value)) {
            romode = "ro";
        } else if (util_valid_value_false(value)) {
            /* use default value rw */
        } else {
            ERROR("invalid value for readonly: %s", value);
            return 2;
        }
    }

    if (util_array_append(&mount_element->options, romode)) {
        ERROR("append ro mode to array failed");
        return 2;
    }
    mount_element->options_len++;

    return 0;
}

/*
 * 0: success
 * 1: ignore this item, continue
 * 2: failed
 */
static int fill_mounts_item(const char *key, const char *value, defs_mount *mount_element, bool *has_ro, bool *has_pro)
{
    if (value == NULL && !util_valid_key_ro(key)) {
        ERROR("unsupported item %s", key);
        return 1;
    }

    if (util_valid_key_type(key)) {
        free(mount_element->type);
        mount_element->type = util_strdup_s(value);
    } else if (util_valid_key_src(key)) {
        free(mount_element->source);
        mount_element->source = util_strdup_s(value);
    } else if (util_valid_key_dst(key)) {
        free(mount_element->destination);
        mount_element->destination = util_strdup_s(value);
    } else if (util_valid_key_ro(key)) {
        int ret = fill_mounts_readonly_item(value, mount_element);
        if (ret != 0) {
            return ret;
        }

        *has_ro = true;
    } else if (util_valid_key_propagation(key)) {
        if (!util_valid_propagation_mode(value)) {
            ERROR("invalid propagation mode %s", value);
            return 2;
        }

        if (util_array_append(&mount_element->options, value)) {
            ERROR("append bind propagation to array failed");
            return 2;
        }
        mount_element->options_len++;
        *has_pro = true;
    } else if (util_valid_key_selinux(key)) {
        if (!util_valid_label_mode(value)) {
            ERROR("invalid bind selinux opts %s", value);
            return 2;
        }

        /* This option is not supported currently. Hasen does't want to modify
         * code if it's supported in future, so we support it in interface but
         * not implement it currently.
         */
        WARN("Valid bind selinux opts %s found but not configured for now", value);
    } else {
        ERROR("unsupported item %s", key);
        return 2;
    }

    return 0;
}

static int check_mount_element(const defs_mount *mount_element)
{
    int ret = 0;

    if (mount_element == NULL) {
        ret = EINVALIDARGS;
        goto out;
    }

    if (mount_element->type == NULL) {
        ERROR("type is requested");
        ret = EINVALIDARGS;
        goto out;
    }

    if (strcmp(mount_element->type, "squashfs") &&
        strcmp(mount_element->type, "bind")) {
        ERROR("invalid type %s, only support squashfs and bind", mount_element->type);
        ret = EINVALIDARGS;
        goto out;
    }

    if (mount_element->source == NULL) {
        ERROR("source is requested");
        ret = EINVALIDARGS;
        goto out;
    }

    if (mount_element->source[0] != '/') {
        ERROR("source should be absolute path");
        ret = EINVALIDARGS;
        goto out;
    }

    if (mount_element->destination == NULL) {
        ERROR("destination is requested");
        ret = EINVALIDARGS;
        goto out;
    }

    if (mount_element->destination[0] != '/') {
        ERROR("destination should be absolute path");
        ret = EINVALIDARGS;
        goto out;
    }
out:
    return ret;
}

static int append_default_mount_options(defs_mount *mount_element, bool has_ro, bool has_pro)
{
    int ret = 0;

    if (mount_element == NULL) {
        ret = -1;
        goto out;
    }

    if (strcmp(mount_element->type, "bind") == 0) {
        if (!has_ro) {
            ret = util_array_append(&mount_element->options, DefaultROMode);
            if (ret != 0) {
                ERROR("append default ro mode to array failed");
                ret = -1;
                goto out;
            }
            mount_element->options_len++;
        }

        if (!has_pro) {
            ret = util_array_append(&mount_element->options, DefaultPropagationMode);
            if (ret != 0) {
                ERROR("append default propagation mode to array failed");
                ret = -1;
                goto out;
            }
            mount_element->options_len++;
        }

        ret = util_array_append(&mount_element->options, DefaultRBind);
        if (ret != 0) {
            ERROR("append default rbind to array failed");
            ret = -1;
            goto out;
        }
        mount_element->options_len++;
    }

out:
    return ret;
}

defs_mount *parse_mount(const char *mount)
{
    char **items = NULL;
    defs_mount *mount_element = NULL;
    int ret = 0;
    size_t items_len = 0;
    size_t i = 0;
    char **kv = NULL;
    bool has_ro = false;
    bool has_pro = false;
    char dstpath[PATH_MAX] = {0};

    if (mount == NULL) {
        ERROR("invalid NULL param");
        ret = EINVALIDARGS;
        goto out;
    }
    if (!mount[0]) {
        ERROR("mount can't be empty");
        ret = EINVALIDARGS;
        goto out;
    }

    mount_element = util_common_calloc_s(sizeof(defs_mount));
    if (mount_element == NULL) {
        ERROR("Out of memory");
        return NULL;
    }

    items = util_string_split(mount, ',');
    if (items == NULL) {
        ERROR("split mount %s failed", mount);
        ret = -1;
        goto out;
    }

    items_len = util_array_len((const char **)items);

    for (i = 0; i < items_len; i++) {
        kv = util_string_split(items[i], '=');
        if (kv == NULL) {
            continue;
        }

        ret = fill_mounts_item(kv[0], kv[1], mount_element, &has_ro, &has_pro);
        if (ret == 1) {  /* ignore this item */
            ret = 0;
            util_free_array(kv);
            kv = NULL;
            continue;
        } else if (ret == 2) { /* invalid args */
            ret = EINVALIDARGS;
            goto out;
        }
        util_free_array(kv);
        kv = NULL;
    }

    ret = check_mount_element(mount_element);
    if (ret != 0) {
        goto out;
    }

    if (!cleanpath(mount_element->destination, dstpath, sizeof(dstpath))) {
        ERROR("failed to get clean path");
        ret = EINVALIDARGS;
        goto out;
    }

    free(mount_element->destination);
    mount_element->destination = util_strdup_s(dstpath);
    if (mount_element->destination == NULL) {
        ERROR("out of memory");
        ret = -1;
        goto out;
    }

    /* append default options if it's bind mount */
    ret = append_default_mount_options(mount_element, has_ro, has_pro);
    if (ret != 0) {
        goto out;
    }

out:
    if (ret != 0) {
        free_defs_mount(mount_element);
        mount_element = NULL;
    }

    util_free_array(kv);
    util_free_array(items);

    return mount_element;
}

static int check_volume_element(const char *volume)
{
    int ret = 0;

    if (volume == NULL || !strcmp(volume, "")) {
        ERROR("Volume can't be empty");
        ret = -1;
        return ret;
    }

    if (volume[0] == ':' || volume[strlen(volume) - 1] == ':') {
        ERROR("Delimiter ':' can't be the first or the last character");
        ret = -1;
        return ret;
    }

    return ret;
}

static int get_src_dst_mode_by_volume(const char *volume, defs_mount *mount_element, char ***modes)
{
    int ret = 0;
    size_t alen = 0;
    char **array = NULL;

    // split volume to src:dest:mode
    array = util_string_split(volume, ':');
    if (array == NULL) {
        ERROR("Out of memory");
        ret = -1;
        goto free_out;
    }

    alen = util_array_len((const char **)array);
    switch (alen) {
        case 1:
            ERROR("Not supported volume format '%s'", volume);
            ret = -1;
            break;
        case 2:
            if (util_valid_mount_mode(array[1])) {
                // Destination + Mode is not a valid volume - volumes
                // cannot include a mode. eg /foo:rw
                ERROR("Invalid volume specification '%s'", volume);
                isulad_set_error_message("Invalid volume specification '%s',Invalid mode:%s", volume, array[1]);
                ret = -1;
                break;
            }
            mount_element->source = util_strdup_s(array[0]);
            mount_element->destination = util_strdup_s(array[1]);
            break;
        case 3:
            mount_element->source = util_strdup_s(array[0]);
            mount_element->destination = util_strdup_s(array[1]);
            if (!util_valid_mount_mode(array[2])) {
                ERROR("Invalid volume specification '%s'", volume);
                isulad_set_error_message("Invalid volume specification '%s'.Invalid mode:%s", volume, array[2]);
                ret = -1;
                break;
            }
            *modes = util_string_split(array[2], ',');
            if (*modes == NULL) {
                ERROR("Out of memory");
                ret = -1;
                break;
            }

            break;
        default:
            ERROR("Invalid volume specification '%s'", volume);
            ret = -1;
            break;
    }
    if (ret != 0) {
        goto free_out;
    }

    if (mount_element->source[0] != '/' || mount_element->destination[0] != '/' ||
        strcmp(mount_element->destination, "/") == 0) {
        ERROR("Invalid volume: path must be absolute, and destination can't be '/'");
        ret = -1;
        goto free_out;
    }

free_out:
    util_free_array(array);
    return ret;
}

defs_mount *parse_volume(const char *volume)
{
    int ret = 0;
    size_t i = 0, mlen = 0;
    defs_mount *mount_element = NULL;
    char **modes = NULL;
    char dstpath[PATH_MAX] = { 0x00 };
    char *rw = "rw";
    char *pro = DefaultPropagationMode;
    char *label = NULL;
    size_t options_len = 3;

    ret = check_volume_element(volume);
    if (ret != 0) {
        goto free_out;
    }

    mount_element = util_common_calloc_s(sizeof(defs_mount));
    if (mount_element == NULL) {
        ERROR("Out of memory");
        return NULL;
    }

    ret = get_src_dst_mode_by_volume(volume, mount_element, &modes);
    if (ret != 0) {
        goto free_out;
    }

    mlen = util_array_len((const char **)modes);
    for (i = 0; i < mlen; i++) {
        if (util_valid_rw_mode(modes[i])) {
            rw = modes[i];
        } else if (util_valid_propagation_mode(modes[i])) {
            pro = modes[i];
        } else if (util_valid_label_mode(modes[i])) {
            label = modes[i];
        } else if (util_valid_copy_mode(modes[i])) {
            WARN("Valid mode '%s' found but not configured for now", modes[i]);
        }
    }

    if (!cleanpath(mount_element->destination, dstpath, sizeof(dstpath))) {
        ERROR("Failed to get clean path");
        ret = -1;
        goto free_out;
    }
    free(mount_element->destination);
    mount_element->destination = util_strdup_s(dstpath);
    if (label != NULL) {
        options_len++;
    }
    mount_element->options = util_common_calloc_s(options_len * sizeof(char *));
    if (mount_element->options == NULL) {
        ERROR("Out of memory");
        mount_element->options_len = 0;
        ret = -1;
        goto free_out;
    }
    mount_element->options[0] = util_strdup_s(rw);
    mount_element->options[1] = util_strdup_s(pro);
    mount_element->options[2] = util_strdup_s("rbind");
    if (options_len >= 4) {
        mount_element->options[3] = util_strdup_s(label);
    }
    mount_element->options_len = options_len;
    mount_element->type = util_strdup_s("bind");

free_out:
    util_free_array(modes);
    if (ret != 0) {
        free_defs_mount(mount_element);
        mount_element = NULL;
    }
    return mount_element;
}

static host_config_devices_element *parse_one_device(const char *device_path, const char *dir_host,
                                                     const char *dir_container, const char *permissions)
{
    int nret = 0;
    const char *cgroup_permissions = NULL;
    host_config_devices_element *device_map = NULL;
    char tmp_container_path[PATH_MAX] = {0};

    if (device_path == NULL || !strcmp(device_path, "")) {
        ERROR("devices can't be empty");
        return NULL;
    }

    cgroup_permissions = permissions ? permissions : "rwm";

    device_map = util_common_calloc_s(sizeof(host_config_devices_element));
    if (device_map == NULL) {
        ERROR("Out of memory");
        return NULL;
    }

    device_map->path_on_host = util_strdup_s(device_path);
    if (dir_container != NULL) {
        nret = snprintf(tmp_container_path, sizeof(tmp_container_path), "%s/%s",
                        dir_container, device_path + strlen(dir_host));
        if (nret < 0 || (size_t)nret >= sizeof(tmp_container_path)) {
            ERROR("Failed to sprintf device path in container %s/%s", dir_container, device_path + strlen(dir_host));
            goto erro_out;
        }
        device_map->path_in_container = util_strdup_s(tmp_container_path);
    } else {
        device_map->path_in_container = util_strdup_s(device_path);
    }

    device_map->cgroup_permissions = util_strdup_s(cgroup_permissions);

    if (device_map->path_on_host == NULL || device_map->path_in_container == NULL ||
        device_map->cgroup_permissions == NULL) {
        goto erro_out;
    } else {
        return device_map;
    }

erro_out:
    free_host_config_devices_element(device_map);
    return NULL;
}

static int get_devices_from_path(const host_config_devices_element *dev_map, defs_device *spec_dev,
                                 defs_device_cgroup *spec_dev_cgroup)
{
    int ret = 0;
    struct stat st;
    char *dev_type = NULL;
    unsigned int file_mode = 0;

    if (dev_map == NULL || spec_dev == NULL || spec_dev_cgroup == NULL) {
        return -1;
    }

    ret = stat(dev_map->path_on_host, &st);
    if (ret < 0) {
        ERROR("device %s no exists", dev_map->path_on_host);
        isulad_set_error_message("Error gathering device information while adding device \"%s\",stat %s: "
                                 "no such file or directory", dev_map->path_on_host, dev_map->path_on_host);
        return -1;
    }

    file_mode = st.st_mode & 0777;

    /* check device type first */
    if (S_ISBLK(st.st_mode)) {
        file_mode |= S_IFBLK;
        dev_type = "b";
    } else if (S_ISCHR(st.st_mode)) {
        file_mode |= S_IFCHR;
        dev_type = "c";
    } else {
        ERROR("Cannot determine the device number for device %s",
              dev_map->path_on_host);
        return -1;
    }

    /* fill spec dev */
    spec_dev->major = (int64_t)major(st.st_rdev);
    spec_dev->minor = (int64_t)minor(st.st_rdev);
    spec_dev->uid = st.st_uid;
    spec_dev->gid = st.st_gid;
    spec_dev->file_mode = (int)file_mode;
    spec_dev->type = util_strdup_s(dev_type);
    spec_dev->path = util_strdup_s(dev_map->path_in_container);

    /* fill spec cgroup dev */
    spec_dev_cgroup->allow = true;
    spec_dev_cgroup->access = util_strdup_s(dev_map->cgroup_permissions);
    spec_dev_cgroup->type = util_strdup_s(dev_type);
    spec_dev_cgroup->major = (int64_t)major(st.st_rdev);
    spec_dev_cgroup->minor = (int64_t)minor(st.st_rdev);

    return 0;
}

static int merge_custom_device(defs_device **out_spec_dev,
                               defs_device_cgroup **out_spec_dev_cgroup,
                               const host_config_devices_element *dev_map)
{
    int ret = 0;
    defs_device *spec_dev = NULL;
    defs_device_cgroup *spec_dev_cgroup = NULL;

    spec_dev = util_common_calloc_s(sizeof(defs_device));
    if (spec_dev == NULL) {
        ERROR("Memory out");
        ret = -1;
        goto erro_out;
    }

    spec_dev_cgroup = util_common_calloc_s(sizeof(defs_device_cgroup));
    if (spec_dev_cgroup == NULL) {
        ERROR("Memory out");
        ret = -1;
        goto erro_out;
    }

    ret = get_devices_from_path(dev_map, spec_dev, spec_dev_cgroup);
    if (ret != 0) {
        ERROR("Failed to get devices info");
        ret = -1;
        goto erro_out;
    }

    *out_spec_dev = spec_dev;
    *out_spec_dev_cgroup = spec_dev_cgroup;
    goto out;

erro_out:
    free_defs_device(spec_dev);
    free_defs_device_cgroup(spec_dev_cgroup);
out:
    return ret;
}

static int get_weight_devices_from_path(const host_config_blkio_weight_device_element *weight_dev,
                                        defs_block_io_device_weight *spec_weight_dev)
{
    int ret = 0;
    struct stat st;

    if (weight_dev == NULL || spec_weight_dev == NULL) {
        return -1;
    }

    ret = stat(weight_dev->path, &st);
    if (ret < 0) {
        ERROR("Failed to get state of device:%s", weight_dev->path);
        return -1;
    }

    /* fill spec weight dev */
    spec_weight_dev->major = (int64_t)major(st.st_rdev);
    spec_weight_dev->minor = (int64_t)minor(st.st_rdev);
    spec_weight_dev->weight = weight_dev->weight;
    /* Notes You MUST specify at least one of weight or leafWeight
     * in a given entry, and MAY specify both
     * docker did not specify the leaf weight
     */
    spec_weight_dev->leaf_weight = 0;

    return 0;
}

static int merge_host_config_blk_weight_device(defs_block_io_device_weight **out_spec_weight_dev,
                                               const host_config_blkio_weight_device_element *weight_dev)
{
    int ret = 0;
    defs_block_io_device_weight *spec_weight_dev = NULL;

    spec_weight_dev = util_common_calloc_s(sizeof(defs_block_io_device_weight));
    if (spec_weight_dev == NULL) {
        ERROR("Memory out");
        ret = -1;
        goto erro_out;
    }

    ret = get_weight_devices_from_path(weight_dev, spec_weight_dev);
    if (ret != 0) {
        ERROR("Failed to get weight devices info");
        ret = -1;
        goto erro_out;
    }

    *out_spec_weight_dev = spec_weight_dev;
    goto out;

erro_out:
    free_defs_block_io_device_weight(spec_weight_dev);

out:
    return ret;
}

static int get_read_bps_devices_from_path(const host_config_blkio_device_read_bps_element *read_bps_dev,
                                          defs_block_io_device_throttle *spec_read_bps_dev)
{
    int ret = 0;
    struct stat st;

    if (read_bps_dev == NULL || spec_read_bps_dev == NULL) {
        return -1;
    }

    ret = stat(read_bps_dev->path, &st);
    if (ret < 0) {
        ERROR("Failed to get state of device:%s", read_bps_dev->path);
        isulad_set_error_message("no such file or directory: %s", read_bps_dev->path);
        return -1;
    }

    /* fill spec throttle read bps dev */
    spec_read_bps_dev->rate = read_bps_dev->rate;
    spec_read_bps_dev->major = (int64_t)major(st.st_rdev);
    spec_read_bps_dev->minor = (int64_t)minor(st.st_rdev);

    return 0;
}

static int merge_host_config_blk_read_bps_device(
    defs_block_io_device_throttle **out_spec_read_bps_dev,
    const host_config_blkio_device_read_bps_element *blkio_device_read_bps)
{
    int ret = 0;
    defs_block_io_device_throttle *spec_read_bps_dev = NULL;

    spec_read_bps_dev = util_common_calloc_s(sizeof(defs_block_io_device_throttle));
    if (spec_read_bps_dev == NULL) {
        ERROR("Memory out");
        ret = -1;
        goto erro_out;
    }

    ret = get_read_bps_devices_from_path(blkio_device_read_bps, spec_read_bps_dev);
    if (ret != 0) {
        ERROR("Failed to get throttle read bps devices info");
        ret = -1;
        goto erro_out;
    }

    *out_spec_read_bps_dev = spec_read_bps_dev;
    goto out;

erro_out:
    free_defs_block_io_device_throttle(spec_read_bps_dev);

out:
    return ret;
}

static int get_write_bps_devices_from_path(const host_config_blkio_device_write_bps_element *write_bps_dev,
                                           defs_block_io_device_throttle *spec_write_bps_dev)
{
    int ret = 0;
    struct stat st;

    if (write_bps_dev == NULL || spec_write_bps_dev == NULL) {
        return -1;
    }

    ret = stat(write_bps_dev->path, &st);
    if (ret < 0) {
        ERROR("no such file or directory :%s", write_bps_dev->path);
        isulad_set_error_message("no such file or directory: %s", write_bps_dev->path);
        return -1;
    }

    /* fill spec throttle write bps dev */
    spec_write_bps_dev->rate = write_bps_dev->rate;
    spec_write_bps_dev->major = (int64_t)major(st.st_rdev);
    spec_write_bps_dev->minor = (int64_t)minor(st.st_rdev);

    return 0;
}

static int merge_host_config_blk_write_bps_device(
    defs_block_io_device_throttle **out_spec_write_bps_dev,
    const host_config_blkio_device_write_bps_element *blkio_device_write_bps)
{
    int ret = 0;
    defs_block_io_device_throttle *spec_write_bps_dev = NULL;

    spec_write_bps_dev = util_common_calloc_s(sizeof(defs_block_io_device_throttle));
    if (spec_write_bps_dev == NULL) {
        ERROR("Memory out");
        ret = -1;
        goto erro_out;
    }

    ret = get_write_bps_devices_from_path(blkio_device_write_bps, spec_write_bps_dev);
    if (ret != 0) {
        ERROR("Failed to get throttle write bps devices info");
        ret = -1;
        goto erro_out;
    }

    *out_spec_write_bps_dev = spec_write_bps_dev;
    goto out;

erro_out:
    free_defs_block_io_device_throttle(spec_write_bps_dev);

out:
    return ret;
}

static int merge_all_devices(oci_runtime_spec *oci_spec, host_config_devices_element **dev_maps,
                             size_t devices_len, const char *permissions)
{
    int ret = 0;
    size_t new_size = 0, old_size = 0;
    size_t i = 0;
    defs_device **spec_dev = NULL;

    ret = make_sure_oci_spec_linux_resources(oci_spec);
    if (ret < 0) {
        goto out;
    }

    /* malloc for linux->device */
    if (devices_len > LIST_DEVICE_SIZE_MAX - oci_spec->linux->devices_len) {
        ERROR("Too many linux devices to merge, the limit is %d", LIST_DEVICE_SIZE_MAX);
        isulad_set_error_message("Too many linux devices to merge, the limit is %d", LIST_DEVICE_SIZE_MAX);
        ret = -1;
        goto out;
    }
    new_size = (oci_spec->linux->devices_len + devices_len) * sizeof(defs_device *);
    old_size = oci_spec->linux->devices_len * sizeof(defs_device *);
    ret = mem_realloc((void **)&spec_dev, new_size, oci_spec->linux->devices, old_size);
    if (ret != 0) {
        ERROR("Out of memory");
        ret = -1;
        goto out;
    }
    oci_spec->linux->devices = spec_dev;

    /* malloc for cgroup->device */
    defs_device_cgroup **spec_cgroup_dev = NULL;
    if (devices_len > SIZE_MAX / sizeof(defs_device_cgroup *) -
        oci_spec->linux->resources->devices_len) {
        ERROR("Too many cgroup devices to merge!");
        ret = -1;
        goto out;
    }
    new_size = (oci_spec->linux->resources->devices_len + devices_len)
               * sizeof(defs_device_cgroup *);
    old_size = oci_spec->linux->resources->devices_len * sizeof(defs_device_cgroup *);
    ret = mem_realloc((void **)&spec_cgroup_dev, new_size, oci_spec->linux->resources->devices, old_size);
    if (ret != 0) {
        ERROR("Out of memory");
        ret = -1;
        goto out;
    }
    oci_spec->linux->resources->devices = spec_cgroup_dev;

    for (i = 0; i < devices_len; i++) {
        ret = merge_custom_device(&oci_spec->linux->devices[oci_spec->linux->devices_len],
                                  &oci_spec->linux->resources->devices[oci_spec->linux->resources->devices_len],
                                  dev_maps[i]);
        if (ret != 0) {
            ERROR("Failed to merge custom device");
            ret = -1;
            goto out;
        }
        oci_spec->linux->devices_len++;
        oci_spec->linux->resources->devices_len++;
    }
out:
    return ret;
}

static void free_multi_dev_maps(host_config_devices_element **dev_maps, size_t devices_len)
{
    size_t i = 0;

    if (dev_maps == NULL) {
        return;
    }

    for (i = 0; i < devices_len; i++) {
        free_host_config_devices_element(dev_maps[i]);
        dev_maps[i] = NULL;
    }
    free(dev_maps);
}

static host_config_devices_element **parse_multi_devices(const char *dir_host, const char *dir_container,
                                                         const char *permissions, char **devices, size_t devices_len)
{
    size_t i = 0;
    host_config_devices_element **dev_maps = NULL;

    if (devices_len == 0) {
        return NULL;
    }

    if (devices_len > SIZE_MAX / sizeof(host_config_devices_element *)) {
        ERROR("Too many devices");
        return NULL;
    }

    dev_maps = util_common_calloc_s(devices_len * sizeof(host_config_devices_element *));
    if (dev_maps == NULL) {
        ERROR("Memory out");
        return NULL;
    }

    for (i = 0; i < devices_len; i++) {
        dev_maps[i] = parse_one_device(devices[i], dir_host, dir_container, permissions);
        if (dev_maps[i] == NULL) {
            ERROR("Failed to parse device %s", devices[i]);
            goto on_error;
        }
    }

    return dev_maps;

on_error:
    free_multi_dev_maps(dev_maps, devices_len);
    return NULL;
}

static int merge_all_devices_in_dir(const char *dir, const char *dir_container, const char *permissions,
                                    oci_runtime_spec *oci_spec)
{
    int ret = 0;
    size_t i = 0;
    char **devices = NULL;
    size_t devices_len = 0;
    host_config_devices_element **dev_maps = NULL;

    ret = get_devices(dir, &devices, &devices_len, 0);
    if (ret != 0) {
        ERROR("Failed to get host's device in directory:%s", dir);
        isulad_set_error_message("Failed to get host's device in directory:%s", dir);
        ret = -1;
        goto out;
    }
    if (devices_len == 0) {
        ERROR("Error gathering device information while adding devices in directory \"%s\":no available device nodes",
              dir);
        isulad_set_error_message("Error gathering device information while adding devices in directory"
                                 " \"%s\":,no available device nodes", dir);
        ret = -1;
        goto out;
    }
    /* devices which will be populated into container */
    if (devices != NULL) {
        dev_maps = parse_multi_devices(dir, dir_container, permissions, devices, devices_len);
        if (dev_maps == NULL) {
            ERROR("Failed to parse multi devices");
            ret = -1;
            goto out;
        }

        ret = merge_all_devices(oci_spec, dev_maps, devices_len, permissions);
        if (ret != 0) {
            ERROR("Failed to merge devices");
            ret = -1;
            goto out;
        }
    }
out:
    if (devices != NULL) {
        for (i = 0; i < devices_len; i++) {
            free(devices[i]);
        }
        free(devices);
    }

    free_multi_dev_maps(dev_maps, devices_len);
    return ret;
}

int merge_all_devices_and_all_permission(oci_runtime_spec *oci_spec)
{
    int ret = 0;
    size_t i = 0;
    defs_resources *ptr = NULL;
    defs_device_cgroup *spec_dev_cgroup = NULL;

    ret = merge_all_devices_in_dir("/dev", NULL, NULL, oci_spec);
    if (ret != 0) {
        ERROR("Failed to merge all devices in /dev");
        ret = -1;
        goto out;
    }

    ret = make_sure_oci_spec_linux_resources(oci_spec);
    if (ret < 0) {
        goto out;
    }

    ptr = oci_spec->linux->resources;
    if (ptr->devices != NULL) {
        for (i = 0; i < ptr->devices_len; i++) {
            free_defs_device_cgroup(ptr->devices[i]);
            ptr->devices[i] = NULL;
        }
        free(ptr->devices);
        ptr->devices = NULL;
        ptr->devices_len = 0;
    }

    ptr->devices = util_common_calloc_s(sizeof(defs_device_cgroup *));
    if (ptr->devices == NULL) {
        ret = -1;
        goto out;
    }

    ptr->devices_len = 1;

    spec_dev_cgroup = util_common_calloc_s(sizeof(defs_device_cgroup));
    if (spec_dev_cgroup == NULL) {
        ret = -1;
        goto out;
    }

    spec_dev_cgroup->allow = true;
    spec_dev_cgroup->access = util_strdup_s("rwm");
    spec_dev_cgroup->type = util_strdup_s("a");
    spec_dev_cgroup->major = -1;
    spec_dev_cgroup->minor = -1;

    ptr->devices[0] = spec_dev_cgroup;

out:
    return ret;
}

static inline bool is_sys_proc_mount_destination(const char *destination)
{
    return strcmp("/sys", destination) == 0 || strcmp("/proc", destination) == 0 || \
           strcmp("/sys/fs/cgroup", destination) == 0;
}

static inline bool is_ro_mount_option(const char *option)
{
    return strncmp(option, "ro", strlen("ro")) == 0;
}

int set_mounts_readwrite_option(const oci_runtime_spec *oci_spec)
{
    size_t i = 0;
    size_t j = 0;

    if (oci_spec == NULL) {
        return -1;
    }

    if (oci_spec->mounts == NULL || oci_spec->mounts_len == 0) {
        return 0;
    }

    /* make sure /sys and proc and sys/fs/cgroup writeable */

    for (i = 0; i < oci_spec->mounts_len; i++) {
        if (!oci_spec->mounts[i]->destination) {
            continue;
        }
        if (!is_sys_proc_mount_destination(oci_spec->mounts[i]->destination)) {
            continue;
        }
        if (!oci_spec->mounts[i]->options || !oci_spec->mounts[i]->options_len) {
            continue;
        }
        // default mount permission is rw, so do nothing if "ro" not found
        for (j = 0; j < oci_spec->mounts[i]->options_len; j++) {
            // change mount permission to rw
            if (is_ro_mount_option(oci_spec->mounts[i]->options[j])) {
                oci_spec->mounts[i]->options[j][1] = 'w';
                break;
            }
        }
    }

    return 0;
}

static container_config_v2_common_config_mount_points_element *defs_mnt_to_mount_point(const defs_mount *mnt)
{
    container_config_v2_common_config_mount_points_element *mp = NULL;
    size_t i;
    char *mode = NULL;

    mp = util_common_calloc_s(sizeof(container_config_v2_common_config_mount_points_element));
    if (mp == NULL) {
        ERROR("Out of memory");
        return NULL;
    }
    mp->source = util_strdup_s(mnt->source);
    mp->destination = util_strdup_s(mnt->destination);
    mp->rw = true;
    for (i = 0; i < mnt->options_len; i++) {
        if (strcmp(mnt->options[i], "ro") == 0) {
            mp->rw = false;
        }
        if (util_valid_propagation_mode(mnt->options[i])) {
            free(mp->propagation);
            mp->propagation = util_strdup_s(mnt->options[i]);
        }
        if (strstr(mnt->options[i], "bind") != NULL) {
            continue;
        }
        if (mode == NULL) {
            mode = util_strdup_s(mnt->options[i]);
        } else {
            int pret;
            size_t len = strlen(mode) + strlen(mnt->options[i]) + 2;
            char *new_mode = util_common_calloc_s(len);
            if (new_mode == NULL) {
                ERROR("Out of memory");
                goto cleanup;
            }
            pret = snprintf(new_mode, len, "%s,%s", mode, mnt->options[i]);
            if (pret < 0 || (size_t)pret >= len) {
                ERROR("Sprintf failed");
                free(new_mode);
                goto cleanup;
            }
            free(mode);
            mode = new_mode;
        }
    }
    mp->relabel = mode;
    return mp;
cleanup:
    free(mode);
    free_container_config_v2_common_config_mount_points_element(mp);
    return NULL;
}

int merge_volumes(oci_runtime_spec *oci_spec, char **volumes, size_t volumes_len,
                  container_config_v2_common_config *common_config, parse_mount_cb parse_mount)
{
    int ret = 0;
    size_t new_size = 0, old_size = 0;
    size_t new_mp_key_size, new_mp_val_size, old_mp_key_size, old_mp_val_size;
    size_t i = 0;
    char **mp_key = NULL;
    container_config_v2_common_config_mount_points_element **mp_val = NULL;
    defs_mount **mounts_temp = NULL;
    if (oci_spec == NULL) {
        ret = -1;
        goto out;
    }
    if (volumes_len > LIST_SIZE_MAX - oci_spec->mounts_len) {
        ERROR("Too many volumes to merge, the limit is %d", LIST_SIZE_MAX);
        isulad_set_error_message("Too many volumes to merge, the limit is %d", LIST_SIZE_MAX);
        ret = -1;
        goto out;
    }
    new_size = (oci_spec->mounts_len + volumes_len) * sizeof(defs_mount *);
    old_size = oci_spec->mounts_len * sizeof(defs_mount *);
    ret = mem_realloc((void **)&mounts_temp, new_size, oci_spec->mounts, old_size);
    if (ret != 0) {
        ERROR("Failed to realloc memory volumes");
        ret = -1;
        goto out;
    }
    oci_spec->mounts = mounts_temp;

    if (common_config != NULL) {
        if (common_config->mount_points == NULL) {
            common_config->mount_points = util_common_calloc_s(sizeof(
                                                                   container_config_v2_common_config_mount_points));
            if (common_config->mount_points == NULL) {
                ERROR("Out of memory");
                ret = -1;
                goto out;
            }
        }
        new_mp_key_size = (common_config->mount_points->len + volumes_len) * sizeof(char *);
        old_mp_key_size = common_config->mount_points->len * sizeof(char *);
        new_mp_val_size = (common_config->mount_points->len + volumes_len) *
                          sizeof(container_config_v2_common_config_mount_points_element *);
        old_mp_val_size = common_config->mount_points->len *
                          sizeof(container_config_v2_common_config_mount_points_element *);

        ret = mem_realloc((void **)&mp_key, new_mp_key_size, common_config->mount_points->keys, old_mp_key_size);
        if (ret != 0) {
            ERROR("Failed to realloc memory mount point");
            ret = -1;
            goto out;
        }
        common_config->mount_points->keys = mp_key;
        ret = mem_realloc((void **)&mp_val, new_mp_val_size, common_config->mount_points->values, old_mp_val_size);
        if (ret != 0) {
            ERROR("Failed to realloc memory mount point");
            ret = -1;
            goto out;
        }
        common_config->mount_points->values = mp_val;
    }

    for (i = 0; i < volumes_len; i++) {
        defs_mount *mnt = parse_mount(volumes[i]);
        if (mnt == NULL) {
            ERROR("Failed to parse volume: %s", volumes[i]);
            ret = -1;
            goto out;
        }
        oci_spec->mounts[oci_spec->mounts_len] = mnt;
        oci_spec->mounts_len++;

        if (common_config != NULL) {
            common_config->mount_points->values[common_config->mount_points->len] = defs_mnt_to_mount_point(mnt);
            if (common_config->mount_points->values[common_config->mount_points->len] == NULL) {
                ERROR("Failed to transform to mount point");
                ret = -1;
                goto out;
            }
            common_config->mount_points->keys[common_config->mount_points->len] = util_strdup_s(mnt->destination);
            common_config->mount_points->len++;
        }
    }

out:
    return ret;
}

static int merge_custom_one_device(oci_runtime_spec *oci_spec, const host_config_devices_element *device)
{
    int ret = 0;
    size_t new_size = 0;
    size_t old_size = 0;

    ret = make_sure_oci_spec_linux_resources(oci_spec);
    if (ret < 0) {
        goto out;
    }

    /* malloc for linux->device */
    defs_device **spec_dev = NULL;
    if (oci_spec->linux->devices_len > SIZE_MAX / sizeof(defs_device *) - 1) {
        ERROR("Too many linux devices to merge!");
        ret = -1;
        goto out;
    }
    new_size = (oci_spec->linux->devices_len + 1) * sizeof(defs_device *);
    old_size = new_size - sizeof(defs_device *);
    ret = mem_realloc((void **)&spec_dev, new_size, oci_spec->linux->devices, old_size);
    if (ret != 0) {
        ERROR("Failed to realloc memory for devices");
        ret = -1;
        goto out;
    }

    oci_spec->linux->devices = spec_dev;

    /* malloc for cgroup->device */
    defs_device_cgroup **spec_cgroup_dev = NULL;
    if (oci_spec->linux->resources->devices_len > SIZE_MAX / sizeof(defs_device_cgroup *) - 1) {
        ERROR("Too many cgroup devices to merge!");
        ret = -1;
        goto out;
    }
    new_size = (oci_spec->linux->resources->devices_len + 1) * sizeof(defs_device_cgroup *);
    old_size = new_size - sizeof(defs_device_cgroup *);
    ret = mem_realloc((void **)&spec_cgroup_dev, new_size, oci_spec->linux->resources->devices, old_size);
    if (ret != 0) {
        ERROR("Failed to realloc memory for cgroup devices");
        ret = -1;
        goto out;
    }
    oci_spec->linux->resources->devices = spec_cgroup_dev;


    ret = merge_custom_device(&oci_spec->linux->devices[oci_spec->linux->devices_len],
                              &oci_spec->linux->resources->devices[oci_spec->linux->resources->devices_len], device);
    if (ret != 0) {
        ERROR("Failed to merge custom device");
        ret = -1;
        goto out;
    }
    oci_spec->linux->devices_len++;
    oci_spec->linux->resources->devices_len++;

out:
    return ret;
}


static int merge_custom_devices(oci_runtime_spec *oci_spec, host_config_devices_element **devices, size_t devices_len)
{
    int ret = 0;
    size_t i = 0;
    char *path_on_host = NULL;
    char *cgroup_permissions = NULL;
    char *dir_in_container = NULL;
    host_config_devices_element *device = NULL;

    if (oci_spec == NULL) {
        ret = -1;
        goto out;
    }

    for (i = 0; i < devices_len; i++) {
        device = devices[i];
        path_on_host = device->path_on_host;
        cgroup_permissions = device->cgroup_permissions;
        dir_in_container = device->path_in_container;

        if (util_dir_exists(path_on_host)) {
            ret = merge_all_devices_in_dir(path_on_host, dir_in_container, cgroup_permissions, oci_spec);
            if (ret != 0) {
                ERROR("Failed to merge all devices in directory:%s", path_on_host);
                ret = -1;
                goto out;
            }
        } else {
            ret = merge_custom_one_device(oci_spec, device);
            if (ret != 0) {
                ERROR("Failed to merge device %s", path_on_host);
                ret = -1;
                goto out;
            }
        }
    }

out:
    return ret;
}

static int merge_blkio_weight_device(oci_runtime_spec *oci_spec,
                                     host_config_blkio_weight_device_element **blkio_weight_device,
                                     size_t blkio_weight_device_len)
{
    int ret = 0;
    size_t new_size = 0;
    size_t old_size = 0;
    size_t i = 0;
    defs_block_io_device_weight **weight_device = NULL;

    ret = make_sure_oci_spec_linux_resources_blkio(oci_spec);
    if (ret < 0) {
        goto out;
    }

    if (oci_spec->linux->resources->block_io->weight_device_len > LIST_DEVICE_SIZE_MAX - blkio_weight_device_len) {
        ERROR("Too many weight devices to merge, the limit is %d", LIST_DEVICE_SIZE_MAX);
        isulad_set_error_message("Too many weight devices to merge, the limit is %d", LIST_DEVICE_SIZE_MAX);
        ret = -1;
        goto out;
    }

    new_size = (oci_spec->linux->resources->block_io->weight_device_len + blkio_weight_device_len)
               * sizeof(defs_block_io_device_weight *);
    old_size = oci_spec->linux->resources->block_io->weight_device_len *
               sizeof(defs_block_io_device_weight *);
    ret = mem_realloc((void **)&weight_device, new_size,  oci_spec->linux->resources->block_io->weight_device,
                      old_size);
    if (ret != 0) {
        ERROR("Failed to realloc memory for weight devices");
        ret = -1;
        goto out;
    }
    oci_spec->linux->resources->block_io->weight_device = weight_device;

    for (i = 0; i < blkio_weight_device_len; i++) {
        ret = merge_host_config_blk_weight_device(
                  &oci_spec->linux->resources->block_io->weight_device[oci_spec->
                                                                       linux->resources->block_io->weight_device_len],
                  blkio_weight_device[i]);
        if (ret != 0) {
            ERROR("Failed to merge blkio weight device");
            ret = -1;
            goto out;
        }
        oci_spec->linux->resources->block_io->weight_device_len++;
    }

out:
    return ret;
}

static int merge_blkio_read_bps_device(oci_runtime_spec *oci_spec,
                                       host_config_blkio_device_read_bps_element **blkio_read_bps_device,
                                       size_t throttle_read_bps_device_len)
{
    int ret = 0;
    size_t new_size = 0;
    size_t old_size = 0;
    size_t i = 0;
    defs_block_io_device_throttle **throttle_read_bps_device = NULL;

    ret = make_sure_oci_spec_linux_resources_blkio(oci_spec);
    if (ret < 0) {
        goto out;
    }

    if (oci_spec->linux->resources->block_io->throttle_read_bps_device_len >
        LIST_DEVICE_SIZE_MAX - throttle_read_bps_device_len) {
        ERROR("Too many throttle read bps devices to merge, the limit is %d", LIST_DEVICE_SIZE_MAX);
        isulad_set_error_message("Too many throttle read bps devices devices to merge, the limit is %d",
                                 LIST_DEVICE_SIZE_MAX);
        ret = -1;
        goto out;
    }

    new_size = (oci_spec->linux->resources->block_io->throttle_read_bps_device_len + throttle_read_bps_device_len)
               * sizeof(defs_block_io_device_throttle *);
    old_size = oci_spec->linux->resources->block_io->throttle_read_bps_device_len *
               sizeof(defs_block_io_device_throttle *);
    ret = mem_realloc((void **)&throttle_read_bps_device, new_size,
                      oci_spec->linux->resources->block_io->throttle_read_bps_device, old_size);
    if (ret != 0) {
        ERROR("Failed to realloc memory for blkio throttle read bps devices");
        ret = -1;
        goto out;
    }
    oci_spec->linux->resources->block_io->throttle_read_bps_device = throttle_read_bps_device;

    for (i = 0; i < throttle_read_bps_device_len; i++) {
        ret = merge_host_config_blk_read_bps_device(
                  &oci_spec->linux->resources->block_io->throttle_read_bps_device[
                      oci_spec->linux->resources->block_io->throttle_read_bps_device_len], blkio_read_bps_device[i]);
        if (ret != 0) {
            ERROR("Failed to merge blkio throttle read bps device");
            ret = -1;
            goto out;
        }
        oci_spec->linux->resources->block_io->throttle_read_bps_device_len++;
    }

out:
    return ret;
}

static int merge_blkio_write_bps_device(oci_runtime_spec *oci_spec,
                                        host_config_blkio_device_write_bps_element **blkio_write_bps_device,
                                        size_t throttle_write_bps_device_len)
{
    int ret = 0;
    size_t new_size = 0;
    size_t old_size = 0;
    size_t i = 0;
    defs_block_io_device_throttle **throttle_write_bps_device = NULL;

    ret = make_sure_oci_spec_linux_resources_blkio(oci_spec);
    if (ret < 0) {
        goto out;
    }

    if (oci_spec->linux->resources->block_io->throttle_write_bps_device_len >
        LIST_DEVICE_SIZE_MAX - throttle_write_bps_device_len) {
        ERROR("Too many throttle write bps devices to merge, the limit is %d", LIST_DEVICE_SIZE_MAX);
        isulad_set_error_message("Too many throttle write bps devices devices to merge, the limit is %d",
                                 LIST_DEVICE_SIZE_MAX);
        ret = -1;
        goto out;
    }

    new_size = (oci_spec->linux->resources->block_io->throttle_write_bps_device_len + throttle_write_bps_device_len)
               * sizeof(defs_block_io_device_throttle *);
    old_size = oci_spec->linux->resources->block_io->throttle_write_bps_device_len *
               sizeof(defs_block_io_device_throttle *);
    ret = mem_realloc((void **)&throttle_write_bps_device, new_size,
                      oci_spec->linux->resources->block_io->throttle_write_bps_device, old_size);
    if (ret != 0) {
        ERROR("Failed to realloc memory for throttle write bps devices");
        ret = -1;
        goto out;
    }
    oci_spec->linux->resources->block_io->throttle_write_bps_device = throttle_write_bps_device;

    for (i = 0; i < throttle_write_bps_device_len; i++) {
        ret = merge_host_config_blk_write_bps_device(
                  &oci_spec->linux->resources->block_io->throttle_write_bps_device[
                      oci_spec->linux->resources->block_io->throttle_write_bps_device_len], blkio_write_bps_device[i]);
        if (ret != 0) {
            ERROR("Failed to merge blkio throttle write bps device");
            ret = -1;
            goto out;
        }
        oci_spec->linux->resources->block_io->throttle_write_bps_device_len++;
    }

out:
    return ret;
}

int merge_conf_device(oci_runtime_spec *oci_spec, host_config *host_spec)
{
    int ret = 0;

    /* blkio weight devices */
    if (host_spec->blkio_weight_device != NULL && host_spec->blkio_weight_device_len != 0) {
        ret = merge_blkio_weight_device(oci_spec, host_spec->blkio_weight_device, host_spec->blkio_weight_device_len);
        if (ret != 0) {
            ERROR("Failed to merge blkio weight devices");
            goto out;
        }
    }

    /* blkio throttle read bps devices */
    if (host_spec->blkio_device_read_bps != NULL && host_spec->blkio_device_read_bps_len != 0) {
        ret = merge_blkio_read_bps_device(oci_spec, host_spec->blkio_device_read_bps,
                                          host_spec->blkio_device_read_bps_len);
        if (ret != 0) {
            ERROR("Failed to merge blkio read bps devices");
            goto out;
        }
    }

    /* blkio throttle write bps devices */
    if (host_spec->blkio_device_write_bps != NULL && host_spec->blkio_device_write_bps_len != 0) {
        ret = merge_blkio_write_bps_device(oci_spec, host_spec->blkio_device_write_bps,
                                           host_spec->blkio_device_write_bps_len);
        if (ret != 0) {
            ERROR("Failed to merge blkio write bps devices");
            goto out;
        }
    }

    /* devices which will be populated into container */
    if (host_spec->devices  != NULL && host_spec->devices_len != 0) {
        /* privileged containers will populate all devices in host */
        if (!host_spec->privileged) {
            ret = merge_custom_devices(oci_spec, host_spec->devices, host_spec->devices_len);
            if (ret != 0) {
                ERROR("Failed to merge custom devices");
                goto out;
            }
        } else {
            INFO("Skipped \"--device\" due to conflict with \"--privileged\"");
        }
    }

out:
    return ret;
}

static bool mounts_expand(oci_runtime_spec *container, size_t add_len)
{
    defs_mount **tmp_mount = NULL;
    int ret = 0;
    size_t old_len = container->mounts_len;
    if (add_len > SIZE_MAX / sizeof(defs_mount *) - old_len) {
        ERROR("Too many mount elements!");
        return false;
    }
    ret = mem_realloc((void **)&tmp_mount, (old_len + add_len) * sizeof(defs_mount *),
                      container->mounts, old_len * sizeof(defs_mount *));
    if (ret < 0) {
        ERROR("memory realloc failed for mount array expand");
        return false;
    }
    container->mounts = tmp_mount;
    container->mounts_len = old_len + add_len;

    return true;
}

static bool mount_file(oci_runtime_spec *container, const char *src_path, const char *dst_path)
{
    char **options = NULL;
    size_t options_len = 2;
    bool ret = false;
    defs_mount *tmp_mounts = NULL;

    /* mount options */
    if (options_len > SIZE_MAX / sizeof(char *)) {
        ERROR("Options len is too long!");
        goto out_free;
    }
    options = util_common_calloc_s(options_len * sizeof(char *));
    if (options == NULL) {
        ERROR("Out of memory");
        goto out_free;
    }
    options[0] = util_strdup_s("rbind");
    options[1] = util_strdup_s("rprivate");
    /* generate mount node */
    tmp_mounts = util_common_calloc_s(sizeof(defs_mount));
    if (tmp_mounts == NULL) {
        ERROR("Malloc tmp_mounts memory failed");
        goto out_free;
    }

    tmp_mounts->destination = util_strdup_s(dst_path);
    tmp_mounts->source = util_strdup_s(src_path);
    tmp_mounts->type = util_strdup_s("bind");
    tmp_mounts->options = options;
    tmp_mounts->options_len = options_len;
    options = NULL;

    /* expand mount array */
    if (!mounts_expand(container, 1)) {
        goto out_free;
    }
    /* add a new mount node */
    container->mounts[container->mounts_len - 1] = tmp_mounts;

    ret = true;
out_free:

    if (!ret) {
        util_free_array_by_len(options, options_len);
        free_defs_mount(tmp_mounts);
    }
    return ret;
}

static bool add_host_channel_mount(oci_runtime_spec *container, const host_config_host_channel *host_channel)
{
    char **options = NULL;
    size_t options_len = 3;
    bool ret = false;
    defs_mount *tmp_mounts = NULL;

    if (options_len > SIZE_MAX / sizeof(char *)) {
        ERROR("Invalid option size");
        return ret;
    }
    options = util_common_calloc_s(options_len * sizeof(char *));
    if (options == NULL) {
        ERROR("Out of memory");
        goto out_free;
    }
    options[0] = util_strdup_s("rbind");
    options[1] = util_strdup_s("rprivate");
    options[2] = util_strdup_s(host_channel->permissions);
    /* generate mount node */
    tmp_mounts = util_common_calloc_s(sizeof(defs_mount));
    if (tmp_mounts == NULL) {
        ERROR("Malloc tmp_mounts memory failed");
        goto out_free;
    }

    tmp_mounts->destination = util_strdup_s(host_channel->path_in_container);
    tmp_mounts->source = util_strdup_s(host_channel->path_on_host);
    tmp_mounts->type = util_strdup_s("bind");
    tmp_mounts->options = options;
    tmp_mounts->options_len = options_len;
    options = NULL;

    /* expand mount array */
    if (!mounts_expand(container, 1)) {
        goto out_free;
    }
    /* add a new mount node */
    container->mounts[container->mounts_len - 1] = tmp_mounts;

    ret = true;
out_free:

    if (!ret) {
        util_free_array_by_len(options, options_len);
        free_defs_mount(tmp_mounts);
    }
    return ret;
}

static int change_dev_shm_size(oci_runtime_spec *oci_spec, host_config *host_spec)
{
    size_t i = 0;
    size_t j = 0;
    char size_opt[MOUNT_PROPERTIES_SIZE] = { 0 };
    char *tmp = NULL;

    if (is_none(host_spec->ipc_mode)) {
        return 0;
    }

    int nret = snprintf(size_opt, sizeof(size_opt), "size=%lld", (long long int)host_spec->shm_size);
    if (nret < 0 || (size_t)nret >= sizeof(size_opt)) {
        ERROR("Out of memory");
        return -1;
    }

    if (oci_spec->mounts == NULL) {
        return -1;
    }

    for (i = 0; i < oci_spec->mounts_len; i++) {
        if (strcmp("/dev/shm", oci_spec->mounts[i]->destination) != 0) {
            continue;
        }

        for (j = 0; j < oci_spec->mounts[i]->options_len; j++) {
            // change dev shm mount size
            if (strncmp("size=", oci_spec->mounts[i]->options[j], strlen("size=")) == 0) {
                tmp = oci_spec->mounts[i]->options[j];
                oci_spec->mounts[i]->options[j] = util_strdup_s(size_opt);
                free(tmp);
                tmp = NULL;
                return 0;
            }
        }
    }

    ERROR("/dev/shm mount point not exist");
    return -1;
}

static inline bool is_mount_destination_hosts(const char *destination)
{
    return strcmp(destination, "/etc/hosts") == 0;
}

static inline bool is_mount_destination_resolv(const char *destination)
{
    return strcmp(destination, "/etc/resolv.conf") == 0;
}

static inline bool is_mount_destination_hostname(const char *destination)
{
    return strcmp(destination, "/etc/hostname") == 0;
}


/* find whether mount entry with destination "/etc/hostname" "/etc/hosts" "/etc/resolv.conf" exits in mount
 * if not exists: append mounts to ocispec by v2_spec
 * if exists: replace the source in v2_spec
 */
static int append_network_files_mounts(oci_runtime_spec *oci_spec, host_config *host_spec,
                                       container_config_v2_common_config *v2_spec)
{
    int ret = 0;
    size_t i = 0;
    bool has_hosts_mount = false;
    bool has_resolv_mount = false;
    bool has_hostname_mount = false;
    bool share = is_container(host_spec->network_mode);

    for (i = 0; i < oci_spec->mounts_len; i++) {
        if (is_mount_destination_hosts(oci_spec->mounts[i]->destination)) {
            has_hosts_mount = true;
            free(v2_spec->hosts_path);
            v2_spec->hosts_path = util_strdup_s(oci_spec->mounts[i]->source);
        }
        if (is_mount_destination_resolv(oci_spec->mounts[i]->destination)) {
            has_resolv_mount = true;
            free(v2_spec->resolv_conf_path);
            v2_spec->resolv_conf_path = util_strdup_s(oci_spec->mounts[i]->source);
        }
        if (is_mount_destination_hostname(oci_spec->mounts[i]->destination)) {
            has_hostname_mount = true;
            free(v2_spec->hostname_path);
            v2_spec->hostname_path = util_strdup_s(oci_spec->mounts[i]->source);
        }
    }

    if (!util_file_exists(v2_spec->hosts_path)) {
        WARN("HostsPath set to %s, but can't stat this filename (err = %v); skipping", v2_spec->resolv_conf_path);
    } else {
        /* add network config files */
        if (!has_hosts_mount) {
            if (relabel(v2_spec->hosts_path, v2_spec->mount_label, share) != 0) {
                ERROR("Error to relabel hosts path: %s", v2_spec->hosts_path);
                ret = -1;
                goto out;
            }
            if (!mount_file(oci_spec, v2_spec->hosts_path, ETC_HOSTS)) {
                ERROR("Merge hosts mount failed");
                ret = -1;
                goto out;
            }
        }
    }
    if (!util_file_exists(v2_spec->resolv_conf_path)) {
        WARN("ResolvConfPath set to %s, but can't stat this filename (err = %v); skipping", v2_spec->resolv_conf_path);
    } else {
        if (!has_resolv_mount) {
            if (relabel(v2_spec->resolv_conf_path, v2_spec->mount_label, share) != 0) {
                ERROR("Error to relabel resolv.conf path: %s", v2_spec->resolv_conf_path);
                ret = -1;
                goto out;
            }
            if (!mount_file(oci_spec, v2_spec->resolv_conf_path, RESOLV_CONF_PATH)) {
                ERROR("Merge resolv.conf mount failed");
                ret = -1;
                goto out;
            }
        }
    }

    if (!util_file_exists(v2_spec->hostname_path)) {
        WARN("HostnamePath set to %s, but can't stat this filename (err = %v); skipping", v2_spec->resolv_conf_path);
    } else {
        if (!has_hostname_mount) {
            if (relabel(v2_spec->hostname_path, v2_spec->mount_label, share) != 0) {
                ERROR("Error to relabel hostname path: %s", v2_spec->hostname_path);
                return -1;
            }
            if (!mount_file(oci_spec, v2_spec->hostname_path, ETC_HOSTNAME)) {
                ERROR("Merge hostname mount failed");
                ret = -1;
                goto out;
            }
        }
    }

out:
    return ret;
}

static int chown_for_shm(const char *shm_path, const char *user_remap)
{
    unsigned int host_uid = 0;
    unsigned int host_gid = 0;
    unsigned int size = 0;

    if (shm_path == NULL) {
        return 0;
    }

    if (user_remap != NULL) {
        if (util_parse_user_remap(user_remap, &host_uid, &host_gid, &size)) {
            ERROR("Failed to split string '%s'.", user_remap);
            return -1;
        }
        if (chown(shm_path, host_uid, host_gid) != 0) {
            ERROR("Failed to chown host path '%s'.", shm_path);
            return -1;
        }
    }
    return 0;
}

static char *get_prepare_share_shm_path(const char *truntime, const char *cid)
{
#define SHM_MOUNT_FILE_NAME "/mounts/shm/"
    char *c_root_path = NULL;
    size_t slen = 0;
    char *spath = NULL;
    int nret = 0;

    if (truntime == NULL) {
        truntime = "lcr";
    }
    c_root_path = conf_get_routine_rootdir(truntime);
    if (c_root_path == NULL) {
        goto err_out;
    }

    // c_root_path + "/" + cid + "/mounts/shm"
    if (strlen(c_root_path) > (((PATH_MAX - strlen(cid)) - 1) - strlen(SHM_MOUNT_FILE_NAME)) - 1) {
        ERROR("Too large path");
        goto err_out;
    }
    slen = strlen(c_root_path) + 1 + strlen(cid) + strlen(SHM_MOUNT_FILE_NAME) + 1;
    spath = util_smart_calloc_s(sizeof(char), slen);
    if (spath == NULL) {
        ERROR("Out of memory");
        goto err_out;
    }

    nret = sprintf(spath, "%s/%s/mounts/shm/", c_root_path, cid);
    if (nret < 0) {
        ERROR("Sprintf failed");
        goto err_out;
    }

    free(c_root_path);
    return spath;

err_out:
    free(spath);
    free(c_root_path);
    return NULL;
}

static bool has_mount_shm(host_config *host_spec, container_config_v2_common_config *v2_spec)
{
    container_t *cont = NULL;
    bool ret = false;

    cont = util_common_calloc_s(sizeof(container_t));
    if (cont == NULL) {
        ERROR("Out of memory");
        goto out;
    }
    cont->common_config = v2_spec;
    cont->hostconfig = host_spec;

    ret = has_mount_for(cont, "/dev/shm");

    cont->common_config = NULL;
    cont->hostconfig = NULL;
out:
    free(cont);
    return ret;
}

static int prepare_share_shm(oci_runtime_spec *oci_spec, host_config *host_spec,
                             container_config_v2_common_config *v2_spec)
{
#define MAX_PROPERTY_LEN 64
    char shmproperty[MAX_PROPERTY_LEN] = {0};
    int ret = -1;
    int nret = 0;
    bool has_mount = false;
    char *spath = NULL;

    // has mount for /dev/shm
    if (has_mount_shm(host_spec, v2_spec)) {
        return 0;
    }

    spath = get_prepare_share_shm_path(host_spec->runtime, v2_spec->id);
    if (spath == NULL) {
        goto out;
    }

    nret = util_mkdir_p(spath, 0700);
    if (nret != 0) {
        ERROR("Build shm dir failed");
        goto out;
    }
    nret = sprintf(shmproperty, "mode=1777,size=%"PRId64, host_spec->shm_size);
    if (nret < 0) {
        ERROR("Sprintf failed");
        goto out;
    }

    nret = mount("shm", spath, "tmpfs", MS_NOEXEC | MS_NODEV | MS_NOSUID, shmproperty);
    if (nret < 0) {
        ERROR("Mount %s failed: %s", spath, strerror(errno));
        goto out;
    }
    has_mount = true;

    nret = chown_for_shm(spath, host_spec->user_remap);
    if (nret != 0) {
        goto out;
    }

    v2_spec->shm_path = spath;
    spath = NULL;
    ret = 0;
out:
    if (ret != 0 && has_mount) {
        (void)umount(spath);
    }
    free(spath);
    return ret;
}

static bool add_shm_mount(oci_runtime_spec *container, const char *shm_path)
{
    char **options = NULL;
    size_t options_len = 3;
    bool ret = false;
    defs_mount *tmp_mounts = NULL;

    if (options_len > SIZE_MAX / sizeof(char *)) {
        ERROR("Invalid option size");
        return ret;
    }
    options = util_common_calloc_s(options_len * sizeof(char *));
    if (options == NULL) {
        ERROR("Out of memory");
        goto out_free;
    }
    options[0] = util_strdup_s("rbind");
    options[1] = util_strdup_s("rprivate");
    // default shm size is 64MB
    options[2] = util_strdup_s("size=65536k");
    /* generate mount node */
    tmp_mounts = util_common_calloc_s(sizeof(defs_mount));
    if (tmp_mounts == NULL) {
        ERROR("Malloc tmp_mounts memory failed");
        goto out_free;
    }

    tmp_mounts->destination = util_strdup_s("/dev/shm");
    tmp_mounts->source = util_strdup_s(shm_path);
    tmp_mounts->type = util_strdup_s("bind");
    tmp_mounts->options = options;
    tmp_mounts->options_len = options_len;
    options = NULL;

    /* expand mount array */
    if (!mounts_expand(container, 1)) {
        goto out_free;
    }
    /* add a new mount node */
    container->mounts[container->mounts_len - 1] = tmp_mounts;

    ret = true;
out_free:

    if (!ret) {
        util_free_array_by_len(options, options_len);
        free_defs_mount(tmp_mounts);
    }
    return ret;
}

#define SHM_MOUNT_POINT "/dev/shm"
static int setup_ipc_dirs(oci_runtime_spec *oci_spec, host_config *host_spec,
                          container_config_v2_common_config *v2_spec)
{
    int ret = 0;
    container_t *cont = NULL;
    char *tmp_cid = NULL;
    char *right_path = NULL;

    // ignore shm of system container
    if (host_spec->system_container) {
        return 0;
    }
    // setup shareable dirs
    if (host_spec->ipc_mode == NULL || is_shareable(host_spec->ipc_mode)) {
        return prepare_share_shm(oci_spec, host_spec, v2_spec);
    }

    if (is_container(host_spec->ipc_mode)) {
        tmp_cid = connected_container(host_spec->ipc_mode);
        cont = containers_store_get(tmp_cid);
        if (cont == NULL) {
            ERROR("Invalid share path: %s", host_spec->ipc_mode);
            ret = -1;
            goto out;
        }
        right_path = util_strdup_s(cont->common_config->shm_path);
        container_unref(cont);
    } else if (is_host(host_spec->ipc_mode)) {
        if (!util_file_exists(SHM_MOUNT_POINT)) {
            ERROR("/dev/shm is not mounted, but must be for --ipc=host");
            ret = -1;
            goto out;
        }
        right_path = util_strdup_s(SHM_MOUNT_POINT);
    }

    free(v2_spec->shm_path);
    v2_spec->shm_path = right_path;
out:
    free(tmp_cid);
    return ret;
}

int merge_conf_mounts(oci_runtime_spec *oci_spec, host_config *host_spec,
                      container_config_v2_common_config *v2_spec)
{
    int ret = 0;
    container_config *container_spec = v2_spec->config;

    /* volumes to mount */
    if (host_spec->binds != NULL && host_spec->binds_len) {
        ret = merge_volumes(oci_spec, host_spec->binds,
                            host_spec->binds_len, v2_spec,
                            parse_volume);
        if (ret) {
            ERROR("Failed to merge volumes");
            goto out;
        }
    }

    /* host channel to mount */
    if (host_spec->host_channel != NULL) {
        if (!add_host_channel_mount(oci_spec, host_spec->host_channel)) {
            ERROR("Failed to merge host channel mount");
            goto out;
        }
    }

    /* mounts to mount filesystem */
    if (container_spec->mounts && container_spec->mounts_len) {
        ret = merge_volumes(oci_spec, container_spec->mounts,
                            container_spec->mounts_len, v2_spec,
                            parse_mount);
        if (ret) {
            ERROR("Failed to merge mounts");
            goto out;
        }
    }

    if (host_spec->shm_size == 0) {
        host_spec->shm_size = DEFAULT_SHM_SIZE;
    }

    /* setup ipc dir */
    if (setup_ipc_dirs(oci_spec, host_spec, v2_spec) != 0) {
        ret = -1;
        goto out;
    }

    /* add ipc mount */
    if (v2_spec->shm_path != NULL) {
        // check whether duplication
        add_shm_mount(oci_spec, v2_spec->shm_path);
    }

    if (host_spec->shm_size > 0) {
        ret = change_dev_shm_size(oci_spec, host_spec);
        if (ret) {
            ERROR("Failed to set dev shm size");
            goto out;
        }
    }

    if (!host_spec->system_container) {
        ret = append_network_files_mounts(oci_spec, host_spec, v2_spec);
        if (ret) {
            ERROR("Failed to append network mounts");
            goto out;
        }
    }

out:
    return ret;
}

int add_rootfs_mount(const container_config *container_spec)
{
    int ret = 0;
    char *mntparent = NULL;

    mntparent = conf_get_isulad_mount_rootfs();
    if (mntparent == NULL) {
        ERROR("Failed to get mount parent directory");
        ret = -1;
        goto out;
    }
    if (append_json_map_string_string(container_spec->annotations, "rootfs.mount", mntparent)) {
        ERROR("Realloc annotatinons failed");
        ret = -1;
        goto out;
    }

out:
    free(mntparent);
    return ret;
}


