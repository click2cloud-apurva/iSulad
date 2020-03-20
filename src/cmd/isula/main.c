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
 * Author: lifeng
 * Create: 2018-11-08
 * Description: provide init process of isula
 ******************************************************************************/

#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include "commands.h"
#include "create.h"
#include "ps.h"
#include "rm.h"
#include "start.h"
#include "inspect.h"
#include "stop.h"
#include "exec.h"
#include "run.h"
#include "images.h"
#include "rmi.h"
#include "wait.h"
#include "restart.h"
#include "pause.h"
#include "resume.h"
#include "logs.h"
#include "events.h"
#include "kill.h"
#include "load.h"
#include "update.h"
#include "attach.h"
#include "info.h"
#include "stats.h"
#include "export.h"
#include "cp.h"
#include "top.h"
#include "pull.h"
#include "login.h"
#include "logout.h"
#include "isula_connect.h"
#include "version.h"
#include "rename.h"
#include "utils.h"

// The list of our supported commands
struct command g_commands[] = {
    {
        // `create` sub-command
        "create", cmd_create_main, g_cmd_create_desc, NULL, &g_cmd_create_args
    },
    {
        // `rm` sub-command
        "rm", cmd_delete_main, g_cmd_delete_desc, NULL, &g_cmd_delete_args
    },
    {
        // `ps` sub-command
        "ps", cmd_list_main, g_cmd_list_desc, NULL, &g_cmd_list_args
    },
    {
        // `start` sub-command
        "start", cmd_start_main, g_cmd_start_desc, NULL, &g_cmd_start_args
    },
    {
        // `run` sub-command
        "run", cmd_run_main, g_cmd_run_desc, NULL, &g_cmd_run_args
    },
    {
        // `restart` sub-command
        "restart", cmd_restart_main, g_cmd_restart_desc, NULL, &g_cmd_restart_args
    },
    {
        // `inspect` sub-command
        "inspect", cmd_inspect_main, g_cmd_inspect_desc, NULL, &g_cmd_inspect_args
    },
    {
        // `pause` sub-command
        "pause", cmd_pause_main, g_cmd_pause_desc, NULL, &g_cmd_pause_args
    },
    {
        // `unpause` sub-command
        "unpause", cmd_resume_main, g_cmd_resume_desc, NULL, &g_cmd_resume_args
    },
#ifdef ENABLE_OCI_IMAGE
    {
        // `stats` sub-command
        "stats",
        cmd_stats_main,
        g_cmd_stats_desc,
        NULL,
        &g_cmd_stats_args
    },
    {
        // `cp` sub-command
        "cp", cmd_cp_main, g_cmd_cp_desc, NULL, &g_cmd_cp_args
    },
#endif
    {
        // `stop` sub-command
        "stop", cmd_stop_main, g_cmd_stop_desc, NULL, &g_cmd_stop_args
    },
    {
        // `version` sub-command
        "version", cmd_version_main, g_cmd_version_desc, NULL, &g_cmd_version_args
    },
    {
        // `exec` sub-command
        "exec", cmd_exec_main, g_cmd_exec_desc, NULL, &g_cmd_exec_args
    },
    {
        // `images` sub-command
        "images", cmd_images_main, g_cmd_images_desc, NULL, &g_cmd_images_args
    },
#ifdef ENABLE_OCI_IMAGE
    {
        // `info` sub-command
        "info", cmd_info_main, g_cmd_info_desc, NULL, &g_cmd_info_args
    },
#endif
    {
        // `remove images` sub-command
        "rmi", cmd_rmi_main, g_cmd_rmi_desc, NULL, &g_cmd_rmi_args
    },
#ifdef ENABLE_OCI_IMAGE
    {
        // `wait` sub-command
        "wait", cmd_wait_main, g_cmd_wait_desc, NULL, &g_cmd_wait_args
    },
    {
        // `logs` sub-command
        "logs", cmd_logs_main, g_cmd_logs_desc, NULL, &g_cmd_logs_args
    },
    {
        // `events` sub-command
        "events", cmd_events_main, g_cmd_events_desc, NULL, &g_cmd_events_args
    },
#endif
    {
        // `kill` sub-command
        "kill", cmd_kill_main, g_cmd_kill_desc, NULL, &g_cmd_kill_args
    },
    {
        // `load` sub-command
        "load", cmd_load_main, g_cmd_load_desc, NULL, &g_cmd_load_args
    },
#ifdef ENABLE_OCI_IMAGE
    {
        // `update` sub-command
        "update", cmd_update_main, g_cmd_update_desc, NULL, &g_cmd_update_args
    },
#endif
    {
        // `attach` sub-command
        "attach", cmd_attach_main, g_cmd_attach_desc, NULL, &g_cmd_attach_args
    },
#ifdef ENABLE_OCI_IMAGE
    {
        // `export` sub-command
        "export", cmd_export_main, g_cmd_export_desc, NULL, &g_cmd_export_args
    },
    {
        // `top` sub-command
        "top", cmd_top_main, g_cmd_top_desc, NULL, &g_cmd_top_args
    },
    {
        // `rename` sub-command
        "rename",
        cmd_rename_main,
        g_cmd_rename_desc,
        NULL,
        &g_cmd_rename_args
    },
    {
        // `pull` sub-command
        "pull", cmd_pull_main, g_cmd_pull_desc, NULL, &g_cmd_pull_args
    },
    {
        // `login` sub-command
        "login", cmd_login_main, g_cmd_login_desc, NULL, &g_cmd_login_args
    },
    {
        // `logout` sub-command
        "logout", cmd_logout_main, g_cmd_logout_desc, NULL, &g_cmd_logout_args
    },
#endif
    { NULL, NULL, NULL, NULL, NULL } // End of the list
};

int main(int argc, char **argv)
{
    if (connect_client_ops_init()) {
        return ECOMMON;
    }
    return run_command(g_commands, argc, (const char **)argv);
}

