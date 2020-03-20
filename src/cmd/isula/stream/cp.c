/******************************************************************************
 * Copyright (c) Huawei Technologies Co., Ltd. 2019. All rights reserved.
 * iSulad licensed under the Mulan PSL v1.
 * You can use this software according to the terms and conditions of the Mulan PSL v1.
 * You may obtain a copy of Mulan PSL v1 at:
 *     http://license.coscl.org.cn/MulanPSL
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND, EITHER EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR
 * PURPOSE.
 * See the Mulan PSL v1 for more details.
 * Author: tanyifeng
 * Create: 2019-04-17
 * Description: provide container cp functions
 ******************************************************************************/
#include <limits.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "error.h"
#include "cp.h"
#include "arguments.h"
#include "log.h"
#include "path.h"
#include "isula_connect.h"
#include "libtar.h"

#define FromContainer 0x01u
#define ToContainer 0x10u
#define AcrossContainers 0x11u

const char g_cmd_cp_desc[] = "Copy files/folders between a container and the local filesystem";

const char g_cmd_cp_usage[] = "cp [OPTIONS] CONTAINER:SRC_PATH DEST_PATH\n"
                              "  cp [OPTIONS] SRC_PATH CONTAINER:DEST_PATH";

struct client_arguments g_cmd_cp_args = {};

static char *resolve_local_path(const char *path)
{
    char abs_path[PATH_MAX] = { 0 };

    if (cleanpath(path, abs_path, sizeof(abs_path)) == NULL) {
        ERROR("Failed to clean path");
        return NULL;
    }

    return preserve_trailing_dot_or_separator(abs_path, path);
}

static void print_copy_from_container_error(const char *ops_err, const char *archive_err, int ret,
                                            const struct client_arguments *args)
{
    if (ops_err != NULL) {
        if (strcmp(ops_err, errno_to_error_message(ISULAD_ERR_CONNECT)) == 0) {
            COMMAND_ERROR("%s", ops_err);
        } else {
            client_print_error(0, ISULAD_ERR_EXEC, ops_err);
        }
    } else if (archive_err != NULL) {
        COMMAND_ERROR("Failed exact archive: %s", archive_err);
    } else if (ret != 0) {
        COMMAND_ERROR("Failed to copy from container");
    }
}

static int client_copy_from_container(const struct client_arguments *args, const char *id, const char *srcpath,
                                      const char *destpath)
{
    isula_connect_ops *ops = NULL;
    struct isula_copy_from_container_request request = { 0 };
    struct isula_copy_from_container_response *response = NULL;
    int ret = 0;
    int nret = 0;
    char *archive_err = NULL;
    char *ops_err = NULL;
    char *resolved = NULL;
    struct archive_copy_info *srcinfo = NULL;
    client_connect_config_t config;

    ops = get_connect_client_ops();
    if (ops == NULL || !ops->container.copy_from_container) {
        COMMAND_ERROR("Unimplemented copy from container operation");
        return -1;
    }

    response = util_common_calloc_s(sizeof(struct isula_copy_from_container_response));
    if (response == NULL) {
        ERROR("Event: Out of memory");
        return -1;
    }

    request.id = (char *)id;
    request.runtime = args->runtime;
    request.srcpath = (char *)srcpath;

    config = get_connect_config(args);
    ret = ops->container.copy_from_container(&request, response, &config);
    if (ret) {
        ops_err = (response->errmsg != NULL) ? util_strdup_s(response->errmsg) : NULL;
        goto out;
    }

    resolved = resolve_local_path(destpath);
    if (resolved == NULL) {
        ret = -1;
        goto out;
    }

    srcinfo = util_common_calloc_s(sizeof(struct archive_copy_info));
    if (srcinfo == NULL) {
        ret = -1;
        goto out;
    }
    srcinfo->exists = true;
    srcinfo->path = util_strdup_s(srcpath);
    srcinfo->isdir = S_ISDIR(response->stat->mode);

    nret = archive_copy_to(&response->reader, false, srcinfo, resolved, &archive_err);
    if (nret != 0) {
        ret = nret;
    }

    nret = response->reader.close(response->reader.context, &ops_err);
    if (nret != 0) {
        ret = nret;
    }

out:
    print_copy_from_container_error(ops_err, archive_err, ret, args);
    free(resolved);
    free(archive_err);
    free(ops_err);
    free_archive_copy_info(srcinfo);
    isula_copy_from_container_response_free(response);
    return ret;
}

static void print_copy_to_container_error(const struct isula_copy_to_container_response *response,
                                          const char *archive_err, int ret, const struct client_arguments *args)
{
    if (response->cc != ISULAD_SUCCESS || response->errmsg != NULL || response->server_errono != 0) {
        client_print_error(response->cc, response->server_errono, response->errmsg);
    } else if (archive_err) {
        COMMAND_ERROR("Failed to archive: %s", archive_err);
    } else if (ret != 0) {
        COMMAND_ERROR("Can not copy to container");
    }
}

static int client_copy_to_container(const struct client_arguments *args, const char *id, const char *srcpath,
                                    const char *destpath)
{
    isula_connect_ops *ops = NULL;
    struct isula_copy_to_container_request request = { 0 };
    struct isula_copy_to_container_response *response = NULL;
    int ret = 0;
    int nret = 0;
    char *archive_err = NULL;
    char *resolved = NULL;
    struct archive_copy_info *srcinfo = NULL;
    struct io_read_wrapper archive_reader = { 0 };
    client_connect_config_t config = { 0 };

    ops = get_connect_client_ops();
    if (ops == NULL || !ops->container.copy_to_container) {
        COMMAND_ERROR("Unimplemented copy to container operation");
        return -1;
    }

    response = util_common_calloc_s(sizeof(struct isula_copy_to_container_response));
    if (response == NULL) {
        ERROR("Event: Out of memory");
        return -1;
    }

    request.id = (char *)id;
    request.runtime = args->runtime;
    request.dstpath = (char *)destpath;

    resolved = resolve_local_path(srcpath);
    if (resolved == NULL) {
        ret = -1;
        goto out;
    }

    srcinfo = copy_info_source_path(resolved, false, &archive_err);
    if (srcinfo == NULL) {
        ret = -1;
        goto out;
    }

    nret = tar_resource(srcinfo, &archive_reader, &archive_err);
    if (nret != 0) {
        ret = -1;
        goto out;
    }

    request.srcpath = srcinfo->path;
    request.srcisdir = srcinfo->isdir;
    request.srcrebase = srcinfo->rebase_name;
    request.reader.context = archive_reader.context;
    request.reader.read = archive_reader.read;
    request.reader.close = archive_reader.close;

    config = get_connect_config(args);
    ret = ops->container.copy_to_container(&request, response, &config);
    if (ret) {
        goto out;
    }

    nret = archive_reader.close(archive_reader.context, &archive_err);
    if (nret < 0) {
        ret = nret;
    }

out:
    print_copy_to_container_error(response, archive_err, ret, args);
    free(resolved);
    free(archive_err);
    free_archive_copy_info(srcinfo);
    isula_copy_to_container_response_free(response);
    return ret;
}

static void ignore_sigpipe()
{
    struct sigaction sa;

    /*
     * Ignore SIGPIPE so the current process still exists after child process exited.
     */
    (void)memset(&sa, 0, sizeof(struct sigaction));

    sa.sa_handler = SIG_IGN;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;

    if (sigaction(SIGPIPE, &sa, NULL) < 0) {
        WARN("Failed to ignore SIGPIPE");
    }
}

static int client_run_copy(const struct client_arguments *args, const char *source, const char *destination)
{
    int ret = 0;
    char *dup_src = NULL;
    char *dup_dest = NULL;
    char *colon = NULL;
    char *src_container = NULL;
    char *src_path = NULL;
    char *dest_container = NULL;
    char *dest_path = NULL;
    char *container = NULL;
    unsigned int direction = 0;

    ignore_sigpipe();

    dup_src = util_strdup_s(source);
    src_path = dup_src;
    colon = strchr(dup_src, ':');
    if (colon != NULL) {
        *colon++ = '\0';
        src_container = dup_src;
        src_path = colon;
    }

    dup_dest = util_strdup_s(destination);
    dest_path = dup_dest;
    colon = strchr(dup_dest, ':');
    if (colon != NULL) {
        *colon++ = '\0';
        dest_container = dup_dest;
        dest_path = colon;
    }

    if (src_container != NULL && src_container[0] != '\0') {
        direction |= FromContainer;
        container = src_container;
    }
    if (dest_container != NULL && dest_container[0] != '\0') {
        direction |= ToContainer;
        container = dest_container;
    }

    if (direction == FromContainer) {
        ret = client_copy_from_container(args, container, src_path, dest_path);
        goto cleanup;
    }

    if (direction == ToContainer) {
        ret = client_copy_to_container(args, container, src_path, dest_path);
        goto cleanup;
    }

    if (direction == AcrossContainers) {
        COMMAND_ERROR("copying between containers is not supported");
        goto cleanup;
    }

    COMMAND_ERROR("must specify at least one container source");

cleanup:
    free(dup_src);
    free(dup_dest);
    return ret;
}

int cmd_cp_main(int argc, const char **argv)
{
    struct log_config lconf = { 0 };
    command_t cmd;

    set_default_command_log_config(argv[0], &lconf);
    if (client_arguments_init(&g_cmd_cp_args)) {
        COMMAND_ERROR("client arguments init failed\n");
        exit(ECOMMON);
    }
    g_cmd_cp_args.progname = argv[0];
    struct command_option options[] = { LOG_OPTIONS(lconf), COMMON_OPTIONS(g_cmd_cp_args) };

    command_init(&cmd, options, sizeof(options) / sizeof(options[0]), argc, (const char **)argv, g_cmd_cp_desc,
                 g_cmd_cp_usage);
    if (command_parse_args(&cmd, &g_cmd_cp_args.argc, &g_cmd_cp_args.argv)) {
        exit(EINVALIDARGS);
    }
    if (log_init(&lconf)) {
        COMMAND_ERROR("cp: log init failed");
        exit(ECOMMON);
    }

    if (g_cmd_cp_args.argc != 2) {
        COMMAND_ERROR("\"%s cp\" requires exactly 2 arguments.", g_cmd_cp_args.progname);
        exit(ECOMMON);
    }

    if (g_cmd_cp_args.socket == NULL) {
        COMMAND_ERROR("Missing --host,-H option");
        exit(EINVALIDARGS);
    }

    if (client_run_copy(&g_cmd_cp_args, g_cmd_cp_args.argv[0], g_cmd_cp_args.argv[1]) != 0) {
        exit(ECOMMON);
    }

    exit(EXIT_SUCCESS);
}

