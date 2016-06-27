/*
 * promise.h - Promise SiriDB.
 *
 * author       : Jeroen van der Heijden
 * email        : jeroen@transceptor.technology
 * copyright    : 2016, Transceptor Technology
 *
 *
 * changes
 *  - initial version, 21-06-2016
 *
 */
#pragma once

#include <siri/net/pkg.h>
#include <siri/net/socket.h>

#define PROMISE_DEFAULT_TIMEOUT 10000  // 10 seconds

typedef enum sirinet_promise_status
{
    PROMISE_TMEOUT_ERROR=-3,
    PROMISE_WRITE_ERROR,
    PROMISE_CANCELLED_ERROR,
    PROMISE_SUCCESS=0
} sirinet_promise_status_t;

typedef struct sirinet_promise_s sirinet_promise_t;
typedef struct sirinet_promises_s sirinet_promises_t;

typedef void (* sirinet_promise_cb_t)(
        sirinet_promise_t * promise,
        const sirinet_pkg_t * pkg,
        int status);

typedef void (* sirinet_promises_cb_t)(
        slist_t * promises, void * data);

/* the callback will always be called and is responsible to free the promise */
typedef struct sirinet_promise_s
{
    uv_timer_t * timer;
    sirinet_promise_cb_t cb;
    siridb_server_t * server;
    uint64_t pid;
    void * data;
} sirinet_promise_t;

typedef struct sirinet_promises_s
{
    sirinet_promises_cb_t cb;
    slist_t * promises;
    void * data;
} sirinet_promises_t;

const char * sirinet_promise_strstatus(sirinet_promise_status_t status);
