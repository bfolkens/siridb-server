/*
 * db.c - contains functions  and constants for a SiriDB database.
 *
 * author       : Jeroen van der Heijden
 * email        : jeroen@transceptor.technology
 * copyright    : 2016, Transceptor Technology
 *
 * changes
 *  - initial version, 10-03-2016
 *
 */
#include <siri/db/db.h>
#include <logger/logger.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <siri/db/time.h>
#include <siri/db/user.h>
#include <uuid/uuid.h>
#include <siri/db/series.h>

static siridb_t * siridb_new(void);
static void siridb_free(siridb_t * siridb);
static void siridb_add(siridb_list_t * siridb_list, siridb_t * siridb);

siridb_list_t * siridb_new_list(void)
{
    siridb_list_t * siridb_list;
    siridb_list = (siridb_list_t *) malloc(sizeof(siridb_list_t));
    siridb_list->siridb = NULL;
    siridb_list->next = NULL;

    return siridb_list;
}

void siridb_free_list(siridb_list_t * siridb_list)
{
    siridb_list_t * next;
    while (siridb_list != NULL)
    {
        next = siridb_list->next;
        siridb_free(siridb_list->siridb);
        free(siridb_list);
        siridb_list = next;
    }
}

int siridb_add_from_unpacker(
        siridb_list_t * siridb_list,
        qp_unpacker_t * unpacker,
        siridb_t ** siridb,
        char * err_msg)
{
    qp_obj_t * qp_obj = qp_new_object();

    if (!qp_is_array(qp_next(unpacker, NULL)) ||
            qp_next(unpacker, qp_obj) != QP_INT64)
    {
        sprintf(err_msg, "error: corrupted database file.");
        qp_free_object(qp_obj);
        return 1;
    }

    /* check schema */
    if (qp_obj->via->int64 != 1)
    {
        sprintf(err_msg, "error: unsupported schema found: %ld",
                qp_obj->via->int64);
        qp_free_object(qp_obj);
        return 1;
    }

    /* create a new SiriDB structure */
    *siridb = siridb_new();

    /* read uuid */
    if (qp_next(unpacker, qp_obj) != QP_RAW ||
            qp_obj->len != 16)
    {
        sprintf(err_msg, "error: cannot read uuid.");
        siridb_free(*siridb);
        qp_free_object(qp_obj);
        return 1;
    }

    /* copy uuid */
    memcpy(&(*siridb)->uuid, qp_obj->via->raw, qp_obj->len);

    /* read database name */
    if (qp_next(unpacker, qp_obj) != QP_RAW ||
            qp_obj->len >= SIRIDB_MAX_DBNAME_LEN)
    {
        sprintf(err_msg, "error: cannot read database name.");
        siridb_free(*siridb);
        qp_free_object(qp_obj);
        return 1;
    }

    /* alloc mem for database name */
    (*siridb)->dbname = (char *) malloc(qp_obj->len + 1);

    /* copy datatabase name */
    memcpy((*siridb)->dbname, qp_obj->via->raw, qp_obj->len);

    /* set terminator */
    (*siridb)->dbname[qp_obj->len] = 0;

    /* read time precision */
    if (qp_next(unpacker, qp_obj) != QP_INT64 ||
            qp_obj->via->int64 < SIRIDB_TIME_SECONDS ||
            qp_obj->via->int64 > SIRIDB_TIME_NANOSECONDS)
    {
        sprintf(err_msg, "error: cannot read time-precision.");
        siridb_free(*siridb);
        qp_free_object(qp_obj);
        return 1;
    }

    /* bind time precision to SiriDB */
    (*siridb)->time_precision = (siridb_time_t) qp_obj->via->int64;

    /* read buffer size */
    if (qp_next(unpacker, qp_obj) != QP_INT64)
    {
        sprintf(err_msg, "error: cannot buffer size.");
        siridb_free(*siridb);
        qp_free_object(qp_obj);
        return 1;
    }

    /* bind buffer size to SiriDB */
    (*siridb)->buffer_size = (size_t) qp_obj->via->int64;

    /* add SiriDB to list */
    siridb_add(siridb_list, *siridb);

    /* free qp_object */
    qp_free_object(qp_obj);

    return 0;
}

siridb_t * siridb_get(siridb_list_t * siridb_list, const char * dbname)
{
    siridb_list_t * current = siridb_list;
    size_t len = strlen(dbname);
    const char * chk;

    if (current->siridb == NULL)
        return NULL;

    while (current != NULL)
    {
        chk = current->siridb->dbname;

        if (strlen(chk) == len && strncmp(chk, dbname, len) == 0)
            return current->siridb;

        current = current->next;
    }

    return NULL;
}

static siridb_t * siridb_new(void)
{
    siridb_t * siridb;
    siridb = (siridb_t *) malloc(sizeof(siridb_t));
    siridb->dbname = NULL;
    siridb->dbpath = NULL;
    siridb->buffer_path = NULL;
    siridb->time_precision = -1;
    siridb->users = NULL;
    siridb->servers = NULL;
    siridb->pools = NULL;
    siridb->max_series_id = 0;
    siridb->series = ct_new();
    siridb->series_map = imap32_new();
    siridb->buffer_size = -1;
    siridb->buffer_fp = NULL; /* make sure this is NULL when file is closed */
    return siridb;
}

static void siridb_free(siridb_t * siridb)
{
    if (siridb != NULL)
    {
        log_debug("Free database '%s'...", siridb->dbname);

        free(siridb->dbname);

        /* only free buffer path when not equal to db_path */
        if (siridb->buffer_path != siridb->dbpath)
            free(siridb->buffer_path);
        free(siridb->dbpath);

        siridb_free_users(siridb->users);

        /* we do not need to free server and replica since they exist in
         * this list and therefore will be freed.
         */
        siridb_free_servers(siridb->servers);

        siridb_free_pools(siridb->pools);

        /* free c-tree lookup */
        ct_free(siridb->series);

        /* free series using imap32 */
        imap32_walk(siridb->series_map, (imap32_cb_t) &siridb_free_series, NULL);

        /* free imap32 */
        imap32_free(siridb->series_map);

        if (siridb->buffer_fp != NULL)
            fclose(siridb->buffer_fp);

        free(siridb);
    }
}

static void siridb_add(siridb_list_t * siridb_list, siridb_t * siridb)
{
    if (siridb_list->siridb == NULL)
    {
        siridb_list->siridb = siridb;
        return;
    }

    while (siridb_list->next != NULL)
        siridb_list = siridb_list->next;

    siridb_list->next = (siridb_list_t *) malloc(sizeof(siridb_list_t));
    siridb_list->next->siridb = siridb;
    siridb_list->next->next = NULL;
}
