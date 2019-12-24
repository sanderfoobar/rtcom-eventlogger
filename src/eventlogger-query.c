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

#include "rtcom-eventlogger/eventlogger-query.h"

#include <string.h>
#include <sqlite3.h>

#include "rtcom-eventlogger/db.h"

#define RTCOM_EL_QUERY_GET_PRIV(ev) (G_TYPE_INSTANCE_GET_PRIVATE ((ev), \
            RTCOM_TYPE_EL_QUERY, RTComElQueryPrivate))
G_DEFINE_TYPE(RTComElQuery, rtcom_el_query, G_TYPE_OBJECT);

typedef struct _RTComElQueryPrivate RTComElQueryPrivate;
struct _RTComElQueryPrivate {
    RTComEl * el;
    gboolean is_caching; /** Indicates if it's a query performed for caching purposes */
    gint limit;
    gint offset;
    RTComElQueryGroupBy group_by;

    /* Borrowed mapping of field name -> expected GType */
    GHashTable *typing;
    /* Borrowed mapping of field name -> SQL column name */
    GHashTable *mapping;

    GString * sql;
    gchar * where_clause;
};

static gboolean _build_where_clause(
        RTComElQuery * query,
        const gchar * key,
        gpointer val,
        RTComElOp op,
        GString *acc);

static const gchar * _build_operator(
        RTComElOp op);

enum
{
    RTCOM_EL_QUERY_PROP_0,
    RTCOM_EL_QUERY_PROP_EL,
    RTCOM_EL_QUERY_PROP_IS_CACHING,
    RTCOM_EL_QUERY_PROP_LIMIT,
    RTCOM_EL_QUERY_PROP_OFFSET,
    RTCOM_EL_QUERY_PROP_GROUP_BY
};

static void rtcom_el_query_set_property(
        GObject * obj,
        guint prop_id,
        const GValue * value,
        GParamSpec * pspec)
{
    RTComElQueryPrivate * priv = RTCOM_EL_QUERY_GET_PRIV(obj);

    switch(prop_id)
    {
        case RTCOM_EL_QUERY_PROP_EL:
            priv->el = g_value_get_pointer(value);
            break;

        case RTCOM_EL_QUERY_PROP_IS_CACHING:
            priv->is_caching = g_value_get_boolean(value);
            break;

        case RTCOM_EL_QUERY_PROP_LIMIT:
            priv->limit = g_value_get_int(value);
            break;

        case RTCOM_EL_QUERY_PROP_OFFSET:
            priv->offset = g_value_get_int(value);
            break;

        case RTCOM_EL_QUERY_PROP_GROUP_BY:
            priv->group_by = g_value_get_int(value);
            break;

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, prop_id, pspec);
            break;
    }
}

static void rtcom_el_query_get_property(
        GObject * obj,
        guint prop_id,
        GValue * value,
        GParamSpec * pspec)
{
    RTComElQueryPrivate * priv = RTCOM_EL_QUERY_GET_PRIV(obj);

    switch(prop_id)
    {
        case RTCOM_EL_QUERY_PROP_EL:
            g_value_set_pointer(value, priv->el);
            break;

        case RTCOM_EL_QUERY_PROP_IS_CACHING:
            g_value_set_boolean(value, priv->is_caching);
            break;

        case RTCOM_EL_QUERY_PROP_LIMIT:
            g_value_set_int(value, priv->limit);
            break;

        case RTCOM_EL_QUERY_PROP_OFFSET:
            g_value_set_int(value, priv->offset);
            break;

        case RTCOM_EL_QUERY_PROP_GROUP_BY:
            g_value_set_int(value, priv->group_by);
            break;

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, prop_id, pspec);
            break;
    }
}

static void rtcom_el_query_init(
        RTComElQuery * ev)
{
    RTComElQueryPrivate * priv = NULL;

    priv = RTCOM_EL_QUERY_GET_PRIV(ev);
    priv->el = NULL;
    priv->is_caching = FALSE;
    priv->limit = -1;
    priv->offset = 0;
    priv->group_by = RTCOM_EL_QUERY_GROUP_BY_NONE;
    priv->sql = NULL;
    priv->where_clause = NULL;

    rtcom_el_db_schema_get_mappings (NULL, &priv->mapping, &priv->typing);
}

static void rtcom_el_query_finalize(
        GObject * object)
{
    RTComElQueryPrivate * priv = RTCOM_EL_QUERY_GET_PRIV(object);

    if(priv->sql)
        g_string_free(priv->sql, TRUE);

    if(priv->where_clause)
        g_free(priv->where_clause);

    G_OBJECT_CLASS(rtcom_el_query_parent_class)->finalize(object);
}

static void rtcom_el_query_class_init(
        RTComElQueryClass * klass)
{
    GObjectClass* object_class = G_OBJECT_CLASS(klass);
    g_type_class_add_private(object_class, sizeof(RTComElQueryPrivate));
    object_class->finalize = rtcom_el_query_finalize;
    object_class->set_property = rtcom_el_query_set_property;
    object_class->get_property = rtcom_el_query_get_property;

    g_object_class_install_property(
            object_class,
            RTCOM_EL_QUERY_PROP_EL,
            g_param_spec_pointer(
                "el",
                "EventLogger",
                "The RTComEl this query operates on",
                G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

    g_object_class_install_property(
            object_class,
            RTCOM_EL_QUERY_PROP_IS_CACHING,
            g_param_spec_boolean(
                "is-caching",
                "It's a caching query",
                "Indicated whether this query is performed "
                  "for caching purposes",
                FALSE,
                G_PARAM_READWRITE));

    g_object_class_install_property(
            object_class,
            RTCOM_EL_QUERY_PROP_LIMIT,
            g_param_spec_int(
                "limit",
                "LIMIT",
                "The LIMIT for the query",
                -1, G_MAXINT, -1,
                G_PARAM_READWRITE));

    g_object_class_install_property(
            object_class,
            RTCOM_EL_QUERY_PROP_OFFSET,
            g_param_spec_int(
                "offset",
                "OFFSET",
                "The OFFSET for the query",
                0, G_MAXINT, 0,
                G_PARAM_READWRITE));

    g_object_class_install_property(
            object_class,
            RTCOM_EL_QUERY_PROP_GROUP_BY,
            g_param_spec_int(
                "group-by",
                "Group events by",
                "Group events by",
                RTCOM_EL_QUERY_GROUP_BY_NONE,
                RTCOM_EL_QUERY_GROUP_BY_GROUP,
                RTCOM_EL_QUERY_GROUP_BY_NONE,
                G_PARAM_READWRITE));
}

RTComElQuery * rtcom_el_query_new(
        RTComEl * el)
{
    return RTCOM_EL_QUERY(
            g_object_new(
                RTCOM_TYPE_EL_QUERY,
                "el", el,
                NULL));
}

void rtcom_el_query_set_is_caching(
        RTComElQuery * query,
        gboolean is_caching)
{
    g_object_set(
            G_OBJECT(query),
            "is-caching", is_caching,
            NULL);
}

void rtcom_el_query_set_limit(
        RTComElQuery * query,
        gint limit)
{
    g_object_set(
            G_OBJECT(query),
            "limit", limit,
            NULL);
}

void rtcom_el_query_set_offset(
        RTComElQuery * query,
        gint offset)
{
    g_object_set(
            G_OBJECT(query),
            "offset", offset,
            NULL);
}

void rtcom_el_query_set_group_by(
        RTComElQuery * query,
        RTComElQueryGroupBy group_by)
{
    g_object_set(
            G_OBJECT(query),
            "group-by", group_by,
            NULL);
}

gboolean rtcom_el_query_refresh(
        RTComElQuery * query)
{
    RTComElQueryPrivate * priv = NULL;
    const gchar *selection;

    g_return_val_if_fail(query, FALSE);

    priv = RTCOM_EL_QUERY_GET_PRIV(query);
    g_assert(priv);

    if(priv->sql)
        g_string_free(priv->sql, TRUE);

    rtcom_el_db_schema_get_mappings (&selection, NULL, NULL);

    priv->sql = g_string_sized_new (1024); /* size hint for performance */

    /* Use the caching data if available. */
    if (priv->group_by == RTCOM_EL_QUERY_GROUP_BY_GROUP)
    {
        g_string_printf (priv->sql, "SELECT %s FROM GroupCache "
            "JOIN Events ON GroupCache.event_id = Events.id "
            "JOIN Services ON GroupCache.service_id = Services.id "
            "JOIN EventTypes ON Events.event_type_id = EventTypes.id "
            "LEFT JOIN Remotes ON Events.remote_uid = Remotes.remote_uid "
                "AND Events.local_uid = Remotes.local_uid "
            "LEFT JOIN Headers ON Headers.event_id = Events.id "
                "AND Headers.name = 'message-token'", selection);

        if(priv->where_clause)
            g_string_append_printf(priv->sql, " WHERE %s", priv->where_clause);
    } else {
        g_string_printf (priv->sql, "SELECT %s FROM Events "
            "JOIN Services ON Events.service_id = Services.id "
            "JOIN EventTypes ON Events.event_type_id = EventTypes.id "
            "LEFT JOIN Remotes ON Events.remote_uid = Remotes.remote_uid "
                "AND Events.local_uid = Remotes.local_uid "
            "LEFT JOIN Headers ON Headers.event_id = Events.id AND "
                "Headers.name = 'message-token'", selection);

        if(priv->where_clause)
            g_string_append_printf(priv->sql, " WHERE %s", priv->where_clause);

        if(priv->group_by == RTCOM_EL_QUERY_GROUP_BY_CONTACT)
            g_string_append(priv->sql, " GROUP BY unique_remote");
        else if(priv->group_by == RTCOM_EL_QUERY_GROUP_BY_UIDS)
            g_string_append(priv->sql, " GROUP BY Remotes.local_uid, Remotes.remote_uid");
    }

    g_string_append_printf(
            priv->sql,
            " ORDER BY Events.id DESC LIMIT %d OFFSET %d;",
            priv->limit,
            priv->offset);

    return TRUE;
}

gboolean rtcom_el_query_prepare(
        RTComElQuery * query,
        ...)
{
    RTComElQueryPrivate * priv = NULL;
    va_list ap;
    const gchar * col = NULL;

    g_return_val_if_fail(query, FALSE);

    priv = RTCOM_EL_QUERY_GET_PRIV(query);
    g_assert(priv);

    g_free (priv->where_clause);
    priv->where_clause = NULL;

    va_start(ap, query);
    col = va_arg(ap, const gchar *);
    if(col)
    {
        GString *where_buf = g_string_new ("");

        while(col)
        {
            gpointer val = va_arg(ap, gpointer);
            /* XXX: use of enum in va_arg is not portable */
            RTComElOp op  = va_arg(ap, RTComElOp);
            if(!_build_where_clause (query, col, val, op, where_buf))
            {
                 va_end(ap);
                 g_string_free(where_buf, TRUE);
                 g_string_free(priv->sql, TRUE); priv->sql = NULL;
                 return FALSE;
            }

            col = va_arg(ap, const gchar *);
            if(col)
            {
                g_string_append(where_buf, " AND ");
            }
        }

        priv->where_clause = g_string_free (where_buf, FALSE);
    }
    va_end(ap);

    rtcom_el_query_refresh (query);
    return TRUE;
}

const gchar * rtcom_el_query_get_sql(
        RTComElQuery * query)
{
    RTComElQueryPrivate * priv = NULL;

    g_return_val_if_fail(RTCOM_IS_EL_QUERY(query), NULL);
    priv = RTCOM_EL_QUERY_GET_PRIV(query);

    if(!priv->sql)
        return NULL;

    return priv->sql->str;
}

const gchar * rtcom_el_query_get_where_clause(
        RTComElQuery * query)
{
    RTComElQueryPrivate * priv = NULL;

    g_return_val_if_fail(RTCOM_IS_EL_QUERY(query), NULL);
    priv = RTCOM_EL_QUERY_GET_PRIV(query);

    return priv->where_clause;
}

/* Some private functions */

static gboolean
_build_where_clause(
        RTComElQuery * query,
        const gchar * key,
        gpointer val,
        RTComElOp op,
        GString *ret)
{
    RTComElQueryPrivate * priv = NULL;
    GType expect;

    g_assert(RTCOM_IS_EL_QUERY(query));

    g_return_val_if_fail(key, FALSE);

    priv = RTCOM_EL_QUERY_GET_PRIV(query);
    g_return_val_if_fail(priv->el, FALSE);

    /* Note: we don't need special handling for Service.name and
     * EventType.name matching because we can use them directly in
     * queries. */

    expect = GPOINTER_TO_UINT (g_hash_table_lookup (priv->typing, key));

    switch (expect)
    {
        case G_TYPE_INT:
        case G_TYPE_BOOLEAN:
            {
                guint int_val = GPOINTER_TO_UINT(val);
                const gchar *op_string = _build_operator(op);
                if(!op_string)
                    return FALSE;

                g_string_append_printf(
                        ret, "%s %s %d",
                        (gchar *) g_hash_table_lookup( priv->mapping, key),
                        op_string, int_val);
                return TRUE;
            }

        case G_TYPE_STRING:
            if(op == RTCOM_EL_OP_IN_STRV)
            {
                gchar **strv_val = (gchar **) val;
                g_string_append_printf(
                        ret, "%s IN (",
                        (gchar *) g_hash_table_lookup(priv->mapping, key));
                for(; *strv_val; strv_val++)
                {
                    char *tmp = sqlite3_mprintf("%Q%c", *strv_val, *(strv_val + 1) ? ',': ' ');
                    g_string_append(ret, tmp);
                    sqlite3_free(tmp);
                }
                ret = g_string_append(ret, ")");
            }
            else if(op == RTCOM_EL_OP_STR_ENDS_WITH)
            {
                gchar *string_val = (gchar *) val;
                char *tmp = sqlite3_mprintf("%s LIKE '%%%q'",
                        (gchar *) g_hash_table_lookup(priv->mapping, key),
                        string_val);
                g_string_append(ret, tmp);
                sqlite3_free(tmp);
            }
            else
            {
                gchar *string_val = (gchar *) val;
                const gchar *op_string = _build_operator(op);
                char *tmp;
                if(!op_string)
                    return FALSE;

                tmp = sqlite3_mprintf("%s %s %Q",
                        (gchar *) g_hash_table_lookup(priv->mapping, key),
                        op_string, string_val);
                g_string_append (ret, tmp);
                sqlite3_free (tmp);
            }
            return TRUE;

        default:
            g_debug ("%s: unknown key %s", G_STRFUNC, key);
            g_return_val_if_reached (FALSE);
    }
}

static const gchar *
_build_operator (RTComElOp op)
{
    switch(op)
    {
        case RTCOM_EL_OP_EQUAL: return "=";
        case RTCOM_EL_OP_NOT_EQUAL: return "<>";
        case RTCOM_EL_OP_GREATER: return ">";
        case RTCOM_EL_OP_GREATER_EQUAL: return ">=";
        case RTCOM_EL_OP_LESS: return "<";
        case RTCOM_EL_OP_LESS_EQUAL: return "<=";
        case RTCOM_EL_OP_IN_STRV: g_return_val_if_reached(NULL);
        case RTCOM_EL_OP_STR_ENDS_WITH: g_return_val_if_reached(NULL);
        default: g_return_val_if_reached(NULL);
    }
}

/* vim: set ai et tw=75 ts=4 sw=4: */

