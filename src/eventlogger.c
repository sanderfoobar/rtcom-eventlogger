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

#include <sqlite3.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include <sched.h>
#include <unistd.h>

#include <gmodule.h>
#include <dbus/dbus.h>
#include <glib/gstdio.h>

#include "rtcom-eventlogger/eventlogger.h"
#include "rtcom-eventlogger/eventlogger-types.h"
#include "rtcom-eventlogger/eventlogger-plugin.h"
#include "rtcom-eventlogger/eventlogger-attach-iter.h"
#include "rtcom-eventlogger/db.h"
#include "eventlogger-marshalers.h"
#include "config.h"

/* Configuration directory where EventLogger database and config is stored */
#define CONFIG_DIR      ".rtcom-eventlogger"
/* SQLite database where log is stored */
#define SQLITE_DATABASE        "el-v1.db"
#define OLD_SQLITE_DATABASE    "el.db"
#define ATTACH_DIR             "attachments"

#define DBUS_PATH              "/rtcomeventlogger/signal"
#define DBUS_INTERFACE         "rtcomeventlogger.signal"
#define DBUS_MATCH             "type='signal'," \
            "interface='rtcomeventlogger.signal'"


/* We'll be using 1s granularity, so value of 2 here actually means
 * "somewhere between 1 and 3". */
#define MAX_SQLITE_BUSY_LOOP_TIME 2

#define RTCOM_EL_GET_PRIV(el) (G_TYPE_INSTANCE_GET_PRIVATE ((el), \
            RTCOM_TYPE_EL, RTComElPrivate))
G_DEFINE_TYPE(RTComEl, rtcom_el, G_TYPE_OBJECT);

enum
{
    RTCOM_EL_PROP_DB = 1, /* Can be used by plugins convenience APIs */
    LAST_PROPERTY
};

enum
{
    NEW_EVENT = 0,
    EVENT_UPDATED,
    EVENT_DELETED,
    ALL_DELETED,
    REFRESH_HINT,
    LAST_SIGNAL
};

static guint signals[LAST_SIGNAL] = {0};

typedef struct _RTComElPrivate RTComElPrivate;
struct _RTComElPrivate {
    sqlite3 * db;

    /* GHashTable of (guint, RTComElPlugin*) */
    GHashTable * plugins;

    GHashTable * services;
    GHashTable * event_types;
    GHashTable * flags;

    DBusConnection   * dbus;

    gchar * last_group_uid;
};

/**************************************/
/* Private functions prototypes begin */
/**************************************/

static const gchar *el_get_home_dir (void);

static void _build_db_representation(
        sqlite3 ** db,
        GHashTable ** services,
        GHashTable ** event_types,
        GHashTable ** flags);

static void _free_db_representation(
        GHashTable ** services,
        GHashTable ** event_types,
        GHashTable ** flags);

static void _load_plugins(
        RTComElPrivate * priv);

static gboolean _scan_plugins_dir(
        const gchar * dir,
        RTComElPrivate * priv);

static gboolean _load_plugin(
        const gchar * filename,
        RTComElPrivate * priv);

static gint _init_plugin(
        RTComElPlugin * plugin,
        RTComElPrivate * priv);

static void _unload_plugins(
        GHashTable ** plugins);

static void _unload_plugin(
        gpointer key,
        gpointer value,
        gpointer user_data);

static gchar * _build_unique_dirname(
        const gchar * parent);

static void _emit_dbus(
        RTComEl * el,
        const gchar * signal,
        gint event_id,
        const gchar * service);

static DBusHandlerResult _dbus_filter_callback(
        DBusConnection * con,
        DBusMessage * msg,
        void * user_data);

static void _get_events_dbus_data(
        RTComEl * el,
        gint event_id,
        gchar ** local_uid,
        gchar ** remote_uid,
        gchar ** remote_ebook_uid,
        gchar ** group_uid);

/**************************************/
/* Private functions prototypes end   */
/**************************************/

GQuark
rtcom_el_error_quark ()
{
    static GQuark error_domain = 0;

    if (G_UNLIKELY (!error_domain))
        error_domain = g_quark_from_string("RTCOM_EL_ERROR");

    return error_domain;
}

static void rtcom_el_get_property(
        GObject * obj,
        guint prop_id,
        GValue * value,
        GParamSpec * pspec)
{
    RTComElPrivate * priv = RTCOM_EL_GET_PRIV(obj);

    switch(prop_id)
    {
        case RTCOM_EL_PROP_DB:
            g_value_set_pointer(value, priv->db);
            break;
        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, prop_id, pspec);
    }
}

/* Take care of initialising the database, potentially trying it
 * later (when needed) if it couldn't be done while creating
 * the eventlogger. This is to try and recover from the disk
 * full conditions.
 */
static gboolean _ensure_db(RTComEl *el, gboolean reopen)
{
    gchar *fn, *old_fn;

    RTComElPrivate * priv = RTCOM_EL_GET_PRIV(el);

    g_assert (priv != NULL);

    if (priv->db != NULL)
        return TRUE;

    g_debug ("%s: called", G_STRFUNC);

    fn = g_build_filename (el_get_home_dir (), CONFIG_DIR, NULL);
    if (!g_file_test (fn, G_FILE_TEST_IS_DIR))
    {
        if (g_mkdir (fn, S_IRWXU) != 0)
        {
            g_warning ("%s: can't create directory '%s': %s", G_STRFUNC,
                fn, g_strerror (errno));
            g_free (fn);
            return FALSE;
        }
    }
    g_free (fn);

    fn = g_build_filename (el_get_home_dir (), CONFIG_DIR, SQLITE_DATABASE,
        NULL);

    old_fn = g_build_filename (el_get_home_dir (), CONFIG_DIR,
        OLD_SQLITE_DATABASE, NULL);

    /* If needed, this will convert the v0 database format into the
     * one we use today. If this fails for any reason, we shouldn't
     * just create a new empty db, instead we should wait for some
     * other process (or perhaps us later) to do the upgrade. */
    if (!rtcom_el_db_convert_from_db0 (fn, old_fn))
    {
        g_free (old_fn);
        g_free (fn);
        return FALSE;
    }

    g_free (old_fn);

    /* Now we can open the database. */
    priv->db = rtcom_el_db_open (fn);
    g_free (fn);

    if (priv->db == NULL)
        return FALSE;

    _build_db_representation(
            &(priv->db),
            &(priv->services),
            &(priv->event_types),
            &(priv->flags));

    _load_plugins(priv);

    if (reopen)
    {
      _emit_dbus(el, "DbReopen", -1, NULL);
    }

    return TRUE;
}

static void rtcom_el_init(
        RTComEl * el)
{
    RTComElPrivate * priv = NULL;
    DBusError err;
    dbus_bool_t filter_added;

    g_debug("%s: called", G_STRFUNC);

    priv = RTCOM_EL_GET_PRIV(el);
    priv->services = NULL;
    priv->event_types = NULL;
    priv->flags = NULL;
    priv->db = NULL;

    priv->last_group_uid = NULL;

    priv->plugins = g_hash_table_new(NULL, NULL);

    dbus_error_init(&err);
    priv->dbus = dbus_bus_get(DBUS_BUS_SESSION, &err);
    if(dbus_error_is_set(&err))
    {
        g_warning("Could not acquire dbus connection: %s", err.message);
        dbus_error_free(&err);
        return;
    }

    filter_added = dbus_connection_add_filter(
            priv->dbus,
            (DBusHandleMessageFunction) _dbus_filter_callback,
            (void *) el,
            NULL);
    if(!filter_added)
    {
        g_warning("Could not add dbus filter: %s", err.message);
        dbus_connection_unref(priv->dbus);
        priv->dbus = NULL;
        return;
    }

    dbus_bus_add_match(priv->dbus, DBUS_MATCH, NULL);

    _ensure_db(el, TRUE);
}

static void
rtcom_el_dispose (GObject *object)
{
    RTComElPrivate *priv = RTCOM_EL_GET_PRIV(object);

    g_debug ("%s: called", G_STRFUNC);

    if (priv->dbus != NULL)
    {
        dbus_bus_remove_match(priv->dbus, DBUS_MATCH, NULL);

        dbus_connection_remove_filter(
            priv->dbus,
            (DBusHandleMessageFunction) _dbus_filter_callback,
            (void *) object);

        dbus_connection_unref(priv->dbus);
        priv->dbus = NULL;
    }

    G_OBJECT_CLASS(rtcom_el_parent_class)->dispose(object);
}

static void rtcom_el_finalize(
        GObject * object)
{
    RTComElPrivate * priv = RTCOM_EL_GET_PRIV(object);

    g_debug("%s: called", G_STRFUNC);

    g_free(priv->last_group_uid);

    _unload_plugins(&(priv->plugins));

    _free_db_representation(
            &(priv->services),
            &(priv->event_types),
            &(priv->flags));

    rtcom_el_db_close (priv->db);
    priv->db = NULL;

    G_OBJECT_CLASS(rtcom_el_parent_class)->finalize(object);
}

static void rtcom_el_class_init(
        RTComElClass * klass)
{
    GObjectClass* object_class = G_OBJECT_CLASS (klass);
    g_type_class_add_private(object_class, sizeof(RTComElPrivate));
    object_class->finalize = rtcom_el_finalize;
    object_class->dispose = rtcom_el_dispose;
    object_class->get_property = rtcom_el_get_property;

    g_object_class_install_property(
            object_class,
            RTCOM_EL_PROP_DB,
            g_param_spec_pointer(
                "db",
                "sqlite db",
                "The sqlite3 db",
                G_PARAM_READABLE));

    signals[NEW_EVENT] = g_signal_new(
            "new-event",
            G_TYPE_FROM_CLASS(object_class),
            G_SIGNAL_RUN_FIRST,
            0,
            NULL,
            NULL,
            rtcom_el_cclosure_marshal_VOID__INT_STRING_STRING_STRING_STRING_STRING,
            G_TYPE_NONE,
            6,
            G_TYPE_INT,
            G_TYPE_STRING,
            G_TYPE_STRING,
            G_TYPE_STRING,
            G_TYPE_STRING,
            G_TYPE_STRING);

    signals[EVENT_UPDATED] = g_signal_new(
            "event-updated",
            G_TYPE_FROM_CLASS(object_class),
            G_SIGNAL_RUN_FIRST,
            0,
            NULL,
            NULL,
            rtcom_el_cclosure_marshal_VOID__INT_STRING_STRING_STRING_STRING_STRING,
            G_TYPE_NONE,
            6,
            G_TYPE_INT,
            G_TYPE_STRING,
            G_TYPE_STRING,
            G_TYPE_STRING,
            G_TYPE_STRING,
            G_TYPE_STRING);

    signals[EVENT_DELETED] = g_signal_new(
            "event-deleted",
            G_TYPE_FROM_CLASS(object_class),
            G_SIGNAL_RUN_FIRST,
            0,
            NULL,
            NULL,
            rtcom_el_cclosure_marshal_VOID__INT_STRING_STRING_STRING_STRING_STRING,
            G_TYPE_NONE,
            6,
            G_TYPE_INT,
            G_TYPE_STRING,
            G_TYPE_STRING,
            G_TYPE_STRING,
            G_TYPE_STRING,
            G_TYPE_STRING);

    signals[ALL_DELETED] = g_signal_new(
            "all-deleted",
            G_TYPE_FROM_CLASS(object_class),
            G_SIGNAL_RUN_FIRST,
            0,
            NULL,
            NULL,
            g_cclosure_marshal_VOID__STRING,
            G_TYPE_NONE,
            1,
            G_TYPE_STRING);

    signals[REFRESH_HINT] = g_signal_new(
            "refresh-hint",
            G_TYPE_FROM_CLASS(object_class),
            G_SIGNAL_RUN_FIRST,
            0,
            NULL,
            NULL,
            g_cclosure_marshal_VOID__VOID,
            G_TYPE_NONE,
            0);
}

/******************************************/
/* Public functions implementation begins */
/******************************************/
RTComEl * rtcom_el_new()
{
    return RTCOM_EL(g_object_new(RTCOM_TYPE_EL, NULL));
}

RTComEl * rtcom_el_get_shared ()
{
    static gpointer el = NULL;

    if (G_UNLIKELY (NULL == el))
    {
        el = rtcom_el_new ();
        g_object_add_weak_pointer (G_OBJECT (el), &el);
    }
    else
    {
        g_object_ref (G_OBJECT (el));
    }

    return el;
}

struct remotes_ctx {
    gboolean exists;
    gchar *abook_uid;
    gchar *remote_name;
};

void _fetch_remote_data (gpointer data, gpointer user_data)
{
  sqlite3_stmt *stmt = data;
  struct remotes_ctx *ctx = user_data;

  ctx->exists = TRUE;
  ctx->abook_uid = g_strdup((gchar *) sqlite3_column_text(stmt, 1));
  ctx->remote_name = g_strdup((gchar *) sqlite3_column_text(stmt, 2));
}

gboolean
rtcom_el_update_remote_contacts(RTComEl *el, GList *contacts, GError **error)
{
    RTComElPrivate *priv = RTCOM_EL_GET_PRIV (el);

    g_return_val_if_fail (contacts != NULL, TRUE);

    if (!_ensure_db (el, TRUE))
    {
        g_set_error(error, RTCOM_EL_ERROR, RTCOM_EL_INTERNAL_ERROR,
            "Can't update contact data, database isn't opened.");
        return FALSE;
    }

    if (!rtcom_el_db_transaction (priv->db, FALSE, error))
    {
        return FALSE;
    }

    while (contacts != NULL)
    {
        RTComElRemote *c = contacts->data;

        if (!rtcom_el_db_exec_printf (priv->db, NULL, NULL, error,
            "UPDATE Remotes SET abook_uid = %Q, remote_name = %Q "
              "WHERE remote_uid = %Q AND local_uid = %Q;",
            c->abook_uid, c->remote_name,
            c->remote_uid, c->local_uid))
        {
            rtcom_el_db_rollback (priv->db, NULL);
            return FALSE;
        }

        contacts = contacts->next;
    }

    if (!rtcom_el_db_commit (priv->db, error))
    {
        rtcom_el_db_rollback (priv->db, NULL);
        return FALSE;
    }

    _emit_dbus(el, "RefreshHint", -1, NULL);

    return TRUE;
}


gboolean
rtcom_el_update_remote_contact(RTComEl *el,
    const gchar *local_uid, const gchar *remote_uid,
    const gchar *new_abook_uid, const gchar *new_remote_name,
    GError **error)
{
    GList *li;
    RTComElRemote c;
    gboolean ret;

    c.local_uid = (gchar *) local_uid;
    c.remote_uid = (gchar *) remote_uid;
    c.abook_uid = (gchar *) new_abook_uid;
    c.remote_name = (gchar *) new_remote_name;

    li = g_list_prepend (NULL, &c);

    ret = rtcom_el_update_remote_contacts (el, li, error);

    g_list_free (li);

    return ret;
}

gboolean
rtcom_el_remove_abook_uids (RTComEl *el, GList *abook_uids, GError **error)
{
    RTComElPrivate *priv = RTCOM_EL_GET_PRIV (el);

    g_return_val_if_fail (abook_uids != NULL, TRUE);

    if (!_ensure_db (el, TRUE))
    {
        g_set_error(error, RTCOM_EL_ERROR, RTCOM_EL_INTERNAL_ERROR,
            "Can't update contact data, database isn't opened.");
        return FALSE;
    }

    if (!rtcom_el_db_transaction (priv->db, FALSE, error))
    {
        return FALSE;
    }

    while (abook_uids != NULL)
    {
        const gchar *uid = abook_uids->data;

        if (!rtcom_el_db_exec_printf (priv->db, NULL, NULL, error,
            "UPDATE Remotes SET abook_uid = NULL WHERE abook_uid = %Q;",
            uid))
        {
            rtcom_el_db_rollback (priv->db, NULL);
            return FALSE;
        }

        abook_uids = abook_uids->next;
    }

    if (!rtcom_el_db_commit (priv->db, error))
    {
        rtcom_el_db_rollback (priv->db, NULL);
        return FALSE;
    }

    _emit_dbus(el, "RefreshHint", -1, NULL);

    return TRUE;
}

gboolean
rtcom_el_remove_abook_uid (RTComEl *el, const gchar *abook_uid,
    GError **error)
{
    GList *li;
    gboolean ret;

    li = g_list_prepend (NULL, (gchar *) abook_uid);

    ret = rtcom_el_remove_abook_uids (el, li, error);

    g_list_free (li);

    return ret;
}


static gboolean
_add_event_precheck(
        RTComEl * el,
        RTComElEvent * ev,
        GError ** error,
        gint *service_id,
        gint *eventtype_id)
{
    RTComElPrivate * priv;

    if(!el || !RTCOM_IS_EL(el))
    {
        g_warning("Invalid RTComEl.");
        g_set_error(
                error,
                RTCOM_EL_ERROR,
                RTCOM_EL_INVALID_ARGUMENT_ERROR,
                "Invalid RTComEl.");
        return FALSE;
    }

    priv = RTCOM_EL_GET_PRIV(el);

    if (!_ensure_db (el, TRUE))
    {
        g_warning("Database can't be opened.");
        g_set_error(
                error,
                RTCOM_EL_ERROR,
                RTCOM_EL_INTERNAL_ERROR,
                "Database can't be opened.");
        return FALSE;
    }

    if(!ev)
    {
        g_set_error(
                error,
                RTCOM_EL_ERROR,
                RTCOM_EL_INVALID_ARGUMENT_ERROR,
                "'event' must be not NULL.");
        return FALSE;
    }

    /* Check if mandatory arguments are there */
    if(!RTCOM_EL_EVENT_IS_SET(ev, service))
    {
        g_warning("'service' must be not NULL.");
        g_set_error(
                error,
                RTCOM_EL_ERROR,
                RTCOM_EL_INVALID_ARGUMENT_ERROR,
                "'service' must be not NULL.");
        return FALSE;
    }
    if(!RTCOM_EL_EVENT_IS_SET(ev, event_type))
    {
        g_warning("'eventtype' must be not NULL.");
        g_set_error(
                error,
                RTCOM_EL_ERROR,
                RTCOM_EL_INVALID_ARGUMENT_ERROR,
                "'eventtype' must be not NULL.");
        return FALSE;
    }

    if(!RTCOM_EL_EVENT_IS_SET(ev, local_uid))
    {
        g_warning("local uid must be not NULL.");
        g_set_error(
                error,
                RTCOM_EL_ERROR,
                RTCOM_EL_INVALID_ARGUMENT_ERROR,
                "local uid must be not NULL.");
        return FALSE;
    }

    *service_id = rtcom_el_get_service_id(
            el,
            RTCOM_EL_EVENT_GET_FIELD(ev, service));
    if(*service_id == -1)
    {
        g_warning(G_STRLOC ": service not found.");
        g_set_error(
                error,
                RTCOM_EL_ERROR,
                RTCOM_EL_INVALID_ARGUMENT_ERROR,
                "Service not found.");
        return FALSE;
    }

    /* FIXME: these are temporary backwards-compatibility fixes
     * so that old messaging-ui can be used with us. To be removed
     * in a few releases. */
    {
      gchar *eventtype = RTCOM_EL_EVENT_GET_FIELD(ev, event_type);

      if (!g_strcmp0 (eventtype, "RTCOM_EL_EVENTTYPE_CHAT_INBOUND"))
        {
          g_warning ("%s: Event type %s is deprecated!", G_STRFUNC, eventtype);
          RTCOM_EL_EVENT_SET_FIELD(ev, event_type,
              "RTCOM_EL_EVENTTYPE_CHAT_MESSAGE");
        }
      else if (!g_strcmp0 (eventtype, "RTCOM_EL_EVENTTYPE_CHAT_OUTBOUND"))
        {
          g_warning ("%s: Event type %s is deprecated!", G_STRFUNC, eventtype);
          RTCOM_EL_EVENT_SET_FIELD(ev, event_type,
              "RTCOM_EL_EVENTTYPE_CHAT_MESSAGE");
          RTCOM_EL_EVENT_SET_FIELD(ev, outgoing, TRUE);
        }
      else if (!g_strcmp0 (eventtype, "RTCOM_EL_EVENTTYPE_SMS_INBOUND"))
        {
          g_warning ("%s: Event type %s is deprecated!", G_STRFUNC, eventtype);
          RTCOM_EL_EVENT_SET_FIELD(ev, event_type,
              "RTCOM_EL_EVENTTYPE_SMS_MESSAGE");
        }
      else if (!g_strcmp0 (eventtype, "RTCOM_EL_EVENTTYPE_SMS_OUTBOUND"))
        {
          g_warning ("%s: Event type %s is deprecated!", G_STRFUNC, eventtype);
          RTCOM_EL_EVENT_SET_FIELD(ev, event_type,
              "RTCOM_EL_EVENTTYPE_SMS_MESSAGE");
          RTCOM_EL_EVENT_SET_FIELD(ev, outgoing, TRUE);
        }
    }

    *eventtype_id = rtcom_el_get_eventtype_id(el,
        RTCOM_EL_EVENT_GET_FIELD(ev, event_type));

    if(*eventtype_id == -1)
    {
        g_warning("EventType not found.");
        g_set_error(
                error,
                RTCOM_EL_ERROR,
                RTCOM_EL_INVALID_ARGUMENT_ERROR,
                "EventType not found.");
        return FALSE;
    }

    if(G_UNLIKELY(!priv->db))
    {
        g_warning("Database not initialized.");
        g_set_error(
                error,
                RTCOM_EL_ERROR,
                RTCOM_EL_INTERNAL_ERROR,
                "Database not initialized.");
        return FALSE;
    }

    return TRUE;
}

static gint
_add_event_core(
        RTComEl * el,
        RTComElEvent * ev,
        gint service_id,
        gint eventtype_id,
        GError ** error)
{
    RTComElPrivate * priv = RTCOM_EL_GET_PRIV(el);
    gint event_id = -1;
    gint remote_uid_exists = 0;
    gchar *existing_abook_uid = NULL;
    gchar *existing_remote_name = NULL;

    /* Note: if group_uid field is not set, it's copied
     * from the previous event. */
    if(RTCOM_EL_EVENT_IS_SET(ev, group_uid))
    {
        g_free(priv->last_group_uid);
        priv->last_group_uid = g_strdup(
                RTCOM_EL_EVENT_GET_FIELD(ev, group_uid));
    }

    if (RTCOM_EL_EVENT_IS_SET(ev, remote_uid))
      {
        struct remotes_ctx ctx = { FALSE, NULL, NULL };

        if (!rtcom_el_db_exec_printf (priv->db, _fetch_remote_data, &ctx,
              NULL, "SELECT abook_uid, remote_name FROM Remotes WHERE "
              "remote_uid = %Q AND local_uid = %Q;",
            RTCOM_EL_EVENT_GET_FIELD(ev, remote_uid),
              RTCOM_EL_EVENT_GET_FIELD(ev, local_uid)))
          goto db_error;

        if (ctx.exists)
        {
          remote_uid_exists = TRUE;
          existing_abook_uid = ctx.abook_uid;
          existing_remote_name = ctx.remote_name;
        }
      }

    if (!rtcom_el_db_exec_printf (priv->db, NULL, NULL, NULL,
        "INSERT INTO Events ("
        "service_id, event_type_id, "
        "storage_time, start_time, end_time, is_read, outgoing, "
        "flags, bytes_sent, bytes_received, "
        "local_uid, local_name, remote_uid, "
        "channel, free_text, group_uid) VALUES ( "
        "%d, %d, %d, %d, %d, %d, %d, %d, %d, %d, "
        "%Q, %Q, %Q, %Q, %Q, %Q);",
        service_id,
        eventtype_id,
        time(NULL),
        RTCOM_EL_EVENT_IS_SET(ev, start_time) ? RTCOM_EL_EVENT_GET_FIELD(ev, start_time) : 0,
        RTCOM_EL_EVENT_IS_SET(ev, end_time) ? RTCOM_EL_EVENT_GET_FIELD(ev, end_time): 0,
        RTCOM_EL_EVENT_IS_SET(ev, is_read) ? RTCOM_EL_EVENT_GET_FIELD(ev, is_read): 0,
        RTCOM_EL_EVENT_IS_SET(ev, outgoing) ? RTCOM_EL_EVENT_GET_FIELD(ev, outgoing): 0,
        RTCOM_EL_EVENT_IS_SET(ev, flags) ? RTCOM_EL_EVENT_GET_FIELD(ev, flags) : 0,
        RTCOM_EL_EVENT_IS_SET(ev, bytes_sent) ? RTCOM_EL_EVENT_GET_FIELD(ev, bytes_sent) : 0,
        RTCOM_EL_EVENT_IS_SET(ev, bytes_received) ? RTCOM_EL_EVENT_GET_FIELD(ev, bytes_received) : 0,
        RTCOM_EL_EVENT_IS_SET(ev, local_uid) ? RTCOM_EL_EVENT_GET_FIELD(ev, local_uid) : NULL,
        RTCOM_EL_EVENT_IS_SET(ev, local_name) ? RTCOM_EL_EVENT_GET_FIELD(ev, local_name) : NULL,
        RTCOM_EL_EVENT_IS_SET(ev, remote_uid) ? RTCOM_EL_EVENT_GET_FIELD(ev, remote_uid) : NULL,
        RTCOM_EL_EVENT_IS_SET(ev, channel) ? RTCOM_EL_EVENT_GET_FIELD(ev, channel) : NULL,
        RTCOM_EL_EVENT_IS_SET(ev, free_text) ? RTCOM_EL_EVENT_GET_FIELD(ev, free_text) : NULL,
        RTCOM_EL_EVENT_IS_SET(ev, group_uid) ? RTCOM_EL_EVENT_GET_FIELD(ev, group_uid) : priv->last_group_uid))
    {
        goto db_error;
    }

    event_id = sqlite3_last_insert_rowid(priv->db);

    if (RTCOM_EL_EVENT_IS_SET(ev, remote_uid))
      {
        /* If there's no entry for this remote_uid yet, create it. */
        if (!remote_uid_exists)
          {
            if (!rtcom_el_db_exec_printf (priv->db, NULL, NULL, NULL,
                "INSERT INTO Remotes (local_uid, remote_uid, remote_name, abook_uid) "
                "VALUES (%Q, %Q, %Q, %Q);",
                RTCOM_EL_EVENT_GET_FIELD(ev, local_uid),
                RTCOM_EL_EVENT_GET_FIELD(ev, remote_uid),
                RTCOM_EL_EVENT_IS_SET(ev, remote_name) ?
                    RTCOM_EL_EVENT_GET_FIELD(ev, remote_name) : NULL,
                RTCOM_EL_EVENT_IS_SET(ev, remote_ebook_uid) ?
                    RTCOM_EL_EVENT_GET_FIELD(ev, remote_ebook_uid) : NULL))
              goto db_error;
          }
        /* Otherwise, update existing entry with new data. This should
         * happen very infrequently (contact added/removed from abook,
         * or changes their name), so we're not worried about 2
         * UPDATE statements. */
        else
          {
            gchar *local_uid = RTCOM_EL_EVENT_GET_FIELD(ev, local_uid);
            gchar *new_abook_uid =
                RTCOM_EL_EVENT_IS_SET(ev, remote_ebook_uid) ?
                    RTCOM_EL_EVENT_GET_FIELD(ev, remote_ebook_uid) : NULL;
            gchar *new_remote_name =
                RTCOM_EL_EVENT_IS_SET(ev, remote_name) ?
                    RTCOM_EL_EVENT_GET_FIELD(ev, remote_name) : NULL;

            if (g_strcmp0(new_abook_uid, existing_abook_uid))
                if (!rtcom_el_db_exec_printf (priv->db, NULL, NULL, NULL,
                    "UPDATE Remotes SET abook_uid = %Q WHERE "
                        "remote_uid = %Q AND local_uid = %Q;",
                    new_abook_uid, RTCOM_EL_EVENT_GET_FIELD(ev,
                        remote_uid), local_uid))
                          goto db_error;

            if (g_strcmp0(new_remote_name, existing_remote_name))
                if (!rtcom_el_db_exec_printf (priv->db, NULL, NULL, NULL,
                    "UPDATE Remotes SET remote_name = %Q WHERE "
                        "remote_uid = %Q AND local_uid = %Q;",
                    new_remote_name, RTCOM_EL_EVENT_GET_FIELD(ev,
                        remote_uid), local_uid))
                        goto db_error;
          }
      }

    g_free (existing_abook_uid);
    g_free (existing_remote_name);
    return event_id;

db_error:
    if ((sqlite3_errcode (priv->db) == SQLITE_FULL) ||
      (sqlite3_errcode (priv->db) == SQLITE_IOERR))
        g_set_error(error, RTCOM_EL_ERROR, RTCOM_EL_DATABASE_FULL,
            "Can't insert event, database is full.");
    else if (sqlite3_errcode (priv->db) == SQLITE_BUSY)
        g_set_error(error, RTCOM_EL_ERROR, RTCOM_EL_TEMPORARY_ERROR,
            "Can't insert event, database is locked.");
    else
        g_set_error(error, RTCOM_EL_ERROR, RTCOM_EL_INTERNAL_ERROR,
            "Database error while inserting event.");

    g_free (existing_abook_uid);
    g_free (existing_remote_name);
    return -1;
}


gint rtcom_el_add_event(
        RTComEl * el,
        RTComElEvent * ev,
        GError ** error)
{
    RTComElPrivate * priv = RTCOM_EL_GET_PRIV(el);
    gint event_id, service_id, eventtype_id;

    if (!_add_event_precheck (el, ev, error, &service_id, &eventtype_id))
        return -1;

    if (!rtcom_el_db_transaction (priv->db, TRUE, error))
    {
        return -1;
    }

    event_id = _add_event_core (el, ev, service_id, eventtype_id, error);
    if (event_id == -1)
    {
        rtcom_el_db_rollback (priv->db, NULL);
        return -1;
    }

    if (!rtcom_el_db_commit (priv->db, error))
    {
        rtcom_el_db_rollback (priv->db, NULL);
        return -1;
    }

    /* Emit dbus signal */
    if(event_id > 0)
        _emit_dbus(el, "NewEvent", event_id, RTCOM_EL_EVENT_GET_FIELD(ev, service));

    return event_id;
}

gint rtcom_el_add_event_full(
        RTComEl * el,
        RTComElEvent * ev,
        GHashTable *headers,
        GList *attachments,
        GError ** error)
{
    GList *li;
    GHashTableIter iter;
    RTComElPrivate * priv = RTCOM_EL_GET_PRIV(el);
    gint event_id, service_id, eventtype_id;
    gpointer hk, hv;

    if (!_add_event_precheck (el, ev, error, &service_id, &eventtype_id))
        return -1;

    if (!rtcom_el_db_transaction (priv->db, TRUE, error))
    {
        return -1;
    }

    event_id = _add_event_core (el, ev, service_id, eventtype_id, error);
    if (event_id == -1)
    {
        rtcom_el_db_rollback (priv->db, NULL);
        return -1;
    }

    for (li = attachments; li != NULL; li = g_list_next (li))
    {
        RTComElAttachment *att = li->data;
        if (-1 == rtcom_el_add_attachment (el, event_id, att->path,
            att->desc, error))
        {
            rtcom_el_db_rollback (priv->db, NULL);
            return -1;
        }
    }

    g_hash_table_iter_init (&iter, headers);
    while (g_hash_table_iter_next (&iter, &hk, &hv))
    {
        if (-1 == rtcom_el_add_header (el, event_id, hk, hv, error))
        {
            rtcom_el_db_rollback (priv->db, NULL);
            return -1;
        }
    }

    if (!rtcom_el_db_commit (priv->db, error))
    {
        rtcom_el_db_rollback (priv->db, NULL);
        return -1;
    }

    /* Emit dbus signal */
    if(event_id > 0)
        _emit_dbus(el, "NewEvent", event_id, RTCOM_EL_EVENT_GET_FIELD(ev, service));

    return event_id;
}


const gchar * rtcom_el_get_last_group_uid(
        RTComEl * el)
{
    RTComElPrivate * priv;

    g_return_val_if_fail(RTCOM_IS_EL(el), NULL);

    priv = RTCOM_EL_GET_PRIV(el);

    return priv->last_group_uid;
}

gint rtcom_el_add_header(
        RTComEl * el,
        gint event_id,
        const gchar * header,
        const gchar * value,
        GError ** error)
{
    RTComElPrivate * priv = NULL;
    guint header_id;

    if(!RTCOM_IS_EL(el))
    {
        g_set_error(
                error,
                RTCOM_EL_ERROR,
                RTCOM_EL_INVALID_ARGUMENT_ERROR,
                "Invalid RTComEl.");
        return -1;
    }

    if(event_id < 1)
    {
        g_set_error(
                error,
                RTCOM_EL_ERROR,
                RTCOM_EL_INVALID_ARGUMENT_ERROR,
                "Invalid event_id.");
        return -1;
    }

    if(!header)
    {
        g_set_error(
                error,
                RTCOM_EL_ERROR,
                RTCOM_EL_INVALID_ARGUMENT_ERROR,
                "Invalid header.");
        return -1;
    }

    if(!value)
    {
        g_set_error(
                error,
                RTCOM_EL_ERROR,
                RTCOM_EL_INVALID_ARGUMENT_ERROR,
                "Invalid value.");
        return -1;
    }

    priv = RTCOM_EL_GET_PRIV(el);
    g_assert(priv);

    if (!_ensure_db (el, TRUE))
    {
        g_set_error(error, RTCOM_EL_ERROR, RTCOM_EL_INTERNAL_ERROR,
            "Database isn't opened.");
        return -1;
    }

    if (!rtcom_el_db_exec_printf (priv->db, NULL, NULL, error,
        "INSERT INTO Headers (event_id, name, value) VALUES (%d, %Q, %Q);",
        event_id, header, value))
    {
        return -1;
    }

    header_id = sqlite3_last_insert_rowid(priv->db);

    return header_id;
}

gint rtcom_el_add_attachment(
        RTComEl * el,
        gint event_id,
        const gchar * path,
        const gchar * desc,
        GError ** error)
{
    gint attachment_id = -1;
    RTComElPrivate * priv = NULL;
    gchar * dir = NULL;
    gchar * unique_dir = NULL;
    FILE * src = NULL;
    FILE * dest = NULL;
    gchar * dest_filename = NULL;
    gchar * dest_path = NULL;
    gchar copy_buffer[1024] = {0};
    gint nread = 0, nwritten = 0;

    g_return_val_if_fail(RTCOM_IS_EL(el), -1);
    priv = RTCOM_EL_GET_PRIV(el);

    if (!_ensure_db (el, TRUE))
    {
        g_set_error(error, RTCOM_EL_ERROR, RTCOM_EL_INTERNAL_ERROR,
            "Database isn't opened.");
        return -1;
    }

    if(event_id < 0)
    {
        g_set_error(
                error,
                RTCOM_EL_ERROR,
                RTCOM_EL_INVALID_ARGUMENT_ERROR,
                "Invalid event_id.");
        return -1;
    }
    if(!path)
    {
        g_set_error(
                error,
                RTCOM_EL_ERROR,
                RTCOM_EL_INVALID_ARGUMENT_ERROR,
                "Invalid path.");
        return -1;
    }

    /* Check if EventLogger attachments dir exists. If not, create it */
    dir = g_build_filename(
            el_get_home_dir(),
            CONFIG_DIR,
            ATTACH_DIR,
            NULL);
    if(g_file_test(dir, G_FILE_TEST_EXISTS) == FALSE)
    {
        if(g_mkdir(dir, S_IRWXU) != 0)
        {
            g_warning("Creating directory '%s' failed: %s", dir,
                    g_strerror(errno));
            g_free(dir);
            g_set_error(
                    error,
                    RTCOM_EL_ERROR,
                    RTCOM_EL_INTERNAL_ERROR,
                    "Couldn't create attachments dir.");
            return -1;
        }
    }

    unique_dir = _build_unique_dirname(dir);
    g_free(dir);

    if(g_mkdir(unique_dir, S_IRWXU) != 0)
    {
        g_warning("Creating directory '%s' failed: %s", unique_dir,
                g_strerror(errno));
        g_free(unique_dir);
        g_set_error(
                error,
                RTCOM_EL_ERROR,
                RTCOM_EL_INTERNAL_ERROR,
                "Couldn't create attachment dir.");
        return -1;
    }

    dest_filename = g_path_get_basename(path);
    dest_path = g_build_filename(
            unique_dir,
            dest_filename,
            NULL);

    g_free(dest_filename);
    g_free(unique_dir);

    g_debug("Copying %s to %s", path, dest_path);

    src = fopen(path, "rb");
    if(!src)
    {
        g_warning("Couldn't open %s for reading.", path);
        g_set_error(
                error,
                RTCOM_EL_ERROR,
                RTCOM_EL_INTERNAL_ERROR,
                "Couldn't open %s", path);
        g_free(dest_path);
        return -1;
    }
    dest = fopen(dest_path, "wb");
    if(!dest)
    {
        g_warning("Couldn't open %s for writing.", dest_path);
        g_set_error(
                error,
                RTCOM_EL_ERROR,
                RTCOM_EL_INTERNAL_ERROR,
                "Couldn't open %s", dest_path);
        g_free(dest_path);
        fclose(src);
        return -1;
    }

    while((nread = fread(copy_buffer, 1, sizeof(copy_buffer), src) ) > 0)
    {
        nwritten = fwrite(copy_buffer, 1, nread, dest);
        if(nwritten != nread)
        {
            g_warning("Error copying!");
            g_set_error(
                    error,
                    RTCOM_EL_ERROR,
                    RTCOM_EL_INTERNAL_ERROR,
                    "Error copying.");
            g_free(dest_path);
            fclose(src);
            fclose(dest);
            return -1;
        }
    }

    if(fclose(src) == EOF)
    {
        g_warning("Error closing src!");
        g_set_error(
                error,
                RTCOM_EL_ERROR,
                RTCOM_EL_INTERNAL_ERROR,
                "Error closing source file.");
        g_free(dest_path);
        fclose(dest);
        return -1;
    }

    if(fclose(dest) == EOF)
    {
        g_warning("Error closing dest!");
        g_set_error(
                error,
                RTCOM_EL_ERROR,
                RTCOM_EL_INTERNAL_ERROR,
                "Error closing destination file.");
        g_free(dest_path);
        return -1;
    }

    /* We got the file, let's save the path in the db. */

    if (!rtcom_el_db_exec_printf (priv->db, NULL, NULL, error,
        "INSERT INTO Attachments (event_id, path, desc) VALUES (%d, %Q, %Q);",
        event_id, dest_path, desc))
      {
        return -1;
      }

    attachment_id = sqlite3_last_insert_rowid(priv->db);
    g_free(dest_path);

    return attachment_id;
}

gint rtcom_el_fire_event_updated(
        RTComEl * el,
        gint event_id)
{
    RTComElPrivate * priv = NULL;

    g_return_val_if_fail (RTCOM_IS_EL(el), -1);
    g_return_val_if_fail (event_id > 0, -1);

    priv = RTCOM_EL_GET_PRIV(el);
    g_assert(priv);

    _emit_dbus(el, "EventUpdated", event_id, NULL);
    return 0;
}

gint rtcom_el_set_read_event(
        RTComEl * el,
        gint event_id,
        gboolean read,
        GError ** error)
{
    RTComElPrivate * priv = NULL;

    if(!RTCOM_IS_EL(el))
    {
        g_set_error(
                error,
                RTCOM_EL_ERROR,
                RTCOM_EL_INVALID_ARGUMENT_ERROR,
                "Invalid RTComEl.");
        return -1;
    }

    if (!_ensure_db (el, TRUE))
    {
        g_set_error(error, RTCOM_EL_ERROR, RTCOM_EL_INTERNAL_ERROR,
            "Database isn't opened.");
        return -1;
    }

    if(event_id < 1)
    {
        g_set_error(
                error,
                RTCOM_EL_ERROR,
                RTCOM_EL_INVALID_ARGUMENT_ERROR,
                "Invalid event_id.");
        return -1;
    }

    priv = RTCOM_EL_GET_PRIV(el);
    g_assert(priv);

    if (!rtcom_el_db_exec_printf (priv->db, NULL, NULL, error,
        "UPDATE Events SET is_read = %d WHERE id = %d;",
        read == TRUE, event_id))
      {
        return -1;
      }

    _emit_dbus(el, "EventUpdated", event_id, NULL);
    return 0;
}

gint rtcom_el_set_read_events(
        RTComEl * el,
        gint * event_ids,
        gboolean read,
        GError ** error)
{
    RTComElPrivate * priv = NULL;
    guint i = 0, ret = -1;

    priv = RTCOM_EL_GET_PRIV(el);
    g_assert(priv);

    if(!event_ids)
    {
        g_set_error(
                error,
                RTCOM_EL_ERROR,
                RTCOM_EL_INVALID_ARGUMENT_ERROR,
                "event_ids is NULL.");
        return -1;
    }

    if(!event_ids[0])
    {
        g_set_error(
                error,
                RTCOM_EL_ERROR,
                RTCOM_EL_INVALID_ARGUMENT_ERROR,
                "The first event_id is 0.");
        return -1;
    }

    while(event_ids[i])
    {
        ret = rtcom_el_set_read_event(
                el,
                event_ids[i],
                read,
                error);
        if(ret == -1)
        {
            /* Make sure that the error was set. */
            g_assert(error == NULL || *error != NULL);
            break;
        }
        i++;
    }

    return ret;
}

gint rtcom_el_set_event_flag(
        RTComEl * el,
        gint event_id,
        const gchar * flag,
        GError ** error)
{
    RTComElPrivate * priv = NULL;
    gint flag_value = -1;

    if(!RTCOM_IS_EL(el))
    {
        g_set_error(
                error,
                RTCOM_EL_ERROR,
                RTCOM_EL_INVALID_ARGUMENT_ERROR,
                "Invalid RTComEl.");
        return -1;
    }

    priv = RTCOM_EL_GET_PRIV(el);
    g_assert(priv);

    if (!_ensure_db (el, TRUE))
    {
        g_set_error(error, RTCOM_EL_ERROR, RTCOM_EL_INTERNAL_ERROR,
            "Database isn't opened.");
        return -1;
    }

    if(event_id < 1)
    {
        g_set_error(
                error,
                RTCOM_EL_ERROR,
                RTCOM_EL_INVALID_ARGUMENT_ERROR,
                "Invalid event_id.");
        return -1;
    }

    flag_value = rtcom_el_get_flag_value(el, flag);
    if(flag_value == -1)
    {
        g_set_error(
                error,
                RTCOM_EL_ERROR,
                RTCOM_EL_INVALID_ARGUMENT_ERROR,
                "Flag name not in database.");
        return -1;
    }

    if (!rtcom_el_db_exec_printf (priv->db, NULL, NULL, error,
        "UPDATE Events SET flags = flags | %d WHERE id = %d;",
        flag_value, event_id))
      {
        return -1;
      }

    _emit_dbus(el, "EventUpdated", event_id, NULL);

    return 0;
}

gint rtcom_el_unset_event_flag(
        RTComEl * el,
        gint event_id,
        const gchar * flag,
        GError ** error)
{
    RTComElPrivate * priv = NULL;
    gint flag_value = -1;

    if(!RTCOM_IS_EL(el))
    {
        g_set_error(
                error,
                RTCOM_EL_ERROR,
                RTCOM_EL_INVALID_ARGUMENT_ERROR,
                "Invalid RTComEl.");
        return -1;
    }

    priv = RTCOM_EL_GET_PRIV(el);
    g_assert(priv);

    if (!_ensure_db (el, TRUE))
    {
        g_set_error(error, RTCOM_EL_ERROR, RTCOM_EL_INTERNAL_ERROR,
            "Database isn't opened.");
        return -1;
    }

    if(event_id < 1)
    {
        g_set_error(
                error,
                RTCOM_EL_ERROR,
                RTCOM_EL_INVALID_ARGUMENT_ERROR,
                "Invalid event_id.");
        return -1;
    }


    flag_value = rtcom_el_get_flag_value(el, flag);
    if(flag_value == -1)
    {
        g_set_error(
                error,
                RTCOM_EL_ERROR,
                RTCOM_EL_INVALID_ARGUMENT_ERROR,
                "Flag name not in database.");
        return -1;
    }

    if (!rtcom_el_db_exec_printf (priv->db, NULL, NULL, error,
        "UPDATE Events SET flags = flags & ~%d WHERE id = %d;",
        flag_value, event_id))
      {
        return -1;
      }

    _emit_dbus(el, "EventUpdated", event_id, NULL);

    return 0;
}

gboolean rtcom_el_set_end_time(
        RTComEl * el,
        gint event_id,
        time_t end_time,
        GError ** error)
{
    RTComElPrivate * priv = NULL;

    if(!RTCOM_IS_EL(el))
    {
        g_set_error(
                error,
                RTCOM_EL_ERROR,
                RTCOM_EL_INVALID_ARGUMENT_ERROR,
                "Invalid RTComEl.");
        return FALSE;
    }

    priv = RTCOM_EL_GET_PRIV(el);

    if (!_ensure_db (el, TRUE))
    {
        g_set_error(error, RTCOM_EL_ERROR, RTCOM_EL_INTERNAL_ERROR,
            "Database isn't opened.");
        return FALSE;
    }

    if(event_id < 1)
    {
        g_set_error(
                error,
                RTCOM_EL_ERROR,
                RTCOM_EL_INVALID_ARGUMENT_ERROR,
                "Invalid event_id.");
        return FALSE;
    }

    if(end_time == 0)
        /* nothing to do :) */
        return TRUE;

    if (!rtcom_el_db_exec_printf (priv->db, NULL, NULL, error,
        "UPDATE Events SET end_time=%d WHERE id=%d",
        end_time, event_id))
      {
        return FALSE;
      }

    _emit_dbus(el, "EventUpdated", event_id, NULL);
    return TRUE;
}

RTComElIter * _get_events_core(
        RTComEl * el,
        RTComElQuery * query,
        gboolean atomic)
{
    RTComElIter * it = NULL;
    RTComElPrivate * priv = NULL;
    const gchar * sql = NULL;
    sqlite3_stmt * stmt = NULL;
    gint status;

    g_return_val_if_fail(RTCOM_IS_EL(el), NULL);
    g_return_val_if_fail(RTCOM_IS_EL_QUERY(query), NULL);

    if (!_ensure_db (el, TRUE))
    {
        return NULL;
    }

    priv = RTCOM_EL_GET_PRIV(el);
    g_assert(priv);

    sql = rtcom_el_query_get_sql(query);

    if(sqlite3_prepare(priv->db, sql, -1, &stmt, NULL) != SQLITE_OK)
    {
        g_warning("%s: could not compile: '%s': %s.", G_STRFUNC,
            sql, sqlite3_errmsg(priv->db));

        if(stmt)
            sqlite3_finalize(stmt);
        return NULL;
    }

    if (atomic)
      {
        if (!rtcom_el_db_transaction (priv->db, FALSE, NULL))
        {
            g_warning("%s: could not begin transaction", G_STRFUNC);
            sqlite3_finalize(stmt);
            return NULL;
        }
      }

    status = sqlite3_step(stmt);
    if(status == SQLITE_DONE)
    {
        sqlite3_finalize(stmt);
        stmt = NULL;
        if (atomic)
            rtcom_el_db_rollback (priv->db, NULL);
        return NULL;
    }

    if(status != SQLITE_ROW)
    {
        g_warning("%s: could not step statement: %s", G_STRFUNC,
                sqlite3_errmsg (priv->db));
        sqlite3_finalize(stmt);
        if (atomic)
            rtcom_el_db_rollback (priv->db, NULL);
        stmt = NULL;
        return NULL;
    }

    it = g_object_new(
            RTCOM_TYPE_EL_ITER,
            "el", el,
            "query", query,
            "sqlite3-database", priv->db,
            "sqlite3-statement", stmt,
            "plugins-table", priv->plugins,
            "atomic", atomic,
            NULL);

    if(!RTCOM_IS_EL_ITER(it))
        g_warning("Could not create the iterator.");

    return it;
}

RTComElIter * rtcom_el_get_events(
        RTComEl * el,
        RTComElQuery * query)
{
    return _get_events_core (el, query, FALSE);
}

RTComElIter * rtcom_el_get_events_atomic(
        RTComEl * el,
        RTComElQuery * query)
{
    return _get_events_core (el, query, TRUE);
}

static void
_fetch_event_headers_slave (sqlite3_stmt *stmt, GHashTable *table)
{
  g_hash_table_insert(table,
      g_strdup((gchar *) sqlite3_column_text(stmt, 0)),
      g_strdup((gchar *) sqlite3_column_text(stmt, 1)));
}

GHashTable * rtcom_el_fetch_event_headers(
        RTComEl * el,
        gint event_id)
{
    RTComElPrivate * priv = NULL;
    GHashTable * ret = NULL;

    g_return_val_if_fail(RTCOM_IS_EL(el), NULL);

    priv = RTCOM_EL_GET_PRIV(el);

    if (!_ensure_db (el, TRUE))
    {
        return NULL;
    }

    ret = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, g_free);

    if (!rtcom_el_db_exec_printf (priv->db, (GFunc) _fetch_event_headers_slave,
        ret, NULL, "SELECT name, value FROM Headers WHERE event_id = %d;", event_id))
      {
        g_hash_table_destroy (ret);
        return NULL;
      }

    return ret;
}

static void
_events_by_header_slave (sqlite3_stmt *stmt, GArray *arr)
{
  gint id = sqlite3_column_int(stmt, 0);
  g_array_append_val (arr, id);
}

gint * rtcom_el_get_events_by_header(
        RTComEl * el,
        const gchar * key,
        const gchar * val)
{
    RTComElPrivate * priv;
    GArray * a;
    gint last = -1;

    g_return_val_if_fail(RTCOM_IS_EL(el), NULL);
    g_return_val_if_fail(key != NULL, NULL);
    g_return_val_if_fail(val != NULL, NULL);

    if (!_ensure_db (el, TRUE))
    {
        return NULL;
    }

    priv = RTCOM_EL_GET_PRIV(el);

    a = g_array_new(FALSE, FALSE, sizeof(gint));

    if (!rtcom_el_db_exec_printf (priv->db, (GFunc) _events_by_header_slave,
        a, NULL, "SELECT event_id FROM Headers WHERE name=%Q AND value=%Q;",
        key, val))
      {
        g_array_free (a, TRUE);
        return NULL;
      }

    g_array_append_val(a, last);
    return (gint *) g_array_free(a, FALSE);
}

/* FIXME: rtcom_el_get_unique_*() should all query respective separate tables
 * instead of thrashing the whole of Events */

static void
_unique_remote_col_slave (sqlite3_stmt *stmt, GList **li)
{
  const guchar *val = sqlite3_column_text(stmt, 0);
  *li = g_list_prepend(*li, g_strdup((const gchar *) val));
}

static GList *unique_remote_col (RTComEl *el, const gchar *col)
{
  RTComElPrivate *priv = RTCOM_EL_GET_PRIV(el);
  GList *ret = NULL;

  if (!rtcom_el_db_exec_printf(priv->db, (GFunc) _unique_remote_col_slave,
      &ret, NULL, "SELECT DISTINCT %s FROM Remotes WHERE %s IS NOT NULL", col, col))
    return NULL;

  ret = g_list_reverse(ret);
  return ret;
}

GList * rtcom_el_get_unique_remote_ebook_uids(
        RTComEl * el)
{
    return unique_remote_col (el, "abook_uid");
}

GList * rtcom_el_get_unique_remote_uids(
        RTComEl * el)
{
    return unique_remote_col (el, "remote_uid");
}

GList * rtcom_el_get_unique_remote_names(
        RTComEl * el)
{
    return unique_remote_col (el, "remote_name");
}

static void
_get_group_info_slave (sqlite3_stmt *stmt, gint *vars)
{
  vars[0] = sqlite3_column_int(stmt, 0); /* total events */
  /* unread events = total events - read events */
  vars[1] = sqlite3_column_int(stmt, 0) - sqlite3_column_int(stmt, 1);
  vars[2] = sqlite3_column_int(stmt, 2); /* group flags */
}

gboolean rtcom_el_get_group_info(
        RTComEl * el,
        const gchar * group_uid,
        gint * total_events,
        gint * unread_events,
        gint * group_flags)
{
    RTComElPrivate * priv = NULL;
    gint vars[3] = { 0, 0, 0 }; /* total events, unread events, flags */

    g_return_val_if_fail(el, FALSE);
    g_return_val_if_fail(group_uid, FALSE);

    priv = RTCOM_EL_GET_PRIV(el);

    if (!_ensure_db (el, TRUE))
    {
        return FALSE;
    }

    if (!rtcom_el_db_exec_printf(priv->db, (GFunc) _get_group_info_slave,
        vars, NULL, "SELECT total_events, read_events, flags FROM GroupCache WHERE "
        "group_uid = %Q", group_uid))
      return FALSE;

    if (total_events != NULL)
      *total_events = vars[0];

    if (unread_events != NULL)
      *unread_events = vars[1];

    if (group_flags != NULL)
      *group_flags = vars[2];

    return TRUE;
}

gint rtcom_el_get_group_most_recent_event_id(
        RTComEl * el,
        const gchar * group_uid)
{
    RTComElPrivate * priv = NULL;
    gint max_id = -1;

    g_return_val_if_fail(RTCOM_IS_EL(el), -1);
    g_return_val_if_fail(group_uid, -1);

    priv = RTCOM_EL_GET_PRIV(el);

    if (!_ensure_db (el, TRUE))
    {
        return FALSE;
    }

    if (!rtcom_el_db_exec_printf(priv->db, rtcom_el_db_single_int, &max_id, NULL,
        "SELECT MAX(id) FROM Events WHERE group_uid=%Q;", group_uid))
      return -1;

    return max_id;
}

gint rtcom_el_get_flag_value(
        RTComEl * el,
        const gchar * flag)
{
    sqlite3 *db;
    gint ret = -1;

    g_return_val_if_fail(el, -1);
    g_return_val_if_fail(flag, -1);

    if (!_ensure_db (el, TRUE))
    {
        return -1;
    }

    db = RTCOM_EL_GET_PRIV(el)->db;

    if (!rtcom_el_db_exec_printf (db, rtcom_el_db_single_int, &ret, NULL,
        "SELECT value FROM Flags WHERE name=%Q", flag))
      return -1;

    return ret;
}

gint rtcom_el_get_contacts_events_n(
        RTComEl * el,
        const gchar * remote_ebook_uid)
{
    sqlite3 * db;
    gint n;

    g_return_val_if_fail(el, -1);
    g_return_val_if_fail(remote_ebook_uid, -1);

    if (!_ensure_db (el, TRUE))
    {
        return -1;
    }

    db = RTCOM_EL_GET_PRIV(el)->db;

    if (!rtcom_el_db_exec_printf(db, rtcom_el_db_single_int, &n, NULL,
        "SELECT COUNT(*) FROM Events WHERE remote_ebook_uid=%;", 
        remote_ebook_uid))
      return -1;

    return n;
}

gint rtcom_el_get_local_remote_uid_events_n(
        RTComEl * el,
        const gchar * local_uid,
        const gchar * remote_uid)
{
    sqlite3 * db;
    gint n = -1;

    g_return_val_if_fail(el, -1);
    g_return_val_if_fail(local_uid, -1);
    g_return_val_if_fail(remote_uid, -1);

    if (!_ensure_db (el, TRUE))
    {
        return -1;
    }

    db = RTCOM_EL_GET_PRIV(el)->db;

    if (!rtcom_el_db_exec_printf(db, rtcom_el_db_single_int, &n, NULL,
        "SELECT COUNT(*) FROM Events WHERE local_uid=%Q AND remote_uid=%Q;",
        local_uid, remote_uid))
      {
        return -1;
      }

    return n;
}

static void
get_group_uid_slave (sqlite3_stmt *stmt, GSList **li)
{
  *li = g_slist_prepend (*li,
      g_strdup((gchar *) sqlite3_column_text (stmt, 0)));
}

static GSList *
get_event_group_uids (RTComEl *el, gint event_id, const gchar *where)
{
    RTComElPrivate *priv = RTCOM_EL_GET_PRIV (el);
    GSList *li = NULL;

    if (event_id > 0)
    {
        if (!rtcom_el_db_exec_printf (priv->db, (GFunc) get_group_uid_slave,
            &li, NULL, "SELECT DISTINCT(group_uid) FROM Events WHERE id=%d;", event_id))
          return NULL;
    }
    else if (where != NULL)
    {
        if (!rtcom_el_db_exec_printf (priv->db, (GFunc) get_group_uid_slave, &li, NULL,
            "SELECT DISTINCT(Events.group_uid) FROM Events "
                "JOIN Services ON Events.service_id = Services.id "
                "JOIN EventTypes ON Events.event_type_id = EventTypes.id "
                "LEFT JOIN Remotes ON Events.remote_uid = Remotes.remote_uid "
                    "AND Events.local_uid = Remotes.local_uid "
                "LEFT JOIN Headers ON Headers.event_id = Events.id AND "
                    "Headers.name = 'message-token' WHERE %s;", where))
          return NULL;
    }

    return li;
}

static gboolean
update_group_cache (RTComEl *el, GSList *li)
{
    RTComElPrivate *priv = RTCOM_EL_GET_PRIV (el);

    if (li == NULL)
        return TRUE;

    GString *tmp = g_string_sized_new (1024); /* size hint for performance */

    g_string_append (tmp, "INSERT OR REPLACE INTO GroupCache SELECT MAX(id), "
          "service_id, group_uid, COUNT(*), SUM(is_read), SUM(flags) "
          "FROM Events WHERE group_uid IN (");

    while (li != NULL)
    {
        GSList *n = li->next;

        if (li->data != NULL)
        {
            /* FIXME: these assume group_uid can't contain "'" */
            g_string_append_printf (tmp, "'%s'", (const gchar *) li->data);

            if (n != NULL)
                g_string_append_c (tmp, ',');

            g_free (li->data);
        }

        g_slist_free (li);
        li = n;
    }

    g_string_append (tmp, ") GROUP BY group_uid;");

    if (!rtcom_el_db_exec (priv->db, NULL, NULL, tmp->str, NULL))
    {
        g_string_free (tmp, TRUE);
        return FALSE;
    }

    g_string_free (tmp, TRUE);

    /* If there are no events in the group left, the groupcache won't
     * be updated. Clear those. */
    rtcom_el_db_exec (priv->db, NULL, NULL, "DELETE FROM GroupCache "
        "WHERE NOT EXISTS (SELECT id FROM Events WHERE "
            "events.group_uid = groupcache.group_uid LIMIT 1)", NULL);

    return TRUE;
}

/* Note: we rely on constraints (or trigger-based simulation of them)
 * for deleting associated Headers and Attachments */
gint rtcom_el_delete_event (
        RTComEl * el,
        gint event_id,
        GError ** error)
{
    RTComElPrivate *priv = RTCOM_EL_GET_PRIV (el);
    GSList *li;

    if (!_ensure_db (el, TRUE))
    {
        g_set_error(error, RTCOM_EL_ERROR, RTCOM_EL_INTERNAL_ERROR,
            "Database isn't opened.");
        return -1;
    }

    if (event_id < 1)
    {
        g_set_error(error, RTCOM_EL_ERROR, RTCOM_EL_INVALID_ARGUMENT_ERROR,
            "Invalid event_id.");
        return -1;
    }

    /* if disk is full we might not be in position to create journal, so
     * we turn it off temporarily. */
    rtcom_el_db_exec (priv->db, NULL, NULL, "PRAGMA journal_mode = OFF;", NULL);

    if (!rtcom_el_db_transaction (priv->db, FALSE, NULL))
      goto sql_error;

    li = get_event_group_uids (el, event_id, NULL);

    if (!rtcom_el_db_exec_printf (priv->db, NULL, NULL, NULL,
        "DELETE FROM Events WHERE id=%d;", event_id))
      goto sql_error;

    if (!update_group_cache (el, li))
      goto sql_error;

    if (!rtcom_el_db_commit (priv->db, NULL))
      goto sql_error;

    rtcom_el_db_exec (priv->db, NULL, NULL, "PRAGMA journal_mode = TRUNCATE;",
        NULL);
    _emit_dbus(el, "EventDeleted", event_id, NULL);
    return 0;

sql_error:
    rtcom_el_db_rollback (priv->db, NULL);
    g_set_error(error, RTCOM_EL_ERROR, RTCOM_EL_INTERNAL_ERROR,
        "Error executing sql.");
    rtcom_el_db_exec (priv->db, NULL, NULL, "PRAGMA journal_mode = TRUNCATE;",
        NULL);
    return -1;
}

/* Note: we rely on constraints (or trigger-based simulation of them)
 * for deleting associated Headers and Attachments */
gboolean rtcom_el_delete_events(
        RTComEl * el,
        RTComElQuery * query,
        GError ** error)
{
    RTComElPrivate * priv;
    const gchar * where;
    GSList *li;

    g_return_val_if_fail(RTCOM_IS_EL(el), FALSE);
    g_return_val_if_fail(RTCOM_IS_EL_QUERY(query), FALSE);

    if (!_ensure_db (el, TRUE))
    {
        g_set_error(error, RTCOM_EL_ERROR, RTCOM_EL_INTERNAL_ERROR,
            "Database isn't opened.");
        return FALSE;
    }

    where = rtcom_el_query_get_where_clause(query);
    g_return_val_if_fail(where != NULL, FALSE);

    priv = RTCOM_EL_GET_PRIV(el);

    /* if disk is full we might not be in position to create journal, so
     * we turn it off temporarily. */
    rtcom_el_db_exec (priv->db, NULL, NULL, "PRAGMA journal_mode = OFF;", NULL);

    if (!rtcom_el_db_transaction (priv->db, FALSE, NULL))
        goto rtcom_el_delete_events_error;

    li = get_event_group_uids (el, -1, where);

    if (!rtcom_el_db_exec_printf (priv->db, NULL, NULL, NULL,
        "DELETE FROM Events WHERE id IN (SELECT Events.id FROM Events "
        "JOIN Services ON Events.service_id = Services.id "
        "JOIN EventTypes ON Events.event_type_id = EventTypes.id "
        "LEFT JOIN Remotes ON Events.remote_uid = Remotes.remote_uid "
            "AND Events.local_uid = Remotes.local_uid "
        "LEFT JOIN Headers ON Headers.event_id = Events.id AND "
            "Headers.name = 'message-token' WHERE %s);", where))
        goto rtcom_el_delete_events_error;

    if (!update_group_cache (el, li))
        goto rtcom_el_delete_events_error;

    if (!rtcom_el_db_commit (priv->db, NULL))
        goto rtcom_el_delete_events_error;

    rtcom_el_db_exec (priv->db, NULL, NULL, "PRAGMA journal_mode = TRUNCATE;",
        NULL);

    /* What was really deleted depends on the passed query, so we just
     * notify everyone that they should refresh their model, instead of
     * trying to provide deletion details. */
    _emit_dbus(el, "RefreshHint", -1, NULL);

    return TRUE;

rtcom_el_delete_events_error:
    rtcom_el_db_rollback (priv->db, NULL);

    g_set_error(error, RTCOM_EL_ERROR, RTCOM_EL_INTERNAL_ERROR,
        "Error executing sql.");
    rtcom_el_db_exec (priv->db, NULL, NULL, "PRAGMA journal_mode = TRUNCATE;",
        NULL);
    return FALSE;
}

gboolean rtcom_el_delete_by_service(
        RTComEl * el,
        const gchar * service)
{
    RTComElPrivate * priv;
    gint service_id;

    g_return_val_if_fail(RTCOM_IS_EL(el), FALSE);

    if (!_ensure_db (el, TRUE))
    {
        return FALSE;
    }

    service_id = rtcom_el_get_service_id(el, service);
    if(service_id == -1)
    {
        g_warning("%s: couldn't find service %s.", G_STRLOC, service);
        return FALSE;
    }

    priv = RTCOM_EL_GET_PRIV(el);

    /* if disk is full we might not be in position to create journal, so
     * we turn it off temporarily. */
    rtcom_el_db_exec (priv->db, NULL, NULL, "PRAGMA journal_mode = OFF;", NULL);

    if (!rtcom_el_db_transaction (priv->db, FALSE, NULL))
      goto error;

    if (!rtcom_el_db_exec_printf(priv->db, NULL, NULL, NULL,
        "DELETE FROM Events WHERE service_id=%d;", service_id))
      goto error;

    if (!rtcom_el_db_exec_printf (priv->db, NULL, NULL, NULL,
        "DELETE FROM GroupCache WHERE service_id=%d;", service_id))
      goto error;

    if (!rtcom_el_db_commit (priv->db, NULL))
      goto error;

    rtcom_el_db_exec (priv->db, NULL, NULL, "PRAGMA journal_mode = TRUNCATE;",
        NULL);

    _emit_dbus(el, "AllDeleted", -1, service);
    return TRUE;

error:
    rtcom_el_db_rollback (priv->db, NULL);
    rtcom_el_db_exec (priv->db, NULL, NULL, "PRAGMA journal_mode = TRUNCATE;",
        NULL);
    return FALSE;
}

gboolean rtcom_el_delete_by_group_uids(
        RTComEl * el,
        const gchar **group_uids)
{
    RTComElPrivate * priv;
    gint i;

    g_return_val_if_fail(RTCOM_IS_EL(el), FALSE);
    g_return_val_if_fail(group_uids != NULL, FALSE);

    if (!_ensure_db (el, TRUE))
    {
        return FALSE;
    }

    priv = RTCOM_EL_GET_PRIV(el);

    /* if disk is full we might not be in position to create journal, so
     * we turn it off temporarily. */
    rtcom_el_db_exec (priv->db, NULL, NULL, "PRAGMA journal_mode = OFF;", NULL);

    if (!rtcom_el_db_transaction (priv->db, FALSE, NULL))
      goto error;

    for (i = 0; group_uids[i] != NULL; i++)
    {
        if (!rtcom_el_db_exec_printf(priv->db, NULL, NULL, NULL,
            "DELETE FROM Events WHERE group_uid=%Q;", group_uids[i]))
          goto error;
        if (!rtcom_el_db_exec_printf(priv->db, NULL, NULL, NULL,
            "DELETE FROM GroupCache WHERE group_uid=%Q;", group_uids[i]))
          goto error;
    }

    if (!rtcom_el_db_commit (priv->db, NULL))
      goto error;

    rtcom_el_db_exec (priv->db, NULL, NULL, "PRAGMA journal_mode = TRUNCATE;",
        NULL);

    _emit_dbus(el, "RefreshHint", -1, NULL);
    return TRUE;

error:
    rtcom_el_db_rollback (priv->db, NULL);
    rtcom_el_db_exec (priv->db, NULL, NULL, "PRAGMA journal_mode = TRUNCATE;",
        NULL);
    return FALSE;
}

gboolean rtcom_el_delete_all(
        RTComEl * el)
{
    RTComElPrivate * priv;

    g_return_val_if_fail(RTCOM_IS_EL(el), FALSE);

    priv = RTCOM_EL_GET_PRIV(el);

    if (!_ensure_db (el, TRUE))
    {
        return FALSE;
    }

    /* if disk is full we might not be in position to create journal, so
     * we turn it off temporarily. */
    rtcom_el_db_exec (priv->db, NULL, NULL, "PRAGMA journal_mode = OFF;", NULL);

    if (!rtcom_el_db_transaction (priv->db, FALSE, NULL))
      goto delete_all_err;

    if (!rtcom_el_db_exec (priv->db, NULL, NULL, "DELETE FROM Events;", NULL))
      goto delete_all_err;

    if (!rtcom_el_db_commit (priv->db, NULL))
      goto delete_all_err;

    rtcom_el_db_exec (priv->db, NULL, NULL, "PRAGMA journal_mode = TRUNCATE;",
        NULL);

    g_debug("All events, headers and attachments deleted.");
    _emit_dbus(el, "AllDeleted", -1, NULL);
    return TRUE;

delete_all_err:
    rtcom_el_db_rollback (priv->db, NULL);
    rtcom_el_db_exec (priv->db, NULL, NULL, "PRAGMA journal_mode = TRUNCATE;",
        NULL);
    return FALSE;
}

gint rtcom_el_count_by_service(
        RTComEl * el,
        const gchar * service)
{
    RTComElPrivate * priv;
    gint service_id = -1, n;

    g_return_val_if_fail(RTCOM_IS_EL(el), -1);

    priv = RTCOM_EL_GET_PRIV(el);

    if (!_ensure_db (el, TRUE))
    {
        return -1;
    }

    g_debug("%s: getting number of events for service %s.", G_STRLOC, service);

    if(service != NULL)
    {
        service_id = rtcom_el_get_service_id(el, service);
        if(service_id == -1)
        {
            g_warning("%s: couldn't find service %s.", G_STRLOC, service);
            return 0;
        }

        if (!rtcom_el_db_exec_printf(priv->db, rtcom_el_db_single_int, &n, NULL,
            "SELECT COUNT(*) FROM Events WHERE service_id=%d;", service_id))
          return -1;
    }
    else
    {
        if (!rtcom_el_db_exec_printf(priv->db, rtcom_el_db_single_int, &n, NULL,
            "SELECT COUNT(*) FROM Events WHERE service_id=%d;", service_id))
          return -1;
    }

    return n;
}

gint rtcom_el_get_service_id(
        RTComEl * el,
        const gchar * service)
{
    RTComElPrivate * priv;
    gpointer p;

    g_return_val_if_fail(RTCOM_IS_EL(el), -1);

    priv = RTCOM_EL_GET_PRIV(el);
    p = g_hash_table_lookup(priv->services, service);
    if(!p)
        return -1;

    return GPOINTER_TO_INT(p);
}

gint rtcom_el_get_eventtype_id(
        RTComEl * el,
        const gchar * eventtype)
{
    RTComElPrivate * priv;
    gpointer p;

    g_return_val_if_fail(RTCOM_IS_EL(el), -1);

    priv = RTCOM_EL_GET_PRIV(el);
    p = g_hash_table_lookup(priv->event_types, eventtype);
    if(!p)
        return -1;

    return GPOINTER_TO_INT(p);
}

gint rtcom_el_get_flag_id(
        RTComEl * el,
        const gchar * flag)
{
    RTComElPrivate * priv;
    gpointer p;

    g_return_val_if_fail(RTCOM_IS_EL(el), -1);

    priv = RTCOM_EL_GET_PRIV(el);
    p = g_hash_table_lookup(priv->flags, flag);
    if(!p)
        return -1;

    return GPOINTER_TO_INT(p);
}
/******************************************/
/* Public functions implementation ends   */
/******************************************/


/*******************************************/
/* Private functions implementation begins */
/*******************************************/
static const gchar *
el_get_home_dir (void)
{
    static const gchar *home = NULL;

    if (home == NULL)
    {
        home = g_getenv ("RTCOM_EL_HOME");

        if (home == NULL)
        {
            home = g_get_home_dir ();
        }
    }

    return home;
}

static void
_build_db_representation (
        sqlite3 ** db,
        GHashTable ** services,
        GHashTable ** event_types,
        GHashTable ** flags)
{
    g_assert(db);
    g_assert(*db);
    g_assert(services);
    g_assert(event_types);
    g_assert(flags);

    *services = rtcom_el_db_cache_lookup_table (*db, "Services");
    *event_types = rtcom_el_db_cache_lookup_table (*db, "EventTypes");
    *flags = rtcom_el_db_cache_lookup_table (*db, "Flags");
}

static void
_free_db_representation (
        GHashTable ** services,
        GHashTable ** event_types,
        GHashTable ** flags)
{
    if(services && *services)
    {
        g_hash_table_destroy(*services);
        *services = NULL;
    }

    if(event_types && *event_types)
    {
        g_hash_table_destroy(*event_types);
        *event_types = NULL;
    }

    if(flags && *flags)
    {
        g_hash_table_destroy(*flags);
        *flags = NULL;
    }
}

static void
_load_plugins (RTComElPrivate * priv)
{
    const gchar * system_dir = PACKAGE_PLUGINS_DIR;
    gchar * user_dir = NULL;
    gchar * dir = NULL;
    const gchar * env_path = NULL;
    gboolean only_env = FALSE;

    g_assert(priv);

    /* Check if the RTCOM_EL_PLUGINS_PATH is defined. It's
     * not a problem if it isn't, so don't spam warnings. */
    env_path = g_getenv("RTCOM_EL_PLUGINS_PATH");
    if(env_path)
    {
        g_debug("$RTCOM_EL_PLUGINS_PATH = %s", env_path);
        if(g_file_test(env_path, G_FILE_TEST_EXISTS))
        {
            only_env = TRUE;
        }
        else
            g_warning("File doesn't exist: %s", env_path);
    }

    if(only_env)
    {
        if(!_scan_plugins_dir(env_path, priv))
            g_warning("Some env plugins could not be loaded.");
    }
    else
    {
        /* Check if system config dir exists. If not, create it */
        /* FIXME: unused
        g_debug("Checking system config dir...");
        dir = g_build_filename(el_get_home_dir(), CONFIG_DIR, NULL);
        if(g_file_test(dir, G_FILE_TEST_EXISTS) == FALSE)
        {
            g_debug("Creating system config dir...");
            if(g_mkdir(dir, 0775) != 0)
            {
                g_warning("Creating directory '%s' failed: %s", dir,
                        g_strerror(errno));
            }
        }
        g_free(dir);
        */

        /* Check if EventLogger config dir exists. If not, create it */
        dir = g_build_filename(
                el_get_home_dir(),
                CONFIG_DIR,
                NULL);
        if(g_file_test(dir, G_FILE_TEST_EXISTS) == FALSE)
        {
            if(g_mkdir(dir, S_IRWXU) != 0)
            {
                g_warning("Creating directory '%s' failed: %s", dir,
                        g_strerror(errno));
            }
        }
        g_free(dir);

        /* Check if plugins dir exists. If not, create it */
        user_dir = g_build_filename(
                el_get_home_dir(),
                CONFIG_DIR,
                "plugins",
                NULL);
        if(g_file_test(user_dir, G_FILE_TEST_EXISTS) == FALSE)
        {
            if(g_mkdir(user_dir, S_IRWXU) != 0)
            {
                g_warning("Creating directory '%s' failed: %s", user_dir,
                        g_strerror(errno));
            }
        }

        if(!_scan_plugins_dir(system_dir, priv))
            g_warning("Some system plugins could not be loaded.");

        if(!_scan_plugins_dir(user_dir, priv))
            g_warning("Some user plugins could not be loaded.");

        g_free(user_dir);
    }
}

static gboolean
_scan_plugins_dir (
        const gchar * dir,
        RTComElPrivate * priv)
{
    GDir * d = NULL;
    const gchar * filename = NULL;
    gboolean ret = TRUE;

    g_assert(priv);

    if(!(d = g_dir_open(dir, 0, NULL)))
    {
        g_warning("Could not open plugins directory: %s.", dir);
        return FALSE;
    }

    while((filename = g_dir_read_name(d)))
    {
        gchar * extension = NULL;
        gchar * pathname = NULL;

        pathname = g_build_filename(dir, filename, NULL);
        if(g_file_test(pathname, G_FILE_TEST_IS_REGULAR)
                && (extension = strrchr(filename, '.'))
                && (strcmp(++extension, G_MODULE_SUFFIX) == 0))
        {
            ret &= _load_plugin(pathname, priv);
        }
        g_free(pathname);
    }
    g_dir_close(d);

    return ret;
}

static gboolean
_load_plugin (
        const gchar * filename,
        RTComElPrivate * priv)
{
    RTComElPlugin * plugin = g_new0(RTComElPlugin, 1);
    GHashTable * plugins = NULL;
    gint service_id = -1;

    g_assert(filename);
    g_assert(priv);

    plugins = priv->plugins;

    if(!plugin)
    {
        g_warning("Failed to allocate memory for the RTComElPlugin.");
        return FALSE;
    }

    /* Open module ... */
    plugin->module = g_module_open(filename, G_MODULE_BIND_LAZY);
    if(!plugin->module)
    {
        g_warning("Failed to load plugin %s: %s.", filename, g_module_error());
        return FALSE;
    }

    /* ... and bind functions. */
    if(!g_module_symbol(
                plugin->module,
                "rtcom_el_plugin_name",
                (gpointer)(&(plugin->get_name))))
    {
        /* Mandatory */
        g_warning("Couldn't find 'rtcom_el_plugin_name' in %s.", filename);
        return FALSE;
    }

    if(!g_module_symbol(
                plugin->module,
                "rtcom_el_plugin_desc",
                (gpointer)(&(plugin->get_desc))))
    {
        /* Mandatory */
        g_warning("Couldn't find 'rtcom_el_plugin_desc' in %s.", filename);
        return FALSE;
    }

    if(!g_module_symbol(
                plugin->module,
                "rtcom_el_plugin_service",
                (gpointer)(&(plugin->get_service))))
    {
        /* Mandatory */
        g_warning("Couldn't find 'rtcom_el_plugin_service' in %s.", filename);
        return FALSE;
    }

    if(!g_module_symbol(
                plugin->module,
                "rtcom_el_plugin_eventtypes",
                (gpointer)(&(plugin->get_event_types))))
    {
        /* Mandatory */
        g_warning("Couldn't find 'rtcom_el_plugin_eventtypes' in %s.", filename);
        return FALSE;
    }

    /* FIXME: do we really need this, knowing that g_module_check_init() does
     * the same thing? Test plugin doesn't even have this symbol. */
    if(!g_module_symbol(
                plugin->module,
                "rtcom_el_plugin_init",
                (gpointer)(&(plugin->init))))
    {
        g_debug("Couldn't find 'rtcom_el_plugin_init' in %s", filename);
    }

    if(!g_module_symbol(
                plugin->module,
                "rtcom_el_plugin_flags",
                (gpointer)(&(plugin->get_flags))))
    {
        g_debug("Couldn't find 'rtcom_el_plugin_flags' in %s.", filename);
    }

    if(!g_module_symbol(
                plugin->module,
                "rtcom_el_plugin_get_value",
                (gpointer)(&(plugin->get_value))))
    {
        g_debug("Couldn't find 'rtcom_el_plugin_get_value' in %s", filename);
    }

    if((service_id = _init_plugin(plugin, priv)) == -1)
    {
        g_warning("There was an error initializing the plugin.");
        return FALSE;
    }

    /* Finally store it in our list. */
    g_hash_table_insert(
            plugins,
            GINT_TO_POINTER(service_id),
            plugin);

    return TRUE;
}

static gint
_init_plugin (
        RTComElPlugin * plugin,
        RTComElPrivate * priv)
{
    gboolean init;
    const gchar * name = NULL;
    const gchar * desc = NULL;
    RTComElService * service = NULL;
    gint service_id = -1;
    GList * event_types = NULL;
    RTComElEventType * event_type = NULL;
    GList * flags = NULL;
    GList *li;
    RTComElFlag * flag = NULL;
    sqlite3 * db = NULL;
    gpointer p;

    g_assert(plugin);
    g_assert(priv);

    db = priv->db;

    /* Some plugins have init routine, others don't.
     * It's not an error, don't warn on it. */
    init = plugin->init ? plugin->init(db) : FALSE;

    name = plugin->get_name();
    g_return_val_if_fail(name, -1);

    desc = plugin->get_desc();
    if(!desc)
    {
        return -1;
    }

    service = plugin->get_service();
    if(!service)
    {
        return -1;
    }

    plugin->id = 0;
    if (!rtcom_el_db_exec_printf(db, rtcom_el_db_single_int, &plugin->id,
          NULL, "SELECT id FROM Plugins WHERE name = %Q;", name))
    {
        rtcom_el_service_free(service);
        return -1;
    }

    if (plugin->id != 0)
    {
        /* Found! The plugin is already in the table.
         * Let's assume everything is initialized */
        service->plugin_id = plugin->id;

        /* FIXME: each plugin can only register one service? */
        if (!rtcom_el_db_exec_printf(db, rtcom_el_db_single_int, &service->id,
            NULL, "SELECT id from Services WHERE plugin_id = %d;", plugin->id))
          {
            rtcom_el_service_free(service);
            return -1;
          }

        service_id = service->id;
        rtcom_el_service_free(service);

        return service_id;
    }

    /* Plugin not there. */

    if (!rtcom_el_db_exec_printf(db, NULL, NULL, NULL,
        "INSERT INTO Plugins VALUES (NULL,  %Q,  %Q);", name, desc))
      goto db_error;

    plugin->id = sqlite3_last_insert_rowid(db);
    g_debug("Plugin '%s' inserted with id %d.", name, plugin->id);

    /* Check if plugins service is already in.
     * This can happen only because the plugin is not using a really
     * unique service name (FIXME: in that case service selection above
     * will be horribly wrong) */
    p = g_hash_table_lookup(priv->services, service->name);
    if(p)
    {
        g_warning(
                "Service name '%s' already exists. Will unload offending plugin '%s'.",
                service->name,
                name);
        if(!g_module_close(plugin->module))
            g_warning("Could not unload plugin %s: %s.", g_module_name(plugin->module), g_module_error());
        g_free(plugin);

        goto db_error;
    }

    if (!rtcom_el_db_exec_printf (db, NULL, NULL, NULL,
        "INSERT INTO Services VALUES (NULL, %Q, %d, %Q);",
        service->name, plugin->id, service->desc))
      goto db_error;

    service->id = sqlite3_last_insert_rowid(db);
    g_hash_table_insert(priv->services, g_strdup(service->name), GINT_TO_POINTER(service->id));
    service_id = service->id;
    g_debug("Service '%s' inserted with id %d.", service->name, service->id);

    /* Check if plugins event types are already in.
     * This can happen only because the plugin is not using really
     * unique eventtypes names. */
    if(plugin->get_event_types)
    {
        event_types = plugin->get_event_types();

        for (li = event_types; li; li = li->next)
        {
            event_type = (RTComElEventType *) li->data;
            p = g_hash_table_lookup(priv->event_types, event_type->name);
            if(p)
            {
                g_warning(
                        "Event type '%s' already exists. Will unload offending plugin '%s'.",
                        event_type->name,
                        name);
                if(!g_module_close(plugin->module))
                    g_warning("Could not unload plugin %s: %s.", g_module_name(plugin->module), g_module_error());
                g_free(plugin);

                g_list_foreach(event_types, (GFunc) rtcom_el_eventtype_free, NULL);
                g_list_free(event_types);
                goto db_error;
            }

            if (!rtcom_el_db_exec_printf(db, NULL, NULL, NULL,
                "INSERT INTO EventTypes VALUES (NULL, %Q, %d, %Q);",
                event_type->name, plugin->id, event_type->desc))
            {
                g_list_foreach(event_types, (GFunc) rtcom_el_eventtype_free, NULL);
                g_list_free(event_types);
                goto db_error;
            }

            event_type->id = sqlite3_last_insert_rowid(db);
            g_hash_table_insert(priv->event_types, g_strdup(event_type->name), GINT_TO_POINTER(event_type->id));
            g_debug("EventType %s inserted.", event_type->name);
        }

        g_list_foreach(event_types, (GFunc) rtcom_el_eventtype_free, NULL);
        g_list_free(event_types);
    }

    /* Check if plugins flags are already in.
     * This can happen only because the plugin is not using really
     * unique flag names. */
    if(plugin->get_flags)
    {
        flags = plugin->get_flags();

        for (li = flags; li; li = li->next)
        {

            flag = (RTComElFlag *) li->data;
            p = g_hash_table_lookup(priv->flags, flag->name);
            if(p)
            {
                g_warning(
                        "Flag '%s' already exists. Will unload offending plugin '%s'.",
                        flag->name,
                        name);
                if(!g_module_close(plugin->module))
                    g_warning("Could not unload plugin %s: %s.", g_module_name(plugin->module), g_module_error());
                g_free(plugin);

                g_list_foreach(flags, (GFunc) rtcom_el_flag_free, NULL);
                g_list_free(flags);

                goto db_error;
            }

            if (!rtcom_el_db_exec_printf(db, NULL, NULL, NULL,
                "INSERT INTO Flags VALUES (NULL, %d, %Q, %d, %Q);",
                service->id, flag->name, flag->value, flag->desc))
            {
                g_list_foreach(flags, (GFunc) rtcom_el_flag_free, NULL);
                g_list_free(flags);
                goto db_error;
            }
            flag->id = sqlite3_last_insert_rowid(db);
            g_hash_table_insert(priv->flags, g_strdup(flag->name), GINT_TO_POINTER(flag->id));
            g_debug("Flag %s inserted.", flag->name);
        }

        g_list_foreach(flags, (GFunc) rtcom_el_flag_free, NULL);
        g_list_free(flags);
    }

    rtcom_el_service_free(service);

    return service_id;

db_error:
    rtcom_el_db_rollback (db, NULL);
    rtcom_el_service_free(service);
    return -1;
}

static void
_unload_plugins (GHashTable ** plugins)
{
    g_assert(plugins);

    g_hash_table_foreach(
            *plugins,
            _unload_plugin,
            NULL);
    g_hash_table_destroy(*plugins);
}

static void
_unload_plugin (
        gpointer key,
        gpointer value,
        gpointer user_data)
{
    RTComElPlugin * plugin = (RTComElPlugin *) value;

    if(!g_module_close(plugin->module))
        g_warning("Could not unload plugin %s: %s", g_module_name(plugin->module), g_module_error());
    g_free(plugin);
}

static gchar *
_build_unique_dirname (const gchar * parent)
{
    gchar * tmp = NULL;
    GString * dir = NULL;
    guint i = 0;
    time_t t = 0;
    struct tm * stm = NULL;
    gchar time_buf[12+1] = {0};

    g_return_val_if_fail(parent, NULL);

    /* XXX: racy if you don't create the directory in the act */

    dir = g_string_new(NULL);
    do
    {
        t = time(0);
        stm = gmtime(&t);
        memset(time_buf, 0, 12+1);
        strftime(time_buf, 12+1, "%Y%m%d%H%M", stm);
        tmp = g_build_filename(
                parent,
                time_buf,
                NULL);

        g_string_erase(dir, 0, -1);
        g_string_append(dir, tmp);
        if(i != 0)
            g_string_append_printf(dir, "-%d", i);
        g_free(tmp);

        i++;
    } while(g_file_test(dir->str, G_FILE_TEST_EXISTS));

    return g_string_free(dir, FALSE);
}

/* FIXME: Use dbus-glib signal bindings on GObjects for the win */
static void
_emit_dbus (
        RTComEl * el,
        const gchar * signal,
        gint event_id,
        const gchar * service)
{
    DBusError        err;
    DBusConnection * con = NULL;
    DBusMessage *    msg = NULL;
    DBusMessageIter  args;
    dbus_uint32_t    serial = 0;

    const gchar * empty_string = "";

    gchar * local_uid = NULL, * remote_uid = NULL,
          * remote_ebook_uid = NULL, * group_uid = NULL;

    g_return_if_fail(RTCOM_IS_EL(el));

    if ((event_id > 0) && strcmp(signal, "EventDeleted"))
    {
        _get_events_dbus_data(
                el, event_id,
                &local_uid, &remote_uid, &remote_ebook_uid, &group_uid);
    }

    dbus_error_init(&err);

    con = dbus_bus_get(DBUS_BUS_SESSION, &err);
    if(dbus_error_is_set(&err))
    {
        g_warning("Could not aquire dbus connection: %s", err.message);
        dbus_error_free(&err);
        goto free_data_and_return;
    }

    msg = dbus_message_new_signal(DBUS_PATH, DBUS_INTERFACE, signal);
    if(!msg)
    {
        g_warning("Could not allocate dbus message.");
        goto free_data_and_return;
    }

    dbus_message_iter_init_append(msg, &args);
    if(!dbus_message_iter_append_basic(&args, DBUS_TYPE_INT32, &event_id))
    {
        g_warning("Could not append arg to signal.");
        goto free_data_and_return;
    }

    dbus_message_iter_init_append(msg, &args);
    if(!dbus_message_iter_append_basic(
                &args, DBUS_TYPE_STRING,
                local_uid != NULL ?
                (const gchar **) &local_uid : &empty_string))
    {
        g_warning("Could not append arg to signal.");
        goto free_data_and_return;
    }

    dbus_message_iter_init_append(msg, &args);
    if(!dbus_message_iter_append_basic(
                &args, DBUS_TYPE_STRING,
                remote_uid != NULL ?
                (const gchar **) &remote_uid : &empty_string))
    {
        g_warning("Could not append arg to signal.");
        goto free_data_and_return;
    }

    dbus_message_iter_init_append(msg, &args);
    if(!dbus_message_iter_append_basic(&args, DBUS_TYPE_STRING,
                remote_ebook_uid != NULL ?
                (const gchar **) &remote_ebook_uid : &empty_string))
    {
        g_warning("Could not append arg to signal.");
        goto free_data_and_return;
    }

    dbus_message_iter_init_append(msg, &args);
    if(!dbus_message_iter_append_basic(&args, DBUS_TYPE_STRING,
                group_uid ? (const gchar **) &group_uid : &empty_string))
    {
        g_warning("Could not append arg to signal.");
        goto free_data_and_return;
    }

    dbus_message_iter_init_append(msg, &args);
    if(!dbus_message_iter_append_basic(&args, DBUS_TYPE_STRING,
                service != NULL ? &service : &empty_string))
    {
        g_warning("Could not append arg to signal.");
        goto free_data_and_return;
    }

    if(!dbus_connection_send(con, msg, &serial))
    {
        g_warning("Could not send signal!");
    }

    dbus_connection_flush(con);

free_data_and_return:
    if (msg)
        dbus_message_unref(msg);
    dbus_connection_unref(con);
    g_free (local_uid);
    g_free (remote_uid);
    g_free (remote_ebook_uid);
    g_free (group_uid);
}

static DBusHandlerResult
_dbus_filter_callback (
        DBusConnection * con,
        DBusMessage * msg,
        void * user_data)
{
    RTComEl * el = user_data;
    gint event_id = -1;
    const gchar * local_uid = NULL, * remote_uid = NULL,
                * group_uid = NULL, * remote_ebook_uid = NULL,
                * service = NULL;
    const gchar * interface;

    if(!RTCOM_IS_EL(el))
    {
        g_warning("%s: eventlogger object is invalid or disposed", G_STRFUNC);
        return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
    }

    interface = dbus_message_get_interface(msg);
    if(g_strcmp0(interface, DBUS_INTERFACE) == 0)
    {
        const gchar * member = dbus_message_get_member(msg);

        if(dbus_message_get_args(
                    msg, NULL,
                    DBUS_TYPE_INT32, &event_id,
                    DBUS_TYPE_STRING, &local_uid,
                    DBUS_TYPE_STRING, &remote_uid,
                    DBUS_TYPE_STRING, &remote_ebook_uid,
                    DBUS_TYPE_STRING, &group_uid,
                    DBUS_TYPE_STRING, &service,
                    DBUS_TYPE_INVALID))
        {
            gint signal = -1;

            if(g_strcmp0(member, "NewEvent") == 0)
            {
                signal = NEW_EVENT;
            }
            else if(g_strcmp0(member, "EventUpdated") == 0)
            {
                signal = EVENT_UPDATED;
            }
            else if(g_strcmp0(member, "EventDeleted") == 0)
            {
                signal = EVENT_DELETED;
            }

            if(signal != -1)
            {
                g_signal_emit(el, signals[signal], 0,
                              event_id, local_uid, remote_uid,
                              remote_ebook_uid, group_uid, service);
            }
            else if(g_strcmp0(member, "AllDeleted") == 0)
            {
                signal = ALL_DELETED;

                g_signal_emit(el, signals[signal], 0, service);
            }
            else if(g_strcmp0(member, "RefreshHint") == 0)
            {
                g_signal_emit(el, signals[REFRESH_HINT], 0);
            }
            else if(g_strcmp0(member, "DbReopen") == 0)
            {
                RTComElPrivate *priv = RTCOM_EL_GET_PRIV(el);

                /* If needed, reopen the DB and send the refresh-hint
                 * signal. Take care not to emit DbReopen D-Bus signal
                 * again. */
                if (priv->db == NULL)
                {
                    if (_ensure_db(el, FALSE))
                    {
                        g_signal_emit(el, signals[REFRESH_HINT], 0);
                    }
                }
            }
        }
    }

    return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
}

static void _get_events_dbus_data(
        RTComEl * el,
        gint event_id,
        gchar ** local_uid,
        gchar ** remote_uid,
        gchar ** remote_ebook_uid,
        gchar ** group_uid)
{
    RTComElPrivate * priv;
    const gchar * sql;
    sqlite3_stmt * stmt;
    int ret;

    g_return_if_fail(RTCOM_IS_EL(el));
    g_return_if_fail(event_id > 0);
    /* At least one should be defined, or what's the point of calling
     * this? */
    g_return_if_fail(local_uid || remote_uid || remote_ebook_uid || group_uid);

    priv = RTCOM_EL_GET_PRIV(el);
    sql = "SELECT Remotes.local_uid as local_uid, Events.remote_uid AS remote_uid, abook_uid, group_uid from "
          "Events LEFT JOIN Remotes ON Events.remote_uid = Remotes.remote_uid AND "
          "Events.local_uid = Remotes.local_uid WHERE id=?;";

    ret = sqlite3_prepare(priv->db, sql, -1, &stmt, NULL);
    if(ret != SQLITE_OK)
    {
        g_warning("Could not prepare statement for sql \"%s\": %s", sql,
            sqlite3_errmsg(priv->db));
        return;
    }

    ret = sqlite3_bind_int(stmt, 1, event_id);
    if(ret != SQLITE_OK)
    {
        g_warning("Could not bind int %d to sql: %s", event_id, sql);
        sqlite3_finalize(stmt);
        return;
    }

    ret = sqlite3_step(stmt);
    if(ret == SQLITE_DONE)
    {
        g_warning("No event with id %d.", event_id);
    }
    else if(ret == SQLITE_ROW)
    {
        if(local_uid)
            *local_uid = g_strdup((gchar *) sqlite3_column_text(stmt, 0));
        if(remote_uid)
            *remote_uid = g_strdup((gchar *) sqlite3_column_text(stmt, 1));
        if(remote_ebook_uid)
            *remote_ebook_uid =
                g_strdup((gchar *) sqlite3_column_text(stmt, 2));
        if(group_uid)
            *group_uid = g_strdup((gchar *) sqlite3_column_text(stmt, 3));
    }
    else
    {
        g_warning("Could not step statement for sql: %s", sql);
    }

    sqlite3_finalize(stmt);
}

/*******************************************/
/* Private functions implementation ends   */
/*******************************************/

/* vim: set ai et tw=75 ts=4 sw=4: */

