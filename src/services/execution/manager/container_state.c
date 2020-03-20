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
 * Author: lifeng
 * Create: 2017-11-22
 * Description: provide container state functions
 ******************************************************************************/
#include <stdlib.h>
#include <string.h>

#include "container_unix.h"
#include "container_state.h"
#include "log.h"
#include "utils.h"
#include "error.h"

/* container state lock */
void container_state_lock(container_state_t *state)
{
    if (pthread_mutex_lock(&state->mutex)) {
        ERROR("Failed to lock state");
    }
}

/* container state unlock */
void container_state_unlock(container_state_t *state)
{
    if (pthread_mutex_unlock(&state->mutex)) {
        ERROR("Failed to lock state");
    }
}

/* container state new */
container_state_t *container_state_new(void)
{
    int ret;
    container_state_t *s = NULL;

    s = util_common_calloc_s(sizeof(container_state_t));
    if (s == NULL) {
        ERROR("Out of memory");
        return NULL;
    }
    ret = pthread_mutex_init(&s->mutex, NULL);
    if (ret != 0) {
        ERROR("Failed to init mutex of state");
        goto error_out;
    }

    s->state = util_common_calloc_s(sizeof(container_config_v2_state));
    if (s->state == NULL) {
        ERROR("Out of memory");
        goto error_out;
    }

    s->state->started_at = util_strdup_s(defaultContainerTime);
    s->state->finished_at = util_strdup_s(defaultContainerTime);

    return s;
error_out:
    container_state_free(s);
    return NULL;
}

/* container state free */
void container_state_free(container_state_t *state)
{
    if (state == NULL) {
        return;
    }

    free_container_config_v2_state(state->state);
    state->state = NULL;

    pthread_mutex_destroy(&state->mutex);
    free(state);
}

/* state set starting */
void state_set_starting(container_state_t *s)
{
    if (s == NULL) {
        return;
    }

    container_state_lock(s);

    s->state->starting = true;

    container_state_unlock(s);
}

/* state set dead */
void state_set_dead(container_state_t *s)
{
    if (s == NULL) {
        return;
    }

    container_state_lock(s);

    s->state->dead = true;

    container_state_unlock(s);
}

/* state reset starting */
void state_reset_starting(container_state_t *s)
{
    if (s == NULL) {
        return;
    }

    container_state_lock(s);

    s->state->starting = false;

    container_state_unlock(s);
}

/* state set running */
void state_set_running(container_state_t *s, const container_pid_t *pid_info, bool initial)
{
    container_config_v2_state *state = NULL;
    char timebuffer[TIME_STR_SIZE] = { 0 };

    if (s == NULL) {
        return;
    }

    container_state_lock(s);

    state = s->state;

    free(state->error);
    state->error = NULL;

    state->running = true;
    state->starting = false;
    state->restarting = false;
    if (initial) {
        state->paused = false;
    }
    state->exit_code = 0;

    if (pid_info != NULL) {
        state->pid = pid_info->pid;
        state->start_time = pid_info->start_time;
        state->p_pid = pid_info->ppid;
        state->p_start_time = pid_info->pstart_time;
    } else {
        state->pid = 0;
        state->start_time = 0;
        state->p_pid = 0;
        state->p_start_time = 0;
    }

    (void)get_now_time_buffer(timebuffer, sizeof(timebuffer));
    free(state->started_at);
    state->started_at = util_strdup_s(timebuffer);

    container_state_unlock(s);
}

/* state reset stopped */
void state_set_stopped(container_state_t *s, int exit_code)
{
    container_config_v2_state *state = NULL;
    char timebuffer[TIME_STR_SIZE] = { 0 };

    if (s == NULL) {
        return;
    }

    container_state_lock(s);

    state = s->state;

    state->running = false;
    state->starting = false;
    state->restarting = false;
    state->paused = false;
    state->exit_code = exit_code;
    state->pid = 0;
    state->start_time = 0;
    state->p_pid = 0;
    state->p_start_time = 0;

    (void)get_now_time_buffer(timebuffer, sizeof(timebuffer));
    free(state->finished_at);
    state->finished_at = util_strdup_s(timebuffer);

    container_state_unlock(s);
}

/* state set paused */
void state_set_paused(container_state_t *s)
{
    container_config_v2_state *state = NULL;

    if (s == NULL) {
        return;
    }

    container_state_lock(s);

    state = s->state;
    state->paused = true;

    container_state_unlock(s);
}

/* state reset paused */
void state_reset_paused(container_state_t *s)
{
    container_config_v2_state *state = NULL;

    if (s == NULL) {
        return;
    }

    container_state_lock(s);

    state = s->state;
    state->paused = false;

    container_state_unlock(s);
}

/* update start and finish time */
void update_start_and_finish_time(container_state_t *s, const char *finish_at)
{
    container_config_v2_state *state = NULL;
    char timebuffer[TIME_STR_SIZE] = { 0 };

    if (s == NULL) {
        return;
    }

    container_state_lock(s);

    state = s->state;

    state->running = true;
    state->starting = false;
    state->restarting = false;
    state->paused = false;
    state->exit_code = 0;

    (void)get_now_time_buffer(timebuffer, sizeof(timebuffer));
    free(state->finished_at);
    state->finished_at = util_strdup_s(finish_at);
    free(state->started_at);
    state->started_at = util_strdup_s(timebuffer);

    container_state_unlock(s);

    return;
}

/* state set restarting */
void state_set_restarting(container_state_t *s, int exit_code)
{
    container_config_v2_state *state = NULL;
    char timebuffer[TIME_STR_SIZE] = { 0 };

    if (s == NULL) {
        return;
    }

    container_state_lock(s);

    state = s->state;

    state->running = true;
    state->starting = false;
    state->restarting = true;
    state->paused = false;
    state->pid = 0;
    state->start_time = 0;
    state->p_pid = 0;
    state->p_start_time = 0;
    state->exit_code = exit_code;

    (void)get_now_time_buffer(timebuffer, sizeof(timebuffer));
    free(state->finished_at);
    state->finished_at = util_strdup_s(timebuffer);

    container_state_unlock(s);

    return;
}

// state_set_removal_in_progress sets the container state as being removed.
// It returns true if the container was already in that state
bool state_set_removal_in_progress(container_state_t *s)
{
    bool ret = false;

    if (s == NULL) {
        return false;
    }

    container_state_lock(s);

    if (s->state->removal_inprogress) {
        ret = true;
    } else {
        s->state->removal_inprogress = true;
    }

    container_state_unlock(s);

    return ret;
}

/* state reset removal in progress */
void state_reset_removal_in_progress(container_state_t *s)
{
    if (s == NULL) {
        return;
    }

    container_state_lock(s);

    s->state->removal_inprogress = false;

    container_state_unlock(s);

    return;
}

/* container state set error */
void container_state_set_error(container_state_t *s, const char *err)
{
    container_state_lock(s);
    if (err != NULL) {
        free(s->state->error);
        s->state->error = util_strdup_s(err);
    }
    container_state_unlock(s);
}

/* state judge status */
Container_Status state_judge_status(const container_config_v2_state *state)
{
    if (state == NULL) {
        return CONTAINER_STATUS_UNKNOWN;
    }

    if (state->running) {
        if (state->paused) {
            return CONTAINER_STATUS_PAUSED;
        }
        if (state->restarting) {
            return CONTAINER_STATUS_RESTARTING;
        }

        return CONTAINER_STATUS_RUNNING;
    }

    if (state->starting) {
        return CONTAINER_STATUS_STARTING;
    }

    if (state->started_at == NULL || state->finished_at == NULL) {
        return CONTAINER_STATUS_UNKNOWN;
    }
    if (strcmp(state->started_at, defaultContainerTime) == 0 &&
        strcmp(state->finished_at, defaultContainerTime) == 0) {
        return CONTAINER_STATUS_CREATED;
    }

    return CONTAINER_STATUS_STOPPED;
}

const char *state_to_string(Container_Status cs)
{
    const char *state_string[] = {
        "unknown", "inited", "starting", "running", "exited", "paused", "restarting"
    };

    if (cs >= CONTAINER_STATUS_MAX_STATE) {
        return "unknown";
    }
    return state_string[cs];
}

/* state get status */
Container_Status state_get_status(container_state_t *s)
{
    Container_Status status = CONTAINER_STATUS_UNKNOWN;

    if (s == NULL) {
        return status;
    }

    container_state_lock(s);

    status = state_judge_status(s->state);

    container_state_unlock(s);
    return status;
}

int dup_health_check_status(defs_health **dst, const defs_health *src)
{
    int ret = 0;
    size_t i = 0;
    defs_health *result = NULL;

    if (src == NULL) {
        return 0;
    }
    result = util_common_calloc_s(sizeof(defs_health));
    if (result == NULL) {
        ERROR("Out of memory");
        return -1;
    }
    result->status = src->status ? util_strdup_s(src->status) : NULL;
    result->failing_streak = src->failing_streak;
    if (src->log_len != 0) {
        if (src->log_len > SIZE_MAX / sizeof(defs_health_log_element *)) {
            ERROR("Invalid log size");
            ret = -1;
            goto error;
        }
        result->log = util_common_calloc_s(sizeof(defs_health_log_element *) * src->log_len);
        if (result->log == NULL) {
            ERROR("Out of memory");
            ret = -1;
            goto error;
        }
        for (i = 0; i < src->log_len; i++) {
            result->log[i] = util_common_calloc_s(sizeof(defs_health_log_element));
            if (result->log[i] == NULL) {
                ERROR("Out of memory");
                ret = -1;
                goto error;
            }
            result->log[i]->start = util_strdup_s(src->log[i]->start);
            result->log[i]->end = util_strdup_s(src->log[i]->end);
            result->log[i]->exit_code = src->log[i]->exit_code;
            result->log[i]->output = util_strdup_s(src->log[i]->output);
            result->log_len++;
        }
    }
    *dst = result;
    return ret;
error:
    free_defs_health(result);
    return ret;
}

/* state get info */
container_config_v2_state *state_get_info(container_state_t *s)
{
    container_config_v2_state *state = NULL;

    if (s == NULL) {
        return NULL;
    }

    state = util_common_calloc_s(sizeof(container_config_v2_state));
    if (state == NULL) {
        return NULL;
    }

    container_state_lock(s);

    state->pid = s->state->pid;
    state->oom_killed = s->state->oom_killed;
    state->dead = s->state->dead;
    state->paused = s->state->paused;
    state->running = s->state->running;
    state->restarting = s->state->restarting;
    state->removal_inprogress = s->state->removal_inprogress;
    state->starting = s->state->starting;
    state->exit_code = s->state->exit_code;

    state->started_at = s->state->started_at ?
                        util_strdup_s(s->state->started_at) : util_strdup_s(defaultContainerTime);
    state->finished_at = s->state->finished_at ?
                         util_strdup_s(s->state->finished_at) : util_strdup_s(defaultContainerTime);
    state->error = s->state->error ? util_strdup_s(s->state->error) : NULL;

    if (dup_health_check_status(&state->health, s->state->health) != 0) {
        ERROR("Failed to dup health check info");
        free_container_config_v2_state(state);
        state = NULL;
    }

    container_state_unlock(s);

    return state;
}

/* state get exitcode */
uint32_t state_get_exitcode(container_state_t *s)
{
    uint32_t exit_code = 0;
    container_config_v2_state *state = NULL;

    if (s == NULL) {
        return exit_code;
    }

    container_state_lock(s);

    state = s->state;
    exit_code = (uint32_t)state->exit_code;

    container_state_unlock(s);

    return exit_code;
}

/* is running */
bool is_running(container_state_t *s)
{
    bool ret = false;

    if (s == NULL) {
        return false;
    }

    container_state_lock(s);

    ret = s->state->running;

    container_state_unlock(s);

    return ret;
}

/* is restarting */
bool is_restarting(container_state_t *s)
{
    bool ret = false;

    if (s == NULL) {
        return false;
    }

    container_state_lock(s);

    ret = s->state->restarting;

    container_state_unlock(s);

    return ret;
}

/* is paused */
bool is_paused(container_state_t *s)
{
    bool ret = false;

    if (s == NULL) {
        return false;
    }

    container_state_lock(s);

    ret = s->state->paused;

    container_state_unlock(s);

    return ret;
}

/* is removal in progress */
bool is_removal_in_progress(container_state_t *s)
{
    bool ret = false;

    if (s == NULL) {
        return false;
    }

    container_state_lock(s);

    ret = s->state->removal_inprogress;

    container_state_unlock(s);

    return ret;
}

/* is dead */
bool is_dead(container_state_t *s)
{
    bool ret = false;

    if (s == NULL) {
        return false;
    }

    container_state_lock(s);

    ret = s->state->dead;

    container_state_unlock(s);

    return ret;
}


// state_get_pid holds the process id of a container.
int state_get_pid(container_state_t *s)
{
    int pid = 0;

    if (s == NULL) {
        return pid;
    }

    container_state_lock(s);

    pid = s->state->pid;

    container_state_unlock(s);

    return pid;
}

/* state get started at */
char *state_get_started_at(container_state_t *s)
{
    char *ret = NULL;

    if (s == NULL) {
        return NULL;
    }

    container_state_lock(s);

    ret = util_strdup_s(s->state->started_at);

    container_state_unlock(s);

    return ret;
}

static inline bool is_state_string_paused(const char *state)
{
    return strcmp(state, "paused") == 0;
}

static inline bool is_state_string_restarting(const char *state)
{
    return strcmp(state, "restarting") == 0;
}

static inline bool is_state_string_running(const char *state)
{
    return strcmp(state, "running") == 0;
}

static inline bool is_state_string_dead(const char *state)
{
    return strcmp(state, "dead") == 0;
}

static inline bool is_state_string_created(const char *state)
{
    return strcmp(state, "created") == 0;
}

static inline bool is_state_string_exited(const char *state)
{
    return strcmp(state, "exited") == 0;
}

bool is_valid_state_string(const char *state)
{
    if (state == NULL) {
        return false;
    }
    if (!is_state_string_paused(state) && !is_state_string_restarting(state) && \
        !is_state_string_running(state) && !is_state_string_dead(state) && \
        !is_state_string_created(state) && !is_state_string_exited(state)) {
        return false;
    }
    return true;
}

