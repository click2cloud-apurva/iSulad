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
 * Description: provide container events handler functions
 ******************************************************************************/
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <linux/limits.h>
#include <time.h>
#include <pthread.h>

#include "error.h"
#include "log.h"
#include "isulad_config.h"
#include "collector.h"
#include "events_handler.h"
#include "utils.h"
#include "containers_store.h"
#include "execution.h"
#include "plugin.h"

/* events handler lock */
static void events_handler_lock(events_handler_t *handler)
{
    if (pthread_mutex_lock(&(handler->mutex)) != 0) {
        ERROR("Failed to lock events handler");
    }
}

/* events handler unlock */
static void events_handler_unlock(events_handler_t *handler)
{
    if (pthread_mutex_unlock(&(handler->mutex)) != 0) {
        ERROR("Failed to unlock events handler");
    }
}

/* events handler free */
void events_handler_free(events_handler_t *handler)
{
    struct isulad_events_format *event = NULL;
    struct linked_list *it = NULL;
    struct linked_list *next = NULL;

    if (handler == NULL) {
        return;
    }

    linked_list_for_each_safe(it, &(handler->events_list), next) {
        event = (struct isulad_events_format *)it->elem;
        linked_list_del(it);
        isulad_events_format_free(event);
        free(it);
        it = NULL;
    }
    if (handler->init_mutex) {
        pthread_mutex_destroy(&(handler->mutex));
    }
    free(handler);
}

/* events handler new */
events_handler_t *events_handler_new()
{
    int ret;
    events_handler_t *handler = NULL;

    handler = util_common_calloc_s(sizeof(events_handler_t));
    if (handler == NULL) {
        ERROR("Out of memory");
        return NULL;
    }

    ret = pthread_mutex_init(&(handler->mutex), NULL);
    if (ret != 0) {
        ERROR("Failed to init mutex of events_handler");
        goto cleanup;
    }
    handler->init_mutex = true;

    linked_list_init(&(handler->events_list));

    handler->has_handler = false;

    return handler;
cleanup:
    events_handler_free(handler);
    return NULL;
}

/* container state changed */
static int container_state_changed(container_t *cont, const struct isulad_events_format *events)
{
    int ret = 0;
    int pid = 0;
    uint64_t timeout;
    char *id = events->id;
    char *started_at = NULL;
    bool should_restart = false;
    bool auto_remove = false;

    /* only handle Exit event */
    if (events->type != EVENTS_TYPE_STOPPED1) {
        return 0;
    }

    switch (events->type) {
        case EVENTS_TYPE_STOPPED1:
            container_lock(cont);

            if (false == is_running(cont->state)) {
                DEBUG("Container is not in running state ignore STOPPED event");
                container_unlock(cont);
                ret = 0;
                goto out;
            }

            pid = state_get_pid(cont->state);
            if (pid != (int)events->pid) {
                DEBUG("Container's pid \'%d\' is not equal to event's pid \'%d\', ignore STOPPED event",
                      pid, events->pid);
                container_unlock(cont);
                ret = 0;
                goto out;
            }

            started_at = state_get_started_at(cont->state);

            should_restart = restart_manager_should_restart(id, events->exit_status,
                                                            cont->common_config->has_been_manually_stopped,
                                                            time_seconds_since(started_at), &timeout);
            free(started_at);
            started_at = NULL;

            if (should_restart) {
                cont->common_config->restart_count++;
                state_set_restarting(cont->state, (int)events->exit_status);
                container_wait_stop_cond_broadcast(cont);
                INFO("Try to restart container %s after %.2fs", id, (double)timeout / Time_Second);
                (void)container_restart_in_thread(id, timeout, (int)events->exit_status);
            } else {
                state_set_stopped(cont->state, (int)events->exit_status);
                container_wait_stop_cond_broadcast(cont);
                plugin_event_container_post_stop(cont);
                stop_health_checks(cont->common_config->id);
            }

            auto_remove = !should_restart && cont->hostconfig != NULL && cont->hostconfig->auto_remove;
            if (auto_remove) {
                ret = set_container_to_removal(cont);
                if (ret != 0) {
                    ERROR("Failed to set container %s state to removal", cont->common_config->id);
                }
            }

            if (container_to_disk(cont)) {
                container_unlock(cont);
                ERROR("Failed to save container \"%s\" to disk", id);
                ret = -1;
                goto out;
            }

            container_unlock(cont);

            if (auto_remove) {
                ret = cleanup_container(cont, true);
                if (ret != 0) {
                    ERROR("Failed to cleanup container %s", cont->common_config->id);
                    ret = -1;
                    goto out;
                }
            }

            break;
        default:
            /* ignore garbage */
            break;
    }
out:
    return ret;
}

static int handle_one(container_t *cont, events_handler_t *handler)
{
    struct linked_list *it = NULL;
    struct isulad_events_format *events = NULL;

    events_handler_lock(handler);

    if (linked_list_empty(&(handler->events_list))) {
        handler->has_handler = false;
        events_handler_unlock(handler);
        return -1;
    }

    it = linked_list_first_node(&(handler->events_list));
    linked_list_del(it);

    events_handler_unlock(handler);

    events = (struct isulad_events_format *)it->elem;
    INFO("Received event %s with pid %d", events->id, events->pid);

    if (container_state_changed(cont, events)) {
        ERROR("Failed to change container %s state", cont->common_config->id);
    }

    isulad_events_format_free(events);
    events = NULL;

    free(it);
    it = NULL;

    return 0;
}

/* events handler thread */
static void *events_handler_thread(void *args)
{
    int ret = 0;
    char *name = args;
    container_t *cont = NULL;
    events_handler_t *handler = NULL;

    ret = pthread_detach(pthread_self());
    if (ret != 0) {
        CRIT("Set thread detach fail");
        goto out;
    }

    prctl(PR_SET_NAME, "events_handler");

    cont = containers_store_get(name);
    if (cont == NULL) {
        INFO("Container '%s' already removed", name);
        goto out;
    }

    handler = cont->handler;
    if (handler == NULL) {
        INFO("Container '%s' event handler already removed", name);
        goto out;
    }

    while (handle_one(cont, handler) == 0) {}

out:
    container_unref(cont);
    free(name);
    DAEMON_CLEAR_ERRMSG();
    return NULL;
}

/* events handler post events */
int events_handler_post_events(events_handler_t *handler,
                               const struct isulad_events_format *event)
{
    int ret = 0;
    char *name = NULL;
    pthread_t td;
    struct isulad_events_format *post_event = NULL;
    struct linked_list *it = NULL;

    if (handler == NULL || event == NULL) {
        return -1;
    }

    it = util_common_calloc_s(sizeof(struct linked_list));
    if (it == NULL) {
        ERROR("Failed to malloc for linked_list");
        return -1;
    }

    linked_list_init(it);

    post_event = dup_event(event);
    if (post_event == NULL) {
        ERROR("Failed to dup event");
        free(it);
        return -1;
    }

    linked_list_add_elem(it, post_event);

    events_handler_lock(handler);

    linked_list_add_tail(&(handler->events_list), it);

    if (handler->has_handler == false) {
        name = util_strdup_s(event->id);
        ret = pthread_create(&td, NULL, events_handler_thread, name);
        if (ret) {
            CRIT("Events handler thread create failed");
            free(name);
            goto out;
        }
        handler->has_handler = true;
    }
out:
    events_handler_unlock(handler);
    return ret;
}

