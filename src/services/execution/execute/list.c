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
 * Description: provide container list callback function definition
 ********************************************************************************/

#include <stdio.h>
#include <unistd.h>

#include "log.h"
#include "containers_store.h"
#include "timestamp.h"
#include "list.h"
#include "filters.h"
#include "utils.h"
#include "error.h"

struct list_context {
    struct filters_args *ps_filters;
    container_list_request *list_config;
};

static int dup_container_list_request(const container_list_request *src, container_list_request **dest)
{
    int ret = -1;
    char *json = NULL;
    parser_error err = NULL;

    if (src == NULL) {
        *dest = NULL;
        return 0;
    }

    json = container_list_request_generate_json(src, NULL, &err);
    if (json == NULL) {
        ERROR("Failed to generate json: %s", err);
        goto out;
    }
    *dest = container_list_request_parse_data(json, NULL, &err);
    if (*dest == NULL) {
        ERROR("Failed to parse json: %s", err);
        goto out;
    }
    ret = 0;

out:
    free(err);
    free(json);
    return ret;
}

static void free_list_context(struct list_context *ctx)
{
    if (ctx == NULL) {
        return;
    }
    filters_args_free(ctx->ps_filters);
    ctx->ps_filters = NULL;
    free_container_list_request(ctx->list_config);
    ctx->list_config = NULL;
    free(ctx);
}

static struct list_context *list_context_new(const container_list_request *request)
{
    struct list_context *ctx = NULL;

    ctx = util_common_calloc_s(sizeof(struct list_context));
    if (ctx == NULL) {
        ERROR("Out of memory");
        return NULL;
    }
    ctx->ps_filters = filters_args_new();
    if (ctx->ps_filters == NULL) {
        ERROR("Out of memory");
        goto cleanup;
    }
    if (dup_container_list_request(request, &ctx->list_config) != 0) {
        ERROR("Failed to dup list request");
        goto cleanup;
    }
    return ctx;
cleanup:
    free_list_context(ctx);
    return NULL;
}

static const char *accepted_ps_filter_tags[] = {
    "id",
    "label",
    "name",
    "status",
    NULL
};

static int filter_by_name(const struct list_context *ctx, const map_t *map_id_name, const map_t *matches, bool idsearch)
{
    int ret = 0;
    bool default_value = true;

    if (ctx == NULL || map_id_name == NULL) {
        return -1;
    }

    map_itor *itor = map_itor_new(map_id_name);
    if (itor == NULL) {
        ERROR("Out of memory");
        ret = -1;
        goto out;
    }

    for (; map_itor_valid(itor); map_itor_next(itor)) {
        void *id = map_itor_key(itor);
        const char *name = map_itor_value(itor);
        if (idsearch && map_search(matches, id) == NULL) {
            continue;
        }
        if (filters_args_match(ctx->ps_filters, "name", name)) {
            if (!map_replace(matches, id, &default_value)) {
                ERROR("Failed to insert");
                map_itor_free(itor);
                ret = -1;
                goto out;
            }
        }
    }
    map_itor_free(itor);

out:
    return ret;
}


static int append_ids(const map_t *matches, char ***filtered_ids)
{
    map_itor *itor = map_itor_new(matches);
    if (itor == NULL) {
        ERROR("Out of memory");
        return -1;
    }

    for (; map_itor_valid(itor); map_itor_next(itor)) {
        if (util_array_append(filtered_ids, map_itor_key(itor)) != 0) {
            ERROR("Append failed");
            util_free_array(*filtered_ids);
            *filtered_ids = NULL;
            map_itor_free(itor);
            return -1;
        }
    }
    map_itor_free(itor);
    return 0;
}

static int insert_matched_id(char **ids, map_t *matches, void *value, size_t ids_len)
{
    size_t i;

    for (i = 0; i < ids_len; i++) {
        container_t *cont = containers_store_get_by_prefix(ids[i]);
        if (cont != NULL) {
            bool inserted;
            inserted = map_insert(matches, cont->common_config->id, value);
            container_unref(cont);
            if (!inserted) {
                ERROR("Insert map failed: %s", ids[i]);
                return -1;
            }
        }
    }
    return 0;
}

static inline void set_idsearch(size_t ids_len, bool *value)
{
    if (ids_len > 0) {
        *value = true;
    }
}

static char **filter_by_name_id_matches(const struct list_context *ctx, const map_t *map_id_name)
{
    int ret = 0;
    size_t names_len, ids_len;
    bool idsearch = false;
    bool default_value = true;
    char **names = NULL;
    char **ids = NULL;
    char **filtered_ids = NULL;
    map_t *matches = NULL;

    names = filters_args_get(ctx->ps_filters, "name");
    names_len = util_array_len((const char **)names);

    ids = filters_args_get(ctx->ps_filters, "id");
    ids_len = util_array_len((const char **)ids);
    if (names_len == 0 && ids_len == 0) {
        if (append_ids(map_id_name, &filtered_ids) != 0) {
            goto cleanup;
        }
        return filtered_ids;
    }

    set_idsearch(ids_len, &idsearch);

    matches = map_new(MAP_STR_BOOL, MAP_DEFAULT_CMP_FUNC, MAP_DEFAULT_FREE_FUNC);
    if (matches == NULL) {
        ERROR("Out of memory");
        goto cleanup;
    }


    if (insert_matched_id(ids, matches, &default_value, ids_len) != 0) {
        goto cleanup;
    }

    if (names_len > 0) {
        ret = filter_by_name(ctx, map_id_name, matches, idsearch);
        if (ret != 0) {
            goto cleanup;
        }
    }

    if (map_size(matches) > 0) {
        if (append_ids(map_id_name, &filtered_ids) != 0) {
            goto cleanup;
        }
    }

cleanup:
    util_free_array(ids);
    util_free_array(names);
    map_free(matches);
    return filtered_ids;
}


int dup_json_map_string_string(const json_map_string_string *src, json_map_string_string *dest)
{
    int ret = 0;
    size_t i;

    if (src->len == 0) {
        return 0;
    }

    if (src->len > SIZE_MAX / sizeof(char *)) {
        ERROR("Container inspect container config is too much!");
        ret = -1;
        goto out;
    }
    dest->keys = util_common_calloc_s(src->len * sizeof(char *));
    if (dest->keys == NULL) {
        ERROR("Out of memory");
        ret = -1;
        goto out;
    }
    dest->values = util_common_calloc_s(src->len * sizeof(char *));
    if (dest->values == NULL) {
        ERROR("Out of memory");
        free(dest->keys);
        dest->keys = NULL;
        ret = -1;
        goto out;
    }
    for (i = 0; i < src->len; i++) {
        dest->keys[i] = util_strdup_s(src->keys[i] ? src->keys[i] : "");
        dest->values[i] = util_strdup_s(src->values[i] ? src->values[i] : "");
        dest->len++;
    }
out:
    return ret;
}

char *container_get_health_state(const container_config_v2_state *cont_state)
{
    if (cont_state == NULL || cont_state->health == NULL || cont_state->health->status == NULL) {
        return NULL;
    }

    if (strcmp(cont_state->health->status, HEALTH_STARTING) == 0) {
        return util_strdup_s("health: starting");
    }

    return util_strdup_s(cont_state->health->status);
}

static int replace_labels(container_container *isuladinfo, json_map_string_string *labels, const map_t *map_labels)
{
    isuladinfo->labels = util_common_calloc_s(sizeof(json_map_string_string));

    if (isuladinfo->labels == NULL) {
        ERROR("Out of memory");
        return -1;
    }

    if (dup_json_map_string_string(labels, isuladinfo->labels) != 0) {
        ERROR("Failed to dup labels");
        return -1;
    }
    size_t i;
    for (i = 0; i < labels->len; i++) {
        if (!map_replace(map_labels, (void *)labels->keys[i], labels->values[i])) {
            ERROR("Failed to insert labels to map");
            return -1;
        }
    }
    return 0;
}

static int replace_annotations(const container_config_v2_common_config *common_config,
                               container_container *isuladinfo)
{
    if (common_config->config->annotations != NULL &&
        common_config->config->annotations->len != 0) {
        isuladinfo->annotations = util_common_calloc_s(sizeof(json_map_string_string));
        if (isuladinfo->annotations == NULL) {
            ERROR("Out of memory");
            return -1;
        }

        if (dup_json_map_string_string(common_config->config->annotations,
                                       isuladinfo->annotations) != 0) {
            ERROR("Failed to dup annotations");
            return -1;
        }
    }
    return 0;
}

static void dup_id_name(const container_config_v2_common_config *common_config, container_container *isuladinfo)
{
    if (common_config->id != NULL) {
        isuladinfo->id = util_strdup_s(common_config->id);
    }

    if (common_config->name != NULL) {
        isuladinfo->name = util_strdup_s(common_config->name);
    }
}

static int convert_common_config_info(const map_t *map_labels, const container_config_v2_common_config *common_config,
                                      container_container *isuladinfo)
{
    int ret = 0;
    bool args_err = false;

    if (map_labels == NULL || common_config == NULL || isuladinfo == NULL) {
        return -1;
    }

    if (common_config->config == NULL) {
        return 0;
    }
    args_err = (common_config->config->labels != NULL && common_config->config->labels->len != 0);
    if (args_err) {
        json_map_string_string *labels = common_config->config->labels;

        ret = replace_labels(isuladinfo, labels, map_labels);
        if (ret == -1) {
            goto out;
        }
    }

    ret = replace_annotations(common_config, isuladinfo);
    if (ret == -1) {
        goto out;
    }

    dup_id_name(common_config, isuladinfo);
    args_err = (common_config->created != NULL &&
                to_unix_nanos_from_str(common_config->created, &isuladinfo->created) != 0);
    if (args_err) {
        ret = -1;
        goto out;
    }
    isuladinfo->restartcount = (uint64_t)common_config->restart_count;
out:
    return ret;
}

static int container_info_match(const struct list_context *ctx, const map_t *map_labels,
                                const container_container *isuladinfo, const container_config_v2_state *cont_state)
{
    int ret = 0;
    Container_Status cs;

    if (ctx == NULL || map_labels == NULL || cont_state == NULL) {
        return -1;
    }

    if (!filters_args_match(ctx->ps_filters, "name", isuladinfo->name)) {
        ret = -1;
        goto out;
    }

    if (!filters_args_match(ctx->ps_filters, "id", isuladinfo->id)) {
        ret = -1;
        goto out;
    }

    cs = state_judge_status(cont_state);
    if (cs == CONTAINER_STATUS_CREATED) {
        if (!filters_args_match(ctx->ps_filters, "status", "created") && \
            !filters_args_match(ctx->ps_filters, "status", "inited")) {
            ret = -1;
            goto out;
        }
    } else if (!filters_args_match(ctx->ps_filters, "status", state_to_string(cs))) {
        ret = -1;
        goto out;
    }

    // Do not include container if any of the labels don't match
    if (!filters_args_match_kv_list(ctx->ps_filters, "label", map_labels)) {
        ret = -1;
        goto out;
    }

out:
    return ret;
}

static int get_cnt_state(const struct list_context *ctx, const container_config_v2_state *cont_state,
                         const char *name)
{
    if (cont_state == NULL) {
        ERROR("Failed to read %s state", name);
        return -1;
    }

    if (!cont_state->running && !ctx->list_config->all) {
        return -1;
    }
    return 0;
}

static int fill_isuladinfo(container_container *isuladinfo, const container_config_v2_state *cont_state,
                           const map_t *map_labels, const container_t *cont)
{
    int ret = 0;
    char *image = NULL;
    char *timestr = NULL;
    char *defvalue = "-";
    int64_t created_nanos = 0;

    ret = convert_common_config_info(map_labels, cont->common_config, isuladinfo);
    if (ret != 0) {
        goto out;
    }

    isuladinfo->pid = (int32_t)cont_state->pid;

    isuladinfo->status = (int)state_judge_status(cont_state);

    isuladinfo->command = container_get_command(cont);
    image = container_get_image(cont);
    isuladinfo->image = image ? image : util_strdup_s("none");

    isuladinfo->exit_code = (uint32_t)(cont_state->exit_code);
    timestr = cont_state->started_at ? cont_state->started_at : defvalue;
    isuladinfo->startat = util_strdup_s(timestr);

    timestr = cont_state->finished_at ? cont_state->finished_at : defvalue;
    isuladinfo->finishat = util_strdup_s(timestr);

    isuladinfo->runtime = cont->runtime ? util_strdup_s(cont->runtime) : util_strdup_s("none");

    isuladinfo->health_state = container_get_health_state(cont_state);
    if (cont->common_config->created != NULL) {
        ret = to_unix_nanos_from_str(cont->common_config->created, &created_nanos);
        if (ret != 0) {
            goto out;
        }
    }

    isuladinfo->created = created_nanos;

out:
    return ret;
}

static void free_isulad_info(container_container **isuladinfo, int ret)
{
    if (ret != 0) {
        free_container_container(*isuladinfo);
        *isuladinfo = NULL;
    }
    return;
}

static void unref_cont(container_t *cont)
{
    if (cont != NULL) {
        container_unref(cont);
    }
    return;
}

static container_container *get_container_info(const char *name, const struct list_context *ctx)
{
    int ret = 0;
    container_container *isuladinfo = NULL;
    container_t *cont = NULL;
    container_config_v2_state *cont_state = NULL;
    map_t *map_labels = NULL;

    cont = containers_store_get(name);
    if (cont == NULL) {
        ERROR("Container '%s' already removed", name);
        return NULL;
    }
    cont_state = state_get_info(cont->state);

    if (get_cnt_state(ctx, cont_state, name) != 0) {
        ret = -1;
        goto cleanup;
    }

    map_labels = map_new(MAP_STR_STR, MAP_DEFAULT_CMP_FUNC, MAP_DEFAULT_FREE_FUNC);
    if (map_labels == NULL) {
        ERROR("Out of memory");
        ret = -1;
        goto cleanup;
    }

    isuladinfo = util_common_calloc_s(sizeof(container_container));
    if (isuladinfo == NULL) {
        ERROR("Out of memory");
        ret = -1;
        goto cleanup;
    }

    ret = fill_isuladinfo(isuladinfo, cont_state, map_labels, cont);
    if (ret != 0) {
        goto cleanup;
    }

    ret = container_info_match(ctx, map_labels, isuladinfo, cont_state);
    if (ret != 0) {
        goto cleanup;
    }

cleanup:
    unref_cont(cont);
    map_free(map_labels);
    free_container_config_v2_state(cont_state);
    free_isulad_info(&isuladinfo, ret);
    return isuladinfo;
}

static int do_add_filters(const char *filter_key, const json_map_string_bool *filter_value, struct list_context *ctx)
{
    int ret = 0;
    size_t j;
    bool bret = false;

    for (j = 0; j < filter_value->len; j++) {
        if (strcmp(filter_key, "status") == 0) {
            if (!is_valid_state_string(filter_value->keys[j])) {
                ERROR("Unrecognised filter value for status: %s", filter_value->keys[j]);
                isulad_set_error_message("Unrecognised filter value for status: %s",
                                         filter_value->keys[j]);
                ret = -1;
                goto out;
            }
            ctx->list_config->all = true;
        }
        bret = filters_args_add(ctx->ps_filters, filter_key, filter_value->keys[j]);
        if (!bret) {
            ERROR("Add filter args failed");
            ret = -1;
            goto out;
        }
    }
out:
    return ret;
}

static struct list_context *fold_filter(const container_list_request *request)
{
    size_t i;
    struct list_context *ctx = NULL;

    ctx = list_context_new(request);
    if (ctx == NULL) {
        ERROR("Out of memory");
        return NULL;
    }

    if (request->filters == NULL) {
        return ctx;
    }

    for (i = 0; i < request->filters->len; i++) {
        if (!filters_args_valid_key(accepted_ps_filter_tags, sizeof(accepted_ps_filter_tags) / sizeof(char *),
                                    request->filters->keys[i])) {
            ERROR("Invalid filter '%s'", request->filters->keys[i]);
            isulad_set_error_message("Invalid filter '%s'", request->filters->keys[i]);
            goto error_out;
        }
        if (do_add_filters(request->filters->keys[i], request->filters->values[i], ctx) != 0) {
            goto error_out;
        }
    }

    return ctx;
error_out:
    free_list_context(ctx);
    return NULL;
}

static int pack_list_containers(char **idsarray, const struct list_context *ctx, container_list_response *response)
{
    int ret = 0;
    int j = 0;
    size_t container_nums = 0;

    container_nums = util_array_len((const char **)idsarray);
    if (container_nums == 0) {
        goto out;
    }

    if (container_nums > (SIZE_MAX / sizeof(container_container *))) {
        ERROR("Get too many containers:%d", container_nums);
        ret = -1;
        goto out;
    }

    response->containers = util_common_calloc_s(container_nums * sizeof(container_container *));
    if (response->containers == NULL) {
        ERROR("Out of memory");
        ret = -1;
        goto out;
    }

    while (idsarray != NULL && idsarray[j] != NULL) {
        response->containers[response->containers_len] = get_container_info(idsarray[j], ctx);
        if (response->containers[response->containers_len] == NULL) {
            j++;
            continue;
        }
        j++;
        response->containers_len++;
    }
out:
    return ret;
}

int container_list_cb(const container_list_request *request, container_list_response **response)
{
    char **idsarray = NULL;
    map_t *map_id_name = NULL;
    uint32_t cc = ISULAD_SUCCESS;
    struct list_context *ctx = NULL;

    DAEMON_CLEAR_ERRMSG();

    if (request == NULL || response == NULL) {
        ERROR("Invalid NULL input");
        return -1;
    }

    *response = util_common_calloc_s(sizeof(container_list_response));
    if (*response == NULL) {
        ERROR("Out of memory");
        cc = ISULAD_ERR_MEMOUT;
        goto pack_response;
    }

    ctx = fold_filter(request);
    if (ctx == NULL) {
        cc = ISULAD_ERR_EXEC;
        goto pack_response;
    }

    map_id_name = name_index_get_all();
    if (map_id_name == NULL) {
        cc = ISULAD_ERR_EXEC;
        goto pack_response;
    }
    if (map_size(map_id_name) == 0) {
        goto pack_response;
    }
    // fastpath to only look at a subset of containers if specific name
    // or ID matches were provided by the user--otherwise we potentially
    // end up querying many more containers than intended
    idsarray = filter_by_name_id_matches(ctx, map_id_name);

    if (pack_list_containers(idsarray, ctx, (*response)) != 0) {
        cc = ISULAD_ERR_EXEC;
        goto pack_response;
    }

pack_response:
    map_free(map_id_name);
    if (*response != NULL) {
        (*response)->cc = cc;
        if (g_isulad_errmsg != NULL) {
            (*response)->errmsg = util_strdup_s(g_isulad_errmsg);
            DAEMON_CLEAR_ERRMSG();
        }
    }
    util_free_array(idsarray);
    free_list_context(ctx);

    return (cc == ISULAD_SUCCESS) ? 0 : -1;
}

