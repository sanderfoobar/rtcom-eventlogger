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

#include "rtcom-eventlogger/eventlogger-iter.h"
#include "rtcom-eventlogger/eventlogger.h"
#include "rtcom-eventlogger/eventlogger-plugin.h"
#include "rtcom-eventlogger/event.h"
#include "rtcom-eventlogger/db.h"

#include <glib.h>
#include <sqlite3.h>
#include <stdarg.h>
#include <string.h>
#include <sched.h>

#define RTCOM_EL_ITER_GET_PRIV(it) ((RTComElIterPrivate *) \
  rtcom_el_iter_get_instance_private(RTCOM_EL_ITER(it)))

typedef struct _RTComElIterPrivate RTComElIterPrivate;
struct _RTComElIterPrivate {
    RTComEl * el;
    RTComElQuery * query;

    rtcom_el_db_t db;
    rtcom_el_db_stmt_t stmt;

    GHashTable * plugins;

    /* Row data, as returned by rtocm_el_query_get_row() */
    GHashTable * columns;

    /* Whether this iterator is atomic and should close the
     * transaction when getting disposed. */
    gboolean atomic;

    /* Current values (valid only when iterator points at a result row). */
    gint current_event_id;
    gint current_service_id;
    gint current_event_type_id;
    RTComElPlugin *currently_active_plugin;
};

G_DEFINE_TYPE_WITH_PRIVATE(RTComElIter, rtcom_el_iter, G_TYPE_OBJECT);

enum
{
    RTCOM_EL_ITER_PROP_EL = 1,
    RTCOM_EL_ITER_PROP_QUERY,
    RTCOM_EL_ITER_PROP_DB,
    RTCOM_EL_ITER_PROP_STMT,
    RTCOM_EL_ITER_PROP_PLUGINS,
    RTCOM_EL_ITER_PROP_ATOMIC,
};

void _update_representation(
        RTComElIterPrivate * priv)
{
    g_assert(priv);

    g_return_if_fail(priv->stmt);

    if (priv->columns)
        rtcom_el_db_schema_update_row (priv->stmt, priv->columns);
    else
        priv->columns = rtcom_el_db_schema_get_row (priv->stmt);

#define LOOKUP_INT(x) (g_value_get_int (g_hash_table_lookup (priv->columns, x)))

    priv->current_event_id = LOOKUP_INT("id");
    priv->current_service_id = LOOKUP_INT("service-id");
    priv->current_event_type_id = LOOKUP_INT("event-type-id");

    priv->currently_active_plugin = g_hash_table_lookup (priv->plugins,
        GINT_TO_POINTER (priv->current_service_id));

#undef LOOKUP_INT
}

static gboolean _find_value(
        RTComElIter * it,
        const gchar * key,
        GValue * value)
{
    RTComElIterPrivate * priv;
    gboolean got_value = FALSE;

    g_return_val_if_fail(
             RTCOM_IS_EL_ITER(it) && key != NULL && value != NULL,
             FALSE);

    priv = RTCOM_EL_ITER_GET_PRIV(it);

    /* Ask the plugin */
    if(priv->currently_active_plugin && priv->currently_active_plugin->get_value)
        got_value = priv->currently_active_plugin->get_value(it, key, value);

    if(!got_value)
    {
        /* The plugin didn't know anything about this item, let's
         * try to figure if we can get something from the db
         */
        if(! rtcom_el_iter_get_raw(it, key, value))
        {
            g_warning("Error trying to fetch value from db.");
            return FALSE;
        }
    }

    return TRUE;
}

static void rtcom_el_iter_set_property(
        GObject * obj,
        guint prop_id,
        const GValue * value,
        GParamSpec * pspec)
{
    RTComElIterPrivate * priv = RTCOM_EL_ITER_GET_PRIV(obj);

    switch(prop_id)
    {
        case RTCOM_EL_ITER_PROP_EL:
            if(priv->el)
                g_object_unref(priv->el);
            priv->el = g_value_get_pointer(value);
            g_object_ref(priv->el);
            break;

        case RTCOM_EL_ITER_PROP_QUERY:
            if(priv->query)
                g_object_unref(priv->query);
            priv->query = g_value_get_pointer(value);
            g_object_ref(priv->query);
            break;

        case RTCOM_EL_ITER_PROP_DB:
            priv->db = g_value_get_pointer(value);
            break;

        case RTCOM_EL_ITER_PROP_STMT:
            priv->stmt = g_value_get_pointer(value);
            break;

        case RTCOM_EL_ITER_PROP_PLUGINS:
            priv->plugins = g_value_get_pointer(value);
            break;

        case RTCOM_EL_ITER_PROP_ATOMIC:
            priv->atomic = g_value_get_boolean(value);
            break;

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, prop_id, pspec);
            break;
    }
}

static void rtcom_el_iter_get_property(
        GObject * obj,
        guint prop_id,
        GValue * value,
        GParamSpec * pspec)
{
    RTComElIterPrivate * priv = RTCOM_EL_ITER_GET_PRIV(obj);

    switch(prop_id)
    {
        case RTCOM_EL_ITER_PROP_EL:
            g_value_set_pointer(value, priv->el);
            break;

        case RTCOM_EL_ITER_PROP_QUERY:
            g_value_set_pointer(value, priv->query);
            break;

        case RTCOM_EL_ITER_PROP_DB:
            g_value_set_pointer(value, priv->db);
            break;

        case RTCOM_EL_ITER_PROP_STMT:
            g_value_set_pointer(value, priv->stmt);
            break;

        case RTCOM_EL_ITER_PROP_PLUGINS:
            g_value_set_pointer(value, priv->plugins);
            break;

        case RTCOM_EL_ITER_PROP_ATOMIC:
            g_value_set_boolean(value, priv->atomic);
            break;

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, prop_id, pspec);
            break;
    }
}

static void rtcom_el_iter_init(
        RTComElIter * it)
{
    RTComElIterPrivate * priv = NULL;

    priv = RTCOM_EL_ITER_GET_PRIV(it);
    priv->el = NULL;
    priv->query = NULL;
    priv->db = NULL;
    priv->stmt = NULL;
    priv->plugins = NULL;
    priv->atomic = FALSE;

    priv->current_event_id = -1;
    priv->current_service_id = -1;
    priv->current_event_type_id = -1;
    priv->currently_active_plugin = NULL;
}

static GObject * rtcom_el_iter_constructor(
        GType type,
        guint n_construct_params,
        GObjectConstructParam * construct_params)
{
    GObject * object =
        G_OBJECT_CLASS(rtcom_el_iter_parent_class)->constructor(
                type,
                n_construct_params,
                construct_params);

    RTComElIterPrivate * priv = RTCOM_EL_ITER_GET_PRIV(object);
    _update_representation(priv);

    return object;
}

static void rtcom_el_iter_finalize(
        GObject * object)
{
    RTComElIterPrivate * priv = RTCOM_EL_ITER_GET_PRIV(object);

    if (priv->atomic)
      rtcom_el_db_commit (priv->db, NULL);

    g_object_unref(priv->el);
    g_object_unref(priv->query);

    if(priv->stmt)
    {
        sqlite3_finalize(priv->stmt);
        priv->stmt = NULL;
    }

    if(priv->columns)
    {
        g_hash_table_destroy (priv->columns);
        priv->columns = NULL;
    }

    G_OBJECT_CLASS(rtcom_el_iter_parent_class)->finalize(object);
}

static void rtcom_el_iter_class_init(
        RTComElIterClass * klass)
{
    GObjectClass * object_class = G_OBJECT_CLASS(klass);

    object_class->finalize = rtcom_el_iter_finalize;
    object_class->set_property = rtcom_el_iter_set_property;
    object_class->get_property = rtcom_el_iter_get_property;
    object_class->constructor = rtcom_el_iter_constructor;

    g_object_class_install_property(
            object_class,
            RTCOM_EL_ITER_PROP_EL,
            g_param_spec_pointer(
                "el",
                "RTComEl object",
                "The RTComEl object",
                G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

    g_object_class_install_property(
            object_class,
            RTCOM_EL_ITER_PROP_QUERY,
            g_param_spec_pointer(
                "query",
                "RTComElQuery object",
                "The RTComElQuery object used to create this iterator",
                G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

    g_object_class_install_property(
            object_class,
            RTCOM_EL_ITER_PROP_DB,
            g_param_spec_pointer(
                "sqlite3-database",
                "SQLite3 Database",
                "The SQLite3 Database",
                G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

    g_object_class_install_property(
            object_class,
            RTCOM_EL_ITER_PROP_STMT,
            g_param_spec_pointer(
                "sqlite3-statement",
                "SQLite3 Statement",
                "The SQLite3 Statement for this iterator",
                G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

    g_object_class_install_property(
            object_class,
            RTCOM_EL_ITER_PROP_PLUGINS,
            g_param_spec_pointer(
                "plugins-table",
                "GHashTable* of (guint, RTComElPlugin*)",
                "A GHashTable containing pairs of (plugin-id, plugin)",
                G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

    g_object_class_install_property(
            object_class,
            RTCOM_EL_ITER_PROP_ATOMIC,
            g_param_spec_boolean(
                "atomic",
                "Whether the iterator is atomic",
                "Whether the iterator has transactional brackets around it.",
                FALSE,
                G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));
}

gboolean rtcom_el_iter_first(
        RTComElIter * it)
{
    RTComElIterPrivate * priv = NULL;

    g_return_val_if_fail(it, FALSE);
    g_return_val_if_fail(RTCOM_IS_EL_ITER(it), FALSE);

    priv = RTCOM_EL_ITER_GET_PRIV(it);
    g_return_val_if_fail(priv->stmt, FALSE);

    sqlite3_reset(priv->stmt);

    return rtcom_el_iter_next(it);
}

gboolean rtcom_el_iter_next(
        RTComElIter * it)
{
    RTComElIterPrivate * priv = NULL;
    gint status;

    g_return_val_if_fail(it, FALSE);
    g_return_val_if_fail(RTCOM_IS_EL_ITER(it), FALSE);

    priv = RTCOM_EL_ITER_GET_PRIV(it);
    g_return_val_if_fail(priv->db, FALSE);
    g_return_val_if_fail(priv->stmt, FALSE);

    status = rtcom_el_db_iterate (priv->db, priv->stmt, NULL);
    if(status == SQLITE_DONE)
    {
        sqlite3_finalize(priv->stmt);
        priv->stmt = NULL;
        return FALSE;
    }
    if(status != SQLITE_ROW)
    {
        g_warning("Could not step statement: %s",
                sqlite3_errmsg(priv->db));
        sqlite3_finalize(priv->stmt);
        return FALSE;
    }

    _update_representation(priv);

    return TRUE;
}

GValueArray * rtcom_el_iter_get_valuearray(
        RTComElIter * it,
        ...)
{
    RTComElIterPrivate * priv = NULL;
    va_list ap;
    gchar * item = NULL;
    GValueArray * ret = NULL;
    GValue value = { 0 };

    g_return_val_if_fail(RTCOM_IS_EL_ITER(it), NULL);

    priv = RTCOM_EL_ITER_GET_PRIV(it);
    g_return_val_if_fail(priv->db, NULL);
    g_return_val_if_fail(priv->columns, NULL);

    g_warning ("%s: deprecated, use rtcom_el_iter_get_value_map() "
        "or rtcom_el_iter_get_values() instead", G_STRFUNC);

    va_start(ap, it);
    if(!(item = va_arg(ap, gchar *)))
    {
        va_end(ap);
        return NULL;
    }

    ret = g_value_array_new(1);
    while(item)
    {
        _find_value(it, item, &value);
        ret = g_value_array_append(ret, &value);
        g_value_unset(&value);
        item = va_arg(ap, gchar *);
    }
    va_end(ap);

    return ret;
}

GHashTable *
rtcom_el_iter_get_value_map (RTComElIter * it, ...)
{
    RTComElIterPrivate * priv = NULL;
    va_list ap;
    gchar * item = NULL;
    GHashTable *map = NULL;

    g_return_val_if_fail(RTCOM_IS_EL_ITER(it), NULL);

    priv = RTCOM_EL_ITER_GET_PRIV(it);
    g_return_val_if_fail(priv->db, NULL);
    g_return_val_if_fail(priv->columns, NULL);

    va_start(ap, it);
    if(!(item = va_arg(ap, gchar *)))
    {
        va_end(ap);
        return NULL;
    }

    map = g_hash_table_new_full (g_str_hash, g_str_equal, g_free,
      rtcom_el_db_g_value_slice_free);

    while(item)
    {
        GValue *val = g_slice_new0 (GValue);

        _find_value(it, item, val);
        g_hash_table_insert (map, g_strdup (item), val);

        item = va_arg(ap, gchar *);
    }
    va_end(ap);

    return map;
}

gboolean
rtcom_el_iter_get_values (RTComElIter * it, ...)
{
    RTComElIterPrivate * priv = NULL;
    va_list ap;
    gchar * item = NULL;

    g_return_val_if_fail(RTCOM_IS_EL_ITER(it), FALSE);

    priv = RTCOM_EL_ITER_GET_PRIV(it);
    g_return_val_if_fail(priv->db, FALSE);
    g_return_val_if_fail(priv->columns, FALSE);

    va_start(ap, it);
    if(!(item = va_arg(ap, gchar *)))
      {
        va_end(ap);
        return FALSE;
      }

    while(item)
      {
        GValue *val = g_slice_new0 (GValue);

        if (!_find_value(it, item, val))
          {
            va_end (ap);
            g_slice_free (GValue, val);
            return FALSE;
          }

        switch (G_VALUE_TYPE (val))
          {
            case G_TYPE_INT:
              {
                gint *x = va_arg (ap, gint *);

                *x = g_value_get_int (val);
                break;
              }

            case G_TYPE_BOOLEAN:
              {
                gboolean *x = va_arg (ap, gboolean *);

                *x = g_value_get_boolean (val);
                break;
              }

            case G_TYPE_STRING:
              {
                gchar **x = va_arg (ap, gchar **);

                *x = g_value_dup_string (val);
                break;
              }

            default:
              {
                g_error ("%s: bug in library, unexpected type %s", G_STRFUNC,
                    G_VALUE_TYPE_NAME (val));

                va_end (ap);

                g_value_unset(val);
                g_slice_free (GValue, val);

                return FALSE;
              }
          }

        g_value_unset (val);
        g_slice_free (GValue, val);

        item = va_arg(ap, gchar *);
      }

    va_end(ap);

    return TRUE;
}

RTComElAttachIter * rtcom_el_iter_get_attachments(
        RTComElIter * it)
{
    RTComElIterPrivate * priv = NULL;
    rtcom_el_db_stmt_t stmt = NULL;
    gint status;

    g_return_val_if_fail(RTCOM_IS_EL_ITER(it), NULL);

    priv = RTCOM_EL_ITER_GET_PRIV(it);
    g_return_val_if_fail(priv->db, NULL);
    g_return_val_if_fail(priv->stmt, NULL);

    /* Note: to avoid potential inconsistencies, the caller should call
     * rtcom_el_get_events_atomic() which wraps the whole query and all
     * subselects inside a transaction. */

    if (sqlite3_prepare_v2 (priv->db,
                            "SELECT id, event_id, path, desc"
                               " FROM Attachments WHERE event_id = ?", -1,
                            &stmt, NULL) != SQLITE_OK)
    {
        g_warning ("could not compile attachment select query: %s", sqlite3_errmsg(priv->db));
        goto ret_null;
    }

    if (sqlite3_bind_int (stmt, 1, priv->current_event_id) != SQLITE_OK)
    {
        g_warning ("could not bind the event ID parameter: %s", sqlite3_errmsg(priv->db));
        goto ret_null;
    }

    status = rtcom_el_db_iterate (priv->db, stmt, NULL);
    if (status == SQLITE_DONE)
    {
        g_debug ("no attachments found");
        goto ret_null;
    }

    if (status == SQLITE_ROW)
        return g_object_new(
            RTCOM_TYPE_EL_ATTACH_ITER,
            "sqlite3-database", priv->db,
            "sqlite3-statement", stmt,
            NULL);

ret_null:
    if (stmt != NULL)
    {
        sqlite3_finalize (stmt);
    }
    return NULL;
}

gboolean rtcom_el_iter_get_int(
        RTComElIter * it,
        const gchar * key,
        gint * ret)
{
    g_warning ("%s: deprecated, use rtcom_el_iter_get_values() instead",
        G_STRFUNC);
    return rtcom_el_iter_get_values (it, key, ret, NULL);
}

gboolean rtcom_el_iter_dup_string(
        RTComElIter * it,
        const gchar * key,
        gchar ** ret)
{
    g_warning ("%s: deprecated, use rtcom_el_iter_get_values() instead",
        G_STRFUNC);
    return rtcom_el_iter_get_values (it, key, ret, NULL);
}

/* Plugins functions */

gboolean rtcom_el_iter_get_raw(
        RTComElIter * it,
        const gchar * col,
        GValue * value)
{
    RTComElIterPrivate * priv = NULL;
    GValue *src;

    g_return_val_if_fail(RTCOM_IS_EL_ITER(it), FALSE);
    g_return_val_if_fail(value, FALSE);

    priv = RTCOM_EL_ITER_GET_PRIV(it);
    g_return_val_if_fail(priv->columns, FALSE);

    src = g_hash_table_lookup (priv->columns, col);
    if (!src)
      {
        g_debug ("%s: invalid column '%s'", G_STRFUNC, col);
        return FALSE;
      }

    g_value_init (value, G_VALUE_TYPE (src));
    g_value_copy (src, value);

    return TRUE;
}

const GHashTable *
rtcom_el_iter_get_columns (RTComElIter *it)
{
  RTComElIterPrivate * priv = NULL;

  g_return_val_if_fail(RTCOM_IS_EL_ITER(it), NULL);

  priv = RTCOM_EL_ITER_GET_PRIV(it);
  g_return_val_if_fail(priv->stmt, NULL);

  if (priv->columns == NULL)
    {
      g_debug ("%s: No results received, returning nothing", G_STRFUNC);
      return NULL;
    }

  return priv->columns;
}

gboolean
rtcom_el_iter_get (RTComElIter *it, RTComElEvent *ev)
{
  g_warning ("%s: deprecated, please use rtcom_el_iter_get_columns() "
      "or rtcom_el_iter_get_full() instead", G_STRFUNC);

  return rtcom_el_iter_get_full (it, ev);
}

gboolean rtcom_el_iter_get_full (
        RTComElIter * it,
        RTComElEvent * ev)
{
    RTComElIterPrivate * priv = NULL;

    g_return_val_if_fail(RTCOM_IS_EL_ITER(it), FALSE);

    priv = RTCOM_EL_ITER_GET_PRIV(it);
    g_return_val_if_fail(priv->stmt, FALSE);

    if (priv->columns == NULL)
      {
        g_debug ("%s: No results received, returning nothing", G_STRFUNC);
        return FALSE;
      }

    /* FIXME: this should be improved */
    RTCOM_EL_EVENT_SET_FIELD(ev, id,               g_value_get_int(g_hash_table_lookup(priv->columns, "id")));
    RTCOM_EL_EVENT_SET_FIELD(ev, service_id,       g_value_get_int(g_hash_table_lookup(priv->columns, "service-id")));
    RTCOM_EL_EVENT_SET_FIELD(ev, event_type_id,    g_value_get_int(g_hash_table_lookup(priv->columns, "event-type-id")));
    RTCOM_EL_EVENT_SET_FIELD(ev, service,          (gchar *) g_value_dup_string(g_hash_table_lookup(priv->columns, "service")));
    RTCOM_EL_EVENT_SET_FIELD(ev, event_type,       (gchar *) g_value_dup_string(g_hash_table_lookup(priv->columns, "event-type")));
    RTCOM_EL_EVENT_SET_FIELD(ev, storage_time,     g_value_get_int(g_hash_table_lookup(priv->columns, "storage-time")));
    RTCOM_EL_EVENT_SET_FIELD(ev, start_time,       g_value_get_int(g_hash_table_lookup(priv->columns, "start-time")));
    RTCOM_EL_EVENT_SET_FIELD(ev, end_time,         g_value_get_int(g_hash_table_lookup(priv->columns, "end-time")));
    RTCOM_EL_EVENT_SET_FIELD(ev, is_read,          g_value_get_boolean(g_hash_table_lookup(priv->columns, "is-read")));
    RTCOM_EL_EVENT_SET_FIELD(ev, outgoing,         g_value_get_boolean(g_hash_table_lookup(priv->columns, "outgoing")));
    RTCOM_EL_EVENT_SET_FIELD(ev, flags,            g_value_get_int(g_hash_table_lookup(priv->columns, "flags")));
    RTCOM_EL_EVENT_SET_FIELD(ev, bytes_sent,       g_value_get_int(g_hash_table_lookup(priv->columns, "bytes-sent")));
    RTCOM_EL_EVENT_SET_FIELD(ev, bytes_received,   g_value_get_int(g_hash_table_lookup(priv->columns, "bytes-received")));
    RTCOM_EL_EVENT_SET_FIELD(ev, remote_ebook_uid, (gchar *) g_value_dup_string(g_hash_table_lookup(priv->columns, "remote-ebook-uid")));
    RTCOM_EL_EVENT_SET_FIELD(ev, local_uid,        (gchar *) g_value_dup_string(g_hash_table_lookup(priv->columns, "local-uid")));
    RTCOM_EL_EVENT_SET_FIELD(ev, local_name,       (gchar *) g_value_dup_string(g_hash_table_lookup(priv->columns, "local-name")));
    RTCOM_EL_EVENT_SET_FIELD(ev, remote_uid,       (gchar *) g_value_dup_string(g_hash_table_lookup(priv->columns, "remote-uid")));
    RTCOM_EL_EVENT_SET_FIELD(ev, remote_name,      (gchar *) g_value_dup_string(g_hash_table_lookup(priv->columns, "remote-name")));
    RTCOM_EL_EVENT_SET_FIELD(ev, channel,          (gchar *) g_value_dup_string(g_hash_table_lookup(priv->columns, "channel")));
    RTCOM_EL_EVENT_SET_FIELD(ev, free_text,        (gchar *) g_value_dup_string(g_hash_table_lookup(priv->columns, "free-text")));
    RTCOM_EL_EVENT_SET_FIELD(ev, group_uid,        (gchar *) g_value_dup_string(g_hash_table_lookup(priv->columns, "group-uid")));

    /* This is not actually present among the columns, so fill in an empty
     * value; plugins will get a chance to alter it afterwards */
    RTCOM_EL_EVENT_SET_FIELD(ev, additional_text, NULL);

    /* These are initialized from the service column, then plugins get a
     * chance to alter them (FIXME: I can see that it might make sense
     * to look for the service name as an icon in a theme, but does it really
     * make sense for the pango markup?) */
    RTCOM_EL_EVENT_SET_FIELD(ev, icon_name,        (gchar *) g_value_dup_string(g_hash_table_lookup(priv->columns, "service")));
    RTCOM_EL_EVENT_SET_FIELD(ev, pango_markup,     (gchar *) g_value_dup_string(g_hash_table_lookup(priv->columns, "service")));

    /* FIXME: this will look better when we ditch the Event struct fields */
    if(priv->currently_active_plugin && priv->currently_active_plugin->get_value)
    {
        GValue tmp = {0,};
        gboolean ret =
            priv->currently_active_plugin->get_value(it, "additional-text", &tmp);
        if (ret)
        {
            g_free (RTCOM_EL_EVENT_GET_FIELD (ev, additional_text));
            RTCOM_EL_EVENT_SET_FIELD(ev, additional_text,
                (gchar *) g_value_dup_string(&tmp));
            g_value_unset(&tmp);
        }

        ret = priv->currently_active_plugin->get_value(it, "icon-name", &tmp);
        if (ret)
        {
            g_free (RTCOM_EL_EVENT_GET_FIELD (ev, icon_name));
            RTCOM_EL_EVENT_SET_FIELD(ev, icon_name,
                (gchar *) g_value_dup_string(&tmp));
            g_value_unset(&tmp);
        }

        ret = priv->currently_active_plugin->get_value(it, "pango-markup", &tmp);
        if (ret)
        {
            g_free (RTCOM_EL_EVENT_GET_FIELD (ev, pango_markup));
            RTCOM_EL_EVENT_SET_FIELD(ev, pango_markup,
                (gchar *) g_value_dup_string(&tmp));
            g_value_unset(&tmp);
        }
    }

    return TRUE;
}

gchar * rtcom_el_iter_get_header_raw(
        RTComElIter * it,
        const gchar * key)
{
    RTComElIterPrivate * priv = NULL;
    gchar * sql = NULL;
    sqlite3_stmt * stmt = NULL;
    gchar * ret = NULL;

    g_return_val_if_fail(RTCOM_IS_EL_ITER(it), NULL);
    g_return_val_if_fail(key, NULL);

    priv = RTCOM_EL_ITER_GET_PRIV(it);
    g_return_val_if_fail(priv->db, NULL);

    sql = sqlite3_mprintf(
            "SELECT value from Headers where event_id = %d and name = %Q;",
            priv->current_event_id,
            key);
    if(sqlite3_prepare(priv->db, sql, -1, &stmt, NULL) != SQLITE_OK)
    {
        g_warning("Could not compile: '%s': %s",
                sql,
                sqlite3_errmsg(priv->db));
        if(stmt)
        {
            sqlite3_finalize(stmt);
            stmt = NULL;
        }
    }
    else
    {
        while(sqlite3_step(stmt) == SQLITE_ROW)
        {
            ret = g_strdup((gchar *)sqlite3_column_text(stmt, 0));
            break;
        }
        sqlite3_finalize(stmt);
        stmt = NULL;
    }
    sqlite3_free(sql);

    return ret;
}

/* vim: set ai et tw=75 ts=4 sw=4: */

