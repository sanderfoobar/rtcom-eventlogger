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

#include "rtcom-eventlogger/eventlogger-attach-iter.h"
#include "rtcom-eventlogger/db.h"

#include <sqlite3.h>

#define RTCOM_EL_ATTACH_ITER_GET_PRIV(it) (G_TYPE_INSTANCE_GET_PRIVATE ((it), \
            RTCOM_TYPE_EL_ATTACH_ITER, RTComElAttachIterPrivate))
G_DEFINE_TYPE(RTComElAttachIter, rtcom_el_attach_iter, G_TYPE_OBJECT);

typedef struct _RTComElAttachIterPrivate RTComElAttachIterPrivate;
struct _RTComElAttachIterPrivate {
    sqlite3 * db;
    sqlite3_stmt * stmt;
};

enum {
    RTCOM_EL_ATTACH_ITER_PROP_DB = 1,
    RTCOM_EL_ATTACH_ITER_PROP_STMT
};

void rtcom_el_free_attachment(
        RTComElAttachment * e)
{
    g_return_if_fail(e);

    g_free(e->path);
    g_free(e->desc);

    g_slice_free (RTComElAttachment, e);
}

static void rtcom_el_attach_iter_set_property(
        GObject * obj,
        guint prop_id,
        const GValue * value,
        GParamSpec * pspec)
{
    RTComElAttachIterPrivate * priv = RTCOM_EL_ATTACH_ITER_GET_PRIV(obj);

    switch(prop_id)
    {
        case RTCOM_EL_ATTACH_ITER_PROP_DB:
            priv->db = g_value_get_pointer(value);
            break;

        case RTCOM_EL_ATTACH_ITER_PROP_STMT:
            priv->stmt = g_value_get_pointer(value);
            break;

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, prop_id, pspec);
            break;
    }
}

static void rtcom_el_attach_iter_get_property(
        GObject * obj,
        guint prop_id,
        GValue * value,
        GParamSpec * pspec)
{
    RTComElAttachIterPrivate * priv = RTCOM_EL_ATTACH_ITER_GET_PRIV(obj);

    switch(prop_id)
    {
        case RTCOM_EL_ATTACH_ITER_PROP_DB:
            g_value_set_pointer(value, priv->db);
            break;

        case RTCOM_EL_ATTACH_ITER_PROP_STMT:
            g_value_set_pointer(value, priv->stmt);
            break;

        default:
            G_OBJECT_WARN_INVALID_PROPERTY_ID(obj, prop_id, pspec);
            break;
    }
}

static void rtcom_el_attach_iter_init(
        RTComElAttachIter * it)
{
    RTComElAttachIterPrivate * priv = NULL;

    priv = RTCOM_EL_ATTACH_ITER_GET_PRIV(it);
    priv->db = NULL;
    priv->stmt = NULL;
}

static void rtcom_el_attach_iter_finalize(
        GObject * object)
{
    RTComElAttachIterPrivate * priv = RTCOM_EL_ATTACH_ITER_GET_PRIV(object);

    if(priv->stmt)
    {
        sqlite3_finalize(priv->stmt);
        priv->stmt = NULL;
    }

    G_OBJECT_CLASS(rtcom_el_attach_iter_parent_class)->finalize(object);
}

static void rtcom_el_attach_iter_class_init(
        RTComElAttachIterClass * klass)
{
    GObjectClass * object_class = G_OBJECT_CLASS(klass);
    g_type_class_add_private(object_class, sizeof (RTComElAttachIterPrivate));
    object_class->finalize = rtcom_el_attach_iter_finalize;
    object_class->set_property = rtcom_el_attach_iter_set_property;
    object_class->get_property = rtcom_el_attach_iter_get_property;

    g_object_class_install_property(
            object_class,
            RTCOM_EL_ATTACH_ITER_PROP_DB,
            g_param_spec_pointer(
                "sqlite3-database",
                "SQLite3 Database",
                "The SQLite3 Database",
                G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

    g_object_class_install_property(
            object_class,
            RTCOM_EL_ATTACH_ITER_PROP_STMT,
            g_param_spec_pointer(
                "sqlite3-statement",
                "SQLite3 Statement",
                "The SQLite3 Statement for this iterator",
                G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));
}

gboolean rtcom_el_attach_iter_first(
        RTComElAttachIter * it)
{
    RTComElAttachIterPrivate * priv = NULL;

    g_return_val_if_fail(it, FALSE);
    g_return_val_if_fail(RTCOM_IS_EL_ATTACH_ITER(it), FALSE);

    priv = RTCOM_EL_ATTACH_ITER_GET_PRIV(it);
    g_return_val_if_fail(priv->stmt, FALSE);

    sqlite3_reset(priv->stmt);

    return rtcom_el_attach_iter_next(it);
}

gboolean rtcom_el_attach_iter_next(
        RTComElAttachIter * it)
{
    RTComElAttachIterPrivate * priv = NULL;
    gint status;

    g_return_val_if_fail(it, FALSE);
    g_return_val_if_fail(RTCOM_IS_EL_ATTACH_ITER(it), FALSE);

    priv = RTCOM_EL_ATTACH_ITER_GET_PRIV(it);
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
        priv->stmt = NULL;
        return FALSE;
    }

    return TRUE;
}

RTComElAttachment * rtcom_el_attach_iter_get(
        RTComElAttachIter * it)
{
    sqlite3_stmt *stmt;
    RTComElAttachment * ret = NULL;

    stmt = RTCOM_EL_ATTACH_ITER_GET_PRIV(it)->stmt;
    g_return_val_if_fail (stmt != NULL, NULL);

    ret = g_slice_new0 (RTComElAttachment);

    ret->id = sqlite3_column_int (stmt, 0);
    ret->event_id = sqlite3_column_int (stmt, 1);
    ret->path = g_strdup ((const gchar *) sqlite3_column_text (stmt, 2));
    ret->desc = g_strdup ((const gchar *) sqlite3_column_text (stmt, 3));

    return ret;
}

RTComElAttachment *
rtcom_el_attachment_new (const gchar *path,
                         const gchar *desc)
{
    RTComElAttachment *a;

    g_return_val_if_fail (path != NULL, NULL);
    /* desc is allowed to be NULL */

    a = g_slice_new0 (RTComElAttachment);

    a->id = 0;
    a->event_id = 0;
    a->path = g_strdup (path);
    a->desc = g_strdup (desc);

    return a;
}

/* vim: set ai et tw=75 ts=4 sw=4: */

