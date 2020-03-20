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
 * Description: provide container create definition
 ******************************************************************************/
#ifndef __CMD_CREATE_H
#define __CMD_CREATE_H

#include "arguments.h"

#ifdef __cplusplus
extern "C" {
#endif

#define CREATE_OPTIONS(cmdargs) \
    { CMD_OPT_TYPE_CALLBACK, false, "accel", 0, &(cmdargs).custom_conf.accel, \
        "Accelerator bindings (format: [<name>=]<runtime>[@<driver>[,<options>]])", \
        command_append_array }, \
    { CMD_OPT_TYPE_BOOL, false, "read-only", 0, &(cmdargs).custom_conf.readonly, \
      "Make container rootfs readonly", NULL }, \
    { CMD_OPT_TYPE_CALLBACK, false, "cap-add", 0, &(cmdargs).custom_conf.cap_adds, \
      "Add Linux capabilities ('ALL' to add all capabilities)", command_append_array }, \
    { CMD_OPT_TYPE_CALLBACK, false, "cap-drop", 0, &(cmdargs).custom_conf.cap_drops, \
      "Drop Linux capabilities ('ALL' to drop all capabilities)", command_append_array }, \
    { CMD_OPT_TYPE_CALLBACK, false, "cpu-shares", 0, &(cmdargs).cr.cpu_shares, \
      "CPU shares (relative weight)", command_convert_llong }, \
    { CMD_OPT_TYPE_CALLBACK, false, "cpu-period", 0, &(cmdargs).cr.cpu_period, \
      "Limit CPU CFS (Completely Fair Scheduler) period", command_convert_llong }, \
    { CMD_OPT_TYPE_CALLBACK, false, "cpu-quota", 0, &(cmdargs).cr.cpu_quota, \
      "Limit CPU CFS (Completely Fair Scheduler) quota", command_convert_llong }, \
    { CMD_OPT_TYPE_STRING, false, "cpuset-cpus", 0, &(cmdargs).cr.cpuset_cpus, \
      "CPUs in which to allow execution (e.g. 0-3, 0,1)", NULL }, \
    { CMD_OPT_TYPE_STRING, false, "cpuset-mems", 0, &(cmdargs).cr.cpuset_mems, \
      "MEMs in which to allow execution (0-3, 0,1)", NULL }, \
    { CMD_OPT_TYPE_CALLBACK, false, "device-read-bps", 0, &(cmdargs).custom_conf.blkio_throttle_read_bps_device, \
      "Limit read rate (bytes per second) from a device (default [])", command_append_array }, \
    { CMD_OPT_TYPE_CALLBACK, false, "device-write-bps", 0, &(cmdargs).custom_conf.blkio_throttle_write_bps_device, \
      "Limit write rate (bytes per second) to a device (default [])", command_append_array }, \
    { CMD_OPT_TYPE_CALLBACK, false, "oom-score-adj", 0, &(cmdargs).cr.oom_score_adj, \
      "Tune host's OOM preferences (-1000 to 1000)", command_convert_llong }, \
    { CMD_OPT_TYPE_CALLBACK, false, "device", 0, &(cmdargs).custom_conf.devices, \
      "Add a host device to the container", command_append_array }, \
    { CMD_OPT_TYPE_CALLBACK, false, "env", 'e', &(cmdargs).custom_conf.env, \
      "Set environment variables", command_append_array }, \
    { CMD_OPT_TYPE_CALLBACK, false, "env-file", 0, &(cmdargs).custom_conf.env_file, \
      "Read in a file of environment variables", command_append_array }, \
    { CMD_OPT_TYPE_CALLBACK, false, "label", 'l', &(cmdargs).custom_conf.label, \
      "Set metadata on container (default [])", command_append_array }, \
    { CMD_OPT_TYPE_CALLBACK, false, "label-file", 0, &(cmdargs).custom_conf.label_file, \
      "Read in a line delimited file of labels (default [])", command_append_array }, \
    { CMD_OPT_TYPE_STRING_DUP, false, "entrypoint", 0, &(cmdargs).custom_conf.entrypoint, \
      "Entrypoint to run when starting the container", NULL }, \
    { CMD_OPT_TYPE_STRING, false, "external-rootfs", 0, &(cmdargs).external_rootfs, \
      "Specify the custom rootfs that is not managed by isulad for the container, directory or block device", NULL }, \
    { CMD_OPT_TYPE_CALLBACK, false, "files-limit", 0, &(cmdargs).custom_conf.files_limit, \
      "Tune container files limit (set -1 for unlimited)", command_convert_llong }, \
    { CMD_OPT_TYPE_STRING_DUP, false, "hook-spec", 0, &(cmdargs).custom_conf.hook_spec, \
      "File containing hook definition(prestart, poststart, poststop)", NULL }, \
    { CMD_OPT_TYPE_STRING_DUP, false, "hostname", 'h', &(cmdargs).custom_conf.hostname, \
      "Container host name", NULL }, \
    { CMD_OPT_TYPE_CALLBACK, false, "add-host", 0, &(cmdargs).custom_conf.extra_hosts, \
      "Add a custom host-to-IP mapping (host:ip)", command_append_array }, \
    { CMD_OPT_TYPE_CALLBACK, false, "dns", 0, &(cmdargs).custom_conf.dns, \
      "Set custom DNS servers", command_append_array }, \
    { CMD_OPT_TYPE_CALLBACK, false, "dns-opt", 0, &(cmdargs).custom_conf.dns_options, \
      "Set DNS options", command_append_array }, \
    { CMD_OPT_TYPE_CALLBACK, false, "dns-search", 0, &(cmdargs).custom_conf.dns_search, \
      "Set custom DNS search domains", command_append_array }, \
    { CMD_OPT_TYPE_STRING, false, "user-remap", 0, &(cmdargs).custom_conf.user_remap, \
      "Set user remap for container", NULL }, \
    { CMD_OPT_TYPE_STRING_DUP, false, "ipc", 0, &(cmdargs).custom_conf.share_ns[NAMESPACE_IPC], \
      "IPC namespace to use", NULL }, \
    { CMD_OPT_TYPE_CALLBACK, false, "shm-size", 0, &(cmdargs).custom_conf.shm_size, \
      "Size of /dev/shm, default value is 64MB", command_convert_membytes }, \
    { CMD_OPT_TYPE_CALLBACK, false, "kernel-memory", 0, &(cmdargs).cr.kernel_memory_limit, \
      "Kernel memory limit", command_convert_membytes }, \
    { CMD_OPT_TYPE_CALLBACK, false, "hugetlb-limit", 0, &(cmdargs).custom_conf.hugepage_limits, \
      "Huge page limit (format: [size:]<limit>, e.g. --hugetlb-limit 2MB:32MB)", command_append_array }, \
    { CMD_OPT_TYPE_CALLBACK, false, "log-opt", 0, &(cmdargs), \
      "Container log options, value formate: key=value", callback_log_opt }, \
    { CMD_OPT_TYPE_CALLBACK, false, "memory", 'm', &(cmdargs).cr.memory_limit, \
      "Memory limit", command_convert_membytes }, \
    { CMD_OPT_TYPE_CALLBACK, false, "memory-reservation", 0, &(cmdargs).cr.memory_reservation, \
      "Memory soft limit", command_convert_membytes }, \
    { CMD_OPT_TYPE_CALLBACK, false, "memory-swap", 0, &(cmdargs).cr.memory_swap, \
      "Swap limit equal to memory plus swap: '-1' to enable unlimited swap", command_convert_memswapbytes }, \
    { CMD_OPT_TYPE_CALLBACK, false, "memory-swappiness", 0, &(cmdargs).cr.swappiness, \
      "Tune container memory swappiness (0 to 100) (default -1)", command_convert_swappiness }, \
    { CMD_OPT_TYPE_CALLBACK, false, "mount", 0, &(cmdargs).custom_conf.mounts, \
      "Attach a filesystem mount to the service", command_append_array }, \
    { CMD_OPT_TYPE_CALLBACK, false, "group-add", 0, &(cmdargs).custom_conf.group_add, \
      "Add additional groups to join", command_append_array }, \
    { CMD_OPT_TYPE_STRING_DUP, false, "name", 'n', &(cmdargs).name, "Name of the container", NULL }, \
    { CMD_OPT_TYPE_STRING_DUP, false, "net", 0, &(cmdargs).custom_conf.share_ns[NAMESPACE_NET], \
      "Connect a container to a network", NULL }, \
    { CMD_OPT_TYPE_STRING_DUP, false, "pid", 0, &(cmdargs).custom_conf.share_ns[NAMESPACE_PID], \
      "PID namespace to use", NULL }, \
    { CMD_OPT_TYPE_CALLBACK, false, "pids-limit", 0, &(cmdargs).custom_conf.pids_limit, \
      "Tune container pids limit (set -1 for unlimited)", command_convert_llong }, \
    { CMD_OPT_TYPE_BOOL, false, "privileged", 0, &(cmdargs).custom_conf.privileged, \
      "Give extended privileges to this container", NULL }, \
    { CMD_OPT_TYPE_BOOL, false, "tty", 't', &(cmdargs).custom_conf.tty, "Allocate a pseudo-TTY", NULL }, \
    { CMD_OPT_TYPE_STRING, false, "restart", 0, &(cmdargs).restart, \
      "Restart policy to apply when a container exits(no, always, on-reboot, on-failure[:max-retries])", NULL }, \
    { CMD_OPT_TYPE_STRING, false, "host-channel", 0, &(cmdargs).host_channel, \
      "Create share memory between host and container", NULL }, \
    { CMD_OPT_TYPE_STRING, false, "runtime", 'R', &(cmdargs).runtime, \
      "Runtime to use for containers(default: lcr)", NULL }, \
    { CMD_OPT_TYPE_STRING_DUP, false, "user", 'u', &(cmdargs).custom_conf.user, \
      "Username or UID (format: <name|uid>[:<group|gid>])", NULL }, \
    { CMD_OPT_TYPE_STRING_DUP, false, "uts", 0, &(cmdargs).custom_conf.share_ns[NAMESPACE_UTS], \
      "UTS namespace to use", NULL }, \
    { CMD_OPT_TYPE_CALLBACK, false, "volume", 'v', &(cmdargs).custom_conf.volumes, \
      "Bind mount a volume", command_append_array }, \
    { CMD_OPT_TYPE_CALLBACK, false, "annotation", 0, &(cmdargs), \
      "Set annotations on a container", callback_annotation }, \
    { CMD_OPT_TYPE_STRING_DUP, false, "workdir", 0, &(cmdargs).custom_conf.workdir, \
      "Working directory inside the container", NULL }, \
    { CMD_OPT_TYPE_BOOL, false, "system-container", 0, &(cmdargs).custom_conf.system_container, \
      "Extend some features only needed by running system container", NULL }, \
    { CMD_OPT_TYPE_BOOL, false, "oom-kill-disable", 0, &(cmdargs).custom_conf.oom_kill_disable, \
      "Disable OOM Killer", NULL }, \
    { CMD_OPT_TYPE_CALLBACK, false, "security-opt", 0, &(cmdargs).custom_conf.security, \
      "Security Options (default [])", command_append_array }, \
    { CMD_OPT_TYPE_CALLBACK, false, "storage-opt", 0, &(cmdargs).custom_conf.storage_opts, \
      "Storage driver options for the container", command_append_array }, \
    { CMD_OPT_TYPE_STRING_DUP, false, "health-cmd", 0, &(cmdargs).custom_conf.health_cmd, \
      "Command to run to check health", NULL }, \
    { CMD_OPT_TYPE_CALLBACK, false, "sysctl", 0, &(cmdargs).custom_conf.sysctls, \
      "Sysctl options", command_append_array }, \
    { CMD_OPT_TYPE_STRING_DUP, false, "env-target-file", 0, &(cmdargs).custom_conf.env_target_file, \
      "Export env to target file path in rootfs", NULL }, \
    { CMD_OPT_TYPE_STRING_DUP, false, "cgroup-parent", 0, &(cmdargs).custom_conf.cgroup_parent, \
      "Optional parent cgroup for the container", NULL }, \
    { CMD_OPT_TYPE_CALLBACK, false, "health-interval", 0, &(cmdargs).custom_conf.health_interval, \
      "Time between running the check (ms|s|m|h) (default 30s)", command_convert_nanoseconds }, \
    { CMD_OPT_TYPE_CALLBACK, false, "health-retries", 0, &(cmdargs).custom_conf.health_retries, \
      "Consecutive failures needed to report unhealthy (default 3)", command_convert_int }, \
    { CMD_OPT_TYPE_CALLBACK, false, "health-timeout", 0, &(cmdargs).custom_conf.health_timeout, \
      "Maximum time to allow one check to run (ms|s|m|h) (default 30s)", command_convert_nanoseconds }, \
    { CMD_OPT_TYPE_CALLBACK, false, "health-start-period", 0, &(cmdargs).custom_conf.health_start_period, \
      "Start period for the container to initialize before starting health-retries countdown (ms|s|m|h) " \
      "(default 0s)", command_convert_nanoseconds }, \
    { CMD_OPT_TYPE_BOOL, false, "no-healthcheck", 0, &(cmdargs).custom_conf.no_healthcheck, \
      "Disable any container-specified HEALTHCHECK", NULL }, \
    { CMD_OPT_TYPE_BOOL, false, "health-exit-on-unhealthy", 0, &(cmdargs).custom_conf.exit_on_unhealthy, \
      "Kill the container when it is detected to be unhealthy", NULL }, \
    { CMD_OPT_TYPE_STRING, false, "ns-change-opt", 0, &(cmdargs).custom_conf.ns_change_opt, \
      "Namespaced kernel param options for system container (default [])", NULL }, \
    { CMD_OPT_TYPE_CALLBACK, false, "ulimit", 0, &(cmdargs).custom_conf.ulimits, \
      "Ulimit options (default [])", command_append_array }

#define CREATE_EXTEND_OPTIONS(cmdargs) \
    { CMD_OPT_TYPE_BOOL, false, "interactive", 'i', &(cmdargs).custom_conf.open_stdin, \
        "Keep STDIN open even if not attached", NULL }

extern const char g_cmd_create_desc[];
extern const char g_cmd_create_usage[];
extern struct client_arguments g_cmd_create_args;

int create_parser(struct client_arguments *args, int c, char *arg);

int create_checker(struct client_arguments *args);

int client_create(struct client_arguments *args);

int callback_log_opt(command_option_t *option, const char *value);

int callback_annotation(command_option_t *option, const char *value);

int cmd_create_main(int argc, const char **argv);

#ifdef __cplusplus
}
#endif

#endif /* __CMD_CREATE_H */

