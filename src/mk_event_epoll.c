/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*  Monkey HTTP Server
 *  ==================
 *  Copyright 2001-2015 Monkey Software LLC <eduardo@monkey.io>
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 */

#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <sys/timerfd.h>

#include <monkey/mk_event.h>
#include <monkey/mk_memory.h>
#include <monkey/mk_utils.h>

static inline void *_mk_event_loop_create(int size)
{
    mk_event_ctx_t *ctx;

    /* Main event context */
    ctx = mk_mem_malloc_z(sizeof(mk_event_ctx_t));
    if (!ctx) {
        return NULL;
    }

    /* Create the epoll instance */
    ctx->efd = epoll_create1(EPOLL_CLOEXEC);
    if (ctx->efd == -1) {
        mk_libc_error("epoll_create");
        mk_mem_free(ctx);
        return NULL;
    }

    /* Allocate space for events queue */
    ctx->events = mk_mem_malloc_z(sizeof(struct epoll_event) * size);
    if (!ctx->events) {
        close(ctx->efd);
        mk_mem_free(ctx);
        return NULL;
    }
    ctx->queue_size = size;
    return ctx;
}

/* Close handlers and memory */
static inline void _mk_event_loop_destroy(mk_event_ctx_t *ctx)
{
    close(ctx->efd);
    mk_mem_free(ctx->events);
    mk_mem_free(ctx);
}

/*
 * It register certain events for the file descriptor in question, if
 * the file descriptor have not been registered, create a new entry.
 */
static inline int _mk_event_add(mk_event_ctx_t *ctx, int fd, int events)
{
    int op;
    int ret;
    struct mk_event_fd_state *fds;
    struct epoll_event event = {0, {0}};

    /* Verify the FD status and desired operation */
    fds = mk_event_get_state(fd);
    if (fds->mask == MK_EVENT_EMPTY) {
        op = EPOLL_CTL_ADD;
    }
    else {
        op = EPOLL_CTL_MOD;
    }

    event.data.fd = fd;
    event.events = EPOLLERR | EPOLLHUP | EPOLLRDHUP;

    if (events & MK_EVENT_READ) {
        event.events |= EPOLLIN;
    }
    if (events & MK_EVENT_WRITE) {
        event.events |= EPOLLOUT;
    }

    ret = epoll_ctl(ctx->efd, op, fd, &event);
    if (ret < 0) {
        mk_libc_error("epoll_ctl");
        return -1;
    }

    fds->mask = events;
    return ret;
}

/* Delete an event */
static inline int _mk_event_del(mk_event_ctx_t *ctx, int fd)
{
    int ret;

    ret = epoll_ctl(ctx->efd, EPOLL_CTL_DEL, fd, NULL);
    MK_TRACE("[FD %i] Epoll, remove from QUEUE_FD=%i, ret=%i",
             fd, ctx->efd, ret);
    if (ret < 0) {
        mk_libc_error("epoll_ctl");
    }

    return ret;
}

/* Register a timeout file descriptor */
static inline int _mk_event_timeout_create(mk_event_ctx_t *ctx, int expire)
{
    int ret;
    int timer_fd;
    struct itimerspec its;

    /* expiration interval */
    its.it_interval.tv_sec  = expire;
    its.it_interval.tv_nsec = 0;

    /* initial expiration */
    its.it_value.tv_sec  = time(NULL) + expire;
    its.it_value.tv_nsec = 0;

    timer_fd = timerfd_create(CLOCK_REALTIME, 0);
    if (timer_fd == -1) {
        mk_libc_error("timerfd");
        return -1;
    }

    ret = timerfd_settime(timer_fd, TFD_TIMER_ABSTIME, &its, NULL);
    if (ret < 0) {
        mk_libc_error("timerfd_settime");
        return -1;
    }

    /* register the timer into the epoll queue */
    ret = _mk_event_add(ctx, timer_fd, MK_EVENT_READ);
    if (ret != 0) {
        close(timer_fd);
        return ret;
    }

    return timer_fd;
}

static inline int _mk_event_channel_create(mk_event_ctx_t *ctx, int *r_fd, int *w_fd)
{
    int fd;
    int ret;

    fd = eventfd(0, EFD_CLOEXEC);
    if (fd == -1) {
        mk_libc_error("eventfd");
        return -1;
    }

    ret = _mk_event_add(ctx, fd, MK_EVENT_READ);
    if (ret != 0) {
        close(fd);
        return ret;
    }

    *w_fd = *r_fd = fd;
    return 0;
}

static inline int _mk_event_wait(mk_event_loop_t *loop)
{
    mk_event_ctx_t *ctx = loop->data;

    loop->n_events = epoll_wait(ctx->efd, ctx->events, ctx->queue_size, -1);
    return loop->n_events;
}

static inline int _mk_event_translate(mk_event_loop_t *loop)
{
    int i;
    int fd;
    struct mk_event_fd_state *st;
    mk_event_ctx_t *ctx = loop->data;

    for (i = 0; i < loop->n_events; i++) {
        fd = ctx->events[i].data.fd;
        st = &mk_events_fdt->states[fd];

        loop->events[i].fd   = fd;
        loop->events[i].mask = ctx->events[i].events;
        loop->events[i].data = st->data;
    }

    return loop->n_events;
}

static inline char *_mk_event_backend()
{
    return "epoll";
}
