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
 * Description: provide console definition
 ******************************************************************************/
#include <unistd.h>
#include <sys/epoll.h>
#include <sys/types.h>
#include <limits.h>
#include <termios.h>
#include <pthread.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <string.h>

#include "console.h"
#include "mainloop.h"
#include "log.h"
#include "utils.h"
#include "constants.h"

static ssize_t fifo_write_function(void *context, const void *data, size_t len)
{
    ssize_t ret;
    int fd;

    fd = *(int *)context;
    ret = util_write_nointr(fd, data, len);
    // Ignore EAGAIN to prevent hang, do not report error
    if (errno == EAGAIN) {
        return (ssize_t)len;
    }

    if ((ret <= 0) || (ret != (ssize_t)len)) {
        ERROR("Failed to write %d: %s", fd, strerror(errno));
        return -1;
    }
    return ret;
}

static ssize_t fd_write_function(void *context, const void *data, size_t len)
{
    ssize_t ret;
    ret = util_write_nointr(*(int *)context, data, len);
    if ((ret <= 0) || (ret != (ssize_t)len)) {
        ERROR("Failed to write: %s", strerror(errno));
        return -1;
    }
    return ret;
}

/* console cb tty fifoin */
static int console_cb_tty_stdin_with_escape(int fd, uint32_t events, void *cbdata,
                                            struct epoll_descr *descr)
{
    struct tty_state *ts = cbdata;
    char c;
    int ret = 0;
    ssize_t r_ret, w_ret;

    if (fd != ts->stdin_reader) {
        ret = 1;
        goto out;
    }

    r_ret = util_read_nointr(ts->stdin_reader, &c, 1);
    if (r_ret <= 0) {
        ret = 1;
        goto out;
    }

    if (ts->tty_exit != -1) {
        if (c == ts->tty_exit && !(ts->saw_tty_exit)) {
            ts->saw_tty_exit = 1;
            goto out;
        }

        if (c == 'q' && ts->saw_tty_exit) {
            ret = 1;
            goto out;
        }

        ts->saw_tty_exit = 0;
    }

    if (ts->stdin_writer.context && ts->stdin_writer.write_func) {
        w_ret = ts->stdin_writer.write_func(ts->stdin_writer.context, &c, 1);
        if ((w_ret <= 0) || (w_ret != r_ret)) {
            ret = 1;
            goto out;
        }
    }

out:
    return ret;
}


static int console_writer_write_data(const struct io_write_wrapper *writer, const char *buf, ssize_t len)
{
    ssize_t ret;

    if (writer == NULL || writer->context == NULL || writer->write_func == NULL || len <= 0) {
        return 0;
    }
    ret = writer->write_func(writer->context, buf, (size_t)len);
    if (ret <= 0 || ret != len) {
        ERROR("failed to write, error:%s", strerror(errno));
        return -1;
    }
    return 0;
}

/* console cb tty fifoin */
static int console_cb_stdio_copy(int fd, uint32_t events, void *cbdata, struct epoll_descr *descr)
{
    struct tty_state *ts = cbdata;
    char *buf = NULL;
    size_t buf_len = MAX_MSG_BUFFER_SIZE;
    int ret = 0;
    ssize_t r_ret;

    buf = util_common_calloc_s(buf_len);
    if (buf == NULL) {
        ERROR("Out of memory");
        return -1;
    }

    if (fd == ts->sync_fd) {
        ret = 1;
        goto out;
    }

    if (fd != ts->stdin_reader && fd != ts->stdout_reader && fd != ts->stderr_reader) {
        ret = 1;
        goto out;
    }

    r_ret = util_read_nointr(fd, buf, buf_len - 1);
    if (r_ret <= 0) {
        ret = 1;
        goto out;
    }

    if (fd == ts->stdin_reader) {
        if (console_writer_write_data(&ts->stdin_writer, buf, r_ret) != 0) {
            ret = 1;
            goto out;
        }
    }

    if (fd == ts->stdout_reader) {
        if (console_writer_write_data(&ts->stdout_writer, buf, r_ret) != 0) {
            ret = 1;
            goto out;
        }
    }

    if (fd == ts->stderr_reader) {
        if (console_writer_write_data(&ts->stderr_writer, buf, r_ret) != 0) {
            ret = 1;
            goto out;
        }
    }

out:
    free(buf);
    return ret;
}

/* console fifo name */
int console_fifo_name(const char *rundir, const char *subpath,
                      const char *stdflag,
                      char *fifo_name, size_t fifo_name_sz,
                      char *fifo_path, size_t fifo_path_sz, bool do_mkdirp)
{
    int ret = 0;
    int nret = 0;

    nret = snprintf(fifo_path, fifo_path_sz, "%s/%s/", rundir, subpath);
    if (nret < 0 || (size_t)nret >= fifo_path_sz) {
        ERROR("FIFO path:%s/%s/ is too long.", rundir, subpath);
        ret = -1;
        goto out;
    }

    if (do_mkdirp) {
        ret = util_mkdir_p(fifo_path, CONSOLE_FIFO_DIRECTORY_MODE);
        if (ret < 0) {
            COMMAND_ERROR("Unable to create console fifo directory %s: %s.", fifo_path, strerror(errno));
            goto out;
        }
    }

    nret = snprintf(fifo_name, fifo_name_sz, "%s/%s/%s-fifo", rundir, subpath, stdflag);
    if (nret < 0 || (size_t)nret >= fifo_name_sz) {
        ERROR("FIFO name %s/%s/%s-fifo is too long.", rundir, subpath, stdflag);
        ret = -1;
        goto out;
    }

out:
    return ret;
}

/* console fifo create */
int console_fifo_create(const char *fifo_path)
{
    int ret;

    ret = mknod(fifo_path, S_IFIFO | S_IRUSR | S_IWUSR, (dev_t)0);
    if (ret < 0 && errno != EEXIST) {
        ERROR("Failed to mknod monitor fifo %s: %s.", fifo_path, strerror(errno));
        return -1;
    }

    return 0;
}

/* console fifo delete */
int console_fifo_delete(const char *fifo_path)
{
    char *ret = NULL;
    char real_path[PATH_MAX + 1] = { 0x00 };

    if (fifo_path == NULL || strlen(fifo_path) > PATH_MAX) {
        ERROR("Invalid input!");
        return -1;
    }

    if (strlen(fifo_path) == 0) {
        return 0;
    }

    ret = realpath(fifo_path, real_path);
    if (ret == NULL) {
        if (errno != ENOENT) {
            ERROR("Failed to get real path: %s", fifo_path);
            return -1;
        }
        return 0;
    }

    if (unlink(real_path) && errno != ENOENT) {
        WARN("Unlink %s failed", real_path);
        return -1;
    }
    return 0;
}

/* console fifo open */
int console_fifo_open(const char *fifo_path, int *fdout, int flags)
{
    int fd = 0;

    fd = util_open(fifo_path, O_RDONLY | O_NONBLOCK, (mode_t)0);
    if (fd < 0) {
        ERROR("Failed to open fifo %s to send message: %s.", fifo_path,
              strerror(errno));
        return -1;
    }

    *fdout = fd;
    return 0;
}

/* console fifo open withlock */
int console_fifo_open_withlock(const char *fifo_path, int *fdout, int flags)
{
    int fd = 0;
    struct flock lk;

    fd = util_open(fifo_path, flags, 0);
    if (fd < 0) {
        WARN("Failed to open fifo %s to send message: %s.", fifo_path, strerror(errno));
        return -1;
    }

    lk.l_type = F_WRLCK;
    lk.l_whence = SEEK_SET;
    lk.l_start = 0;
    lk.l_len = 0;
    if (fcntl(fd, F_SETLK, &lk) != 0) {
        /* another console instance is already running, don't start up */
        DEBUG("Another console instance already running with path : %s.", fifo_path);
        close(fd);
        return -1;
    }

    *fdout = fd;
    return 0;
}

/* console fifo close */
void console_fifo_close(int fd)
{
    close(fd);
}

/* setup tios */
int setup_tios(int fd, struct termios *curr_tios)
{
    struct termios tmptios;
    int ret = 0;

    if (!isatty(fd)) {
        ERROR("Specified fd: '%d' is not a tty", fd);
        return -1;
    }

    if (tcgetattr(fd, curr_tios)) {
        ERROR("Failed to get current terminal settings");
        ret = -1;
        goto out;
    }

    tmptios = *curr_tios;

    cfmakeraw(&tmptios);
    tmptios.c_oflag |= OPOST;

    if (tcsetattr(fd, TCSAFLUSH, &tmptios)) {
        ERROR("Set terminal settings failed");
        ret = -1;
        goto out;
    }

out:
    return ret;
}

static void client_console_tty_state_close(struct epoll_descr *descr, const struct tty_state *ts)
{
    if (ts->stdin_reader >= 0) {
        epoll_loop_del_handler(descr, ts->stdin_reader);
    }
    if (ts->stdout_reader >= 0) {
        epoll_loop_del_handler(descr, ts->stdout_reader);
    }
    if (ts->stderr_reader >= 0) {
        epoll_loop_del_handler(descr, ts->stderr_reader);
    }
}

/* console loop */
/* data direction: */
/* read stdinfd, write fifoinfd */
/* read fifooutfd, write stdoutfd */
/* read stderrfd, write stderrfd */
int client_console_loop(int stdinfd, int stdoutfd, int stderrfd,
                        int fifoinfd, int fifooutfd, int fifoerrfd, int tty_exit, bool tty)
{
    int ret;
    struct epoll_descr descr;
    struct tty_state ts;

    ret = epoll_loop_open(&descr);
    if (ret) {
        ERROR("Create epoll_loop error");
        return ret;
    }

    ts.tty_exit = tty_exit;
    ts.saw_tty_exit = 0;
    ts.sync_fd = -1;
    ts.stdin_reader = -1;
    ts.stdout_reader = -1;
    ts.stderr_reader = -1;

    if (fifoinfd >= 0) {
        ts.stdin_reader = stdinfd;
        ts.stdin_writer.context = &fifoinfd;
        ts.stdin_writer.write_func = fd_write_function;
        if (tty) {
            ret = epoll_loop_add_handler(&descr, ts.stdin_reader, console_cb_tty_stdin_with_escape, &ts);
            if (ret) {
                INFO("Add handler for stdinfd faied. with error %s", strerror(errno));
            }
        } else {
            ret = epoll_loop_add_handler(&descr, ts.stdin_reader, console_cb_stdio_copy, &ts);
            if (ret) {
                INFO("Add handler for stdinfd faied. with error %s", strerror(errno));
            }
        }
    }

    if (fifooutfd >= 0) {
        ts.stdout_reader = fifooutfd;
        ts.stdout_writer.context = &stdoutfd;
        ts.stdout_writer.write_func = fd_write_function;
        ret = epoll_loop_add_handler(&descr, ts.stdout_reader, console_cb_stdio_copy, &ts);
        if (ret) {
            ERROR("Add handler for masterfd failed");
            goto err_out;
        }
    }

    if (fifoerrfd >= 0) {
        ts.stderr_reader = fifoerrfd;
        ts.stderr_writer.context = &stderrfd;
        ts.stderr_writer.write_func = fd_write_function;
        ret = epoll_loop_add_handler(&descr, ts.stderr_reader, console_cb_stdio_copy, &ts);
        if (ret) {
            ERROR("Add handler for masterfd failed");
            goto err_out;
        }
    }

    ret = epoll_loop(&descr, -1);
    if (ret) {
        ERROR("Epoll_loop error");
        goto err_out;
    }

    ret = 0;

err_out:
    client_console_tty_state_close(&descr, &ts);
    epoll_loop_close(&descr);
    return ret;
}

/* console loop copy */
static int console_loop_io_copy(int sync_fd, const int *srcfds, struct io_write_wrapper *writers, size_t len)
{
    int ret = 0;
    size_t i = 0;
    struct epoll_descr descr;
    struct tty_state *ts = NULL;
    if (len > (SIZE_MAX / sizeof(struct tty_state)) - 1) {
        ERROR("Invalid io size");
        return -1;
    }
    ts = util_common_calloc_s(sizeof(struct tty_state) * (len + 1));
    if (ts == NULL) {
        ERROR("Out of memory");
        return -1;
    }

    ret = epoll_loop_open(&descr);
    if (ret) {
        ERROR("Create epoll_loop error");
        free(ts);
        return ret;
    }

    for (i = 0; i < len; i++) {
        // Reusing ts.stdout_reader and ts.stdout_writer for coping io
        ts[i].stdout_reader = srcfds[i];
        ts[i].stdout_writer.context = writers[i].context;
        ts[i].stdout_writer.write_func = writers[i].write_func;
        ts[i].sync_fd = -1;
        ret = epoll_loop_add_handler(&descr, ts[i].stdout_reader, console_cb_stdio_copy, &ts[i]);
        if (ret != 0) {
            ERROR("Add handler for masterfd failed");
            goto err_out;
        }
    }
    if (sync_fd >= 0) {
        ts[i].sync_fd = sync_fd;
        epoll_loop_add_handler(&descr, ts[i].sync_fd, console_cb_stdio_copy, &ts[i]);
        if (ret) {
            ERROR("Add handler for syncfd failed");
            goto err_out;
        }
    }

    ret = epoll_loop(&descr, -1);
    if (ret != 0) {
        ERROR("Epoll_loop error");
        goto err_out;
    }

err_out:

    for (i = 0; i < (len + 1); i++) {
        epoll_loop_del_handler(&descr, ts[i].stdout_reader);
    }
    epoll_loop_close(&descr);
    free(ts);
    return ret;
}

struct io_copy_thread_arg {
    struct io_copy_arg *copy_arg;
    bool detach;
    size_t len;
    int sync_fd;
    sem_t wait_sem;
};

static int io_copy_init_fds(size_t len, int **infds, int **outfds, int **srcfds, struct io_write_wrapper **writers)
{
    size_t i;

    if (len > SIZE_MAX / sizeof(struct io_write_wrapper)) {
        ERROR("Invalid arguments");
        return -1;
    }
    *srcfds = util_common_calloc_s(sizeof(int) * len);
    if (*srcfds == NULL) {
        ERROR("Out of memory");
        return -1;
    }

    *infds = util_common_calloc_s(sizeof(int) * len);
    if (*infds == NULL) {
        ERROR("Out of memory");
        return -1;
    }
    for (i = 0; i < len; i++) {
        (*infds)[i] = -1;
    }
    *outfds = util_common_calloc_s(sizeof(int) * len);
    if (*outfds == NULL) {
        ERROR("Out of memory");
        return -1;
    }
    for (i = 0; i < len; i++) {
        (*outfds)[i] = -1;
    }

    *writers = util_common_calloc_s(sizeof(struct io_write_wrapper) * len);
    if (*writers == NULL) {
        ERROR("Out of memory");
        return -1;
    }
    return 0;
}

static int io_copy_make_srcfds(size_t len, struct io_copy_arg *copy_arg, int *infds, int *srcfds)
{
    size_t i;

    for (i = 0; i < len; i++) {
        if (copy_arg[i].srctype == IO_FIFO) {
            if (console_fifo_open((const char *)copy_arg[i].src, &(infds[i]), O_RDONLY | O_NONBLOCK)) {
                ERROR("failed to open console fifo.");
                return -1;
            }
            srcfds[i] = infds[i];
        } else if (copy_arg[i].srctype == IO_FD) {
            srcfds[i] = *(int *)(copy_arg[i].src);
        } else {
            ERROR("Got invalid src fd type");
            return -1;
        }
    }
    return 0;
}

static int io_copy_make_dstfds(size_t len, struct io_copy_arg *copy_arg, int *outfds,
                               struct io_write_wrapper *writers)
{
    size_t i;

    for (i = 0; i < len; i++) {
        if (copy_arg[i].dsttype == IO_FIFO) {
            if (console_fifo_open_withlock((const char *)copy_arg[i].dst, &outfds[i], O_RDWR | O_NONBLOCK)) {
                ERROR("Failed to open console fifo.");
                return -1;
            }
            writers[i].context = &outfds[i];
            writers[i].write_func = fifo_write_function;
        } else if (copy_arg[i].dsttype == IO_FD) {
            writers[i].context = copy_arg[i].dst;
            writers[i].write_func = fd_write_function;
        } else if (copy_arg[i].dsttype == IO_FUNC) {
            struct io_write_wrapper *io_write = copy_arg[i].dst;
            writers[i].context = io_write->context;
            writers[i].write_func = io_write->write_func;
            writers[i].close_func = io_write->close_func;
        } else {
            ERROR("Got invalid dst fd type");
            return -1;
        }
    }
    return 0;
}

static void io_copy_thread_cleanup(struct io_write_wrapper *writers, struct io_copy_thread_arg *thread_arg,
                                   int *infds, int *outfds, int *srcfds, size_t len)
{
    size_t i = 0;
    for (i = 0; i < len; i++) {
        if (writers != NULL && writers[i].close_func != NULL) {
            (void)writers[i].close_func(writers[i].context, NULL);
        }
    }
    free(srcfds);
    for (i = 0; i < len; i++) {
        if ((infds != NULL) && (infds[i] >= 0)) {
            console_fifo_close(infds[i]);
        }
        if ((outfds != NULL) && (outfds[i] >= 0)) {
            console_fifo_close(outfds[i]);
        }
    }
    free(infds);
    free(outfds);
    free(writers);
}

static void *io_copy_thread_main(void *arg)
{
    int ret = -1;
    struct io_copy_thread_arg *thread_arg = (struct io_copy_thread_arg *)arg;
    struct io_copy_arg *copy_arg = thread_arg->copy_arg;
    size_t len = 0;
    int *infds = NULL;
    int *outfds = NULL; // recored fds to close
    int *srcfds = NULL;
    struct io_write_wrapper *writers = NULL;
    int sync_fd = thread_arg->sync_fd;
    bool posted = false;

    if (thread_arg->detach) {
        ret = pthread_detach(pthread_self());
        if (ret != 0) {
            CRIT("Set thread detach fail");
            goto err;
        }
    }

    (void)prctl(PR_SET_NAME, "IoCopy");

    len = thread_arg->len;
    if (io_copy_init_fds(len, &infds, &outfds, &srcfds, &writers) != 0) {
        goto err;
    }

    if (io_copy_make_srcfds(len, copy_arg, infds, srcfds) != 0) {
        goto err;
    }

    if (io_copy_make_dstfds(len, copy_arg, outfds, writers) != 0) {
        goto err;
    }

    sem_post(&thread_arg->wait_sem);
    posted = true;
    (void)console_loop_io_copy(sync_fd, srcfds, writers, len);
err:
    if (!posted) {
        sem_post(&thread_arg->wait_sem);
    }
    io_copy_thread_cleanup(writers, thread_arg, infds, outfds, srcfds, len);
    return NULL;
}

int start_io_copy_thread(int sync_fd, bool detach, struct io_copy_arg *copy_arg, size_t len, pthread_t *tid)
{
    int res = 0;
    struct io_copy_thread_arg thread_arg;

    if (copy_arg == NULL || len == 0) {
        return 0;
    }

    thread_arg.detach = detach;
    thread_arg.copy_arg = copy_arg;
    thread_arg.len = len;
    thread_arg.sync_fd = sync_fd;
    if (sem_init(&thread_arg.wait_sem, 0, 0)) {
        ERROR("Failed to init start semaphore");
        return -1;
    }

    res = pthread_create(tid, NULL, io_copy_thread_main, (void *)(&thread_arg));
    if (res != 0) {
        CRIT("Thread creation failed");
        return -1;
    }
    sem_wait(&thread_arg.wait_sem);
    sem_destroy(&thread_arg.wait_sem);
    return 0;
}

