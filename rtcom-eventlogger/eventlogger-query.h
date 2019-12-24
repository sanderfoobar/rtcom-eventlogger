/**
 * Copyright (C) 2005-06 Nokia Corporation.
 * Contact: Naba Kumar <naba.kumar@nokia.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

/**
 * @file eventlogger-query.h
 * @brief Defines an RTComElQuery object.
 *
 * RTComElQuery desribes a query.
 */

#ifndef __QUERY_H
#define __QUERY_H

#include "rtcom-eventlogger/eventlogger-types.h"

#include <glib-object.h>

G_BEGIN_DECLS

#define RTCOM_TYPE_EL_QUERY             (rtcom_el_query_get_type ())
#define RTCOM_EL_QUERY(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), RTCOM_TYPE_EL_QUERY, RTComElQuery))
#define RTCOM_EL_QUERY_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), RTCOM_TYPE_EL_QUERY, RTComElQueryClass))
#define RTCOM_IS_EL_QUERY(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), RTCOM_TYPE_EL_QUERY))
#define RTCOM_IS_EL_QUERY_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), RTCOM_TYPE_EL_QUERY))
#define RTCOM_EL_QUERY_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), RTCOM_TYPE_EL_QUERY, RTComElQueryClass))

typedef struct _RTComElQueryClass RTComElQueryClass;
typedef struct _RTComElQuery RTComElQuery;

#include "rtcom-eventlogger/eventlogger.h"

struct _RTComElQueryClass
{
    GObjectClass parent_class;
};

struct _RTComElQuery
{
    GObject parent_instance;
};

typedef enum
{
    RTCOM_EL_QUERY_GROUP_BY_NONE,
    RTCOM_EL_QUERY_GROUP_BY_CONTACT,
    RTCOM_EL_QUERY_GROUP_BY_UIDS,
    RTCOM_EL_QUERY_GROUP_BY_GROUP
} RTComElQueryGroupBy;

GType rtcom_el_query_get_type(void) G_GNUC_CONST;

/**
 * Creates a new #RTComElQuery.
 * @param el The #RTComEl this query operates on
 * @return A newly allocated RTComElQuery.
 */
RTComElQuery * rtcom_el_query_new(
        RTComEl * el);

/**
 * Sets the is-caching property
 * @param query The #RTComElQuery
 * @param is_caching The value that will be used to set the is-caching prop
 */
void rtcom_el_query_set_is_caching(
        RTComElQuery * query,
        gboolean is_caching);

/**
 * Sets the limit property
 * @param query The #RTComElQuery
 * @param limit The LIMIT for the query
 */
void rtcom_el_query_set_limit(
        RTComElQuery * query,
        gint limit);

/**
 * Sets the offset property
 * @param query The #RTComElQuery
 * @param offset The OFFSET for the query
 */
void rtcom_el_query_set_offset(
        RTComElQuery * query,
        gint offset);

/**
 * Sets the group property
 * @param query The #RTComElQuery
 * @param group A gboolean value, telling wheter you want to group
 * the events by their group_uid.
 */
void rtcom_el_query_set_group_by(
        RTComElQuery * query,
        RTComElQueryGroupBy group_by);

/**
 * Re-prepares the query leaving the WHERE clauses unchanged.
 * This should be used just after changing the limit, offset or group
 * properties of the query, in order to rebuild the sql.
 * @param query The #RTComElQuery object
 * @return TRUE in case of success, FALSE otherwise
 * @see rtcom_el_query_prepare
 */
gboolean rtcom_el_query_refresh(
        RTComElQuery * query);

/** Prepares the query.
 * The syntax is the following:
 * serie of 3-argument where:
 *   arg 1: column name
 *   arg 2: value
 *   arg 3: operation (see RTComElOp enumeration)
 * E.g.:
 * @code
 * rtcom_el_query_prepare(
 *    query,
 *
 *    "service-id",
 *    event_logger_get_service_id(“RTCOM_EL_SERVICE_SMS”),
 *    RTCOM_EL_OP_EQUAL,
 *
 *    "event-type-id",
 *    event_logger_get_eventtype_id(“RTCOM_EL_EVENTTYPE_SMS_INBOUND”),
 *    RTCOM_EL_OP_EQUAL,
 *
 *    "local-uid",
 *    “555-123456”,
 *    RTCOM_EL_OP_EQUAL,
 *
 *    "storage-time",
 *    1324183274,
 *    RTCOM_EL_OP_GREATER
 *
 *    NULL);
 * @endcode
 *
 * This will return all the events relative to incoming SMSs from the
 * number 555-123456, after the time 1324183274.
 *
 * You can also get the service and the event type by name, like:
 *
 * @code
 * "service", "SOME_SERVICE_NAME", RTCOM_EL_OP_EQUAL,
 * "event-type", "SOME_EVENTTYPE_NAME", RTCOM_EL_OP_EQUAL
 * @endcode
 *
 * Then you can use array of strings to get multiple values. E.g.:
 *
 * @code
 * const gchar * channels[] = {"Foo", "Bar", NULL};
 * const gchar * services[] = {"S1", S2", "S3", NULL};
 * rtcom_el_query_prepare(
 *     query,
 *     "channel", channels, RTCOM_EL_OP_IN_STRV,
 *     "service", services, RTCOM_EL_OP_IN_STRV);
 * @endcode
 *
 * @param query The #RTComElQuery object
 * @return TRUE in case of success, FALSE otherwise
 */
gboolean rtcom_el_query_prepare(
        RTComElQuery * query,
        ...);

/**
 * Gets the sql generated by this query
 * @param query The #RTComElQuery
 * @return the generated sql
 */
const gchar * rtcom_el_query_get_sql(
        RTComElQuery * query);

/**
 * Gets the WHERE clause generated by this query, i.e. a pointer to the
 * internal SQL string, starting after the word 'WHERE'.
 * @param query The #RTComElQuery
 * @return the WHERE clause
 */
const gchar * rtcom_el_query_get_where_clause(
        RTComElQuery * query);

/* Takes an opaque pointer containing reference to sqlite3
 * statement, and returns a key(string)->GValue(int/string) map
 * with the row values. The returned GHashTable can be freed
 * with g_hash_table_destroy(). */
GHashTable *rtcom_el_query_get_row(gpointer stmt);

/* Updates row values. Useful when iterating over a large number
 * of rows as it avoids hash table repeated destruction/creation. */
void rtcom_el_query_update_row(gpointer stmt, GHashTable *row);

G_END_DECLS

#endif

/* vim: set ai et tw=75 ts=4 sw=4: */

