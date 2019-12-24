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

#ifndef __RTCOM_EL_DB_H__
#define __RTCOM_EL_DB_H__

#include <sqlite3.h>
#include <glib.h>

G_BEGIN_DECLS

typedef sqlite3 *rtcom_el_db_t;
typedef sqlite3_stmt *rtcom_el_db_stmt_t;

#define RTCOM_EL_DB_MAX_BUSYLOOP_TIME 2.00 /* in seconds */
#define RTCOM_EL_ERROR rtcom_el_error_quark ()

rtcom_el_db_t rtcom_el_db_open (const gchar *fname);
void rtcom_el_db_close (rtcom_el_db_t db);
gboolean rtcom_el_db_exec (rtcom_el_db_t db, GFunc cb, gpointer user_data,
    const gchar *sql, GError **error);
gboolean rtcom_el_db_exec_printf (rtcom_el_db_t db, GFunc cb,
    gpointer user_data, GError **error, const gchar *fmt, ...);
gboolean rtcom_el_db_transaction (rtcom_el_db_t db, gboolean exclusive,
    GError **error);
gboolean rtcom_el_db_commit (rtcom_el_db_t db, GError **error);
gboolean rtcom_el_db_rollback (rtcom_el_db_t db, GError **error);
gint rtcom_el_db_iterate (rtcom_el_db_t db, rtcom_el_db_stmt_t stmt,
    GError **error);
void rtcom_el_db_single_int (gpointer data, gpointer user_data);
GHashTable * rtcom_el_db_cache_lookup_table (rtcom_el_db_t db,
const gchar *tname);

const gchar ** rtcom_el_db_schema_get_sql ();
gint rtcom_el_db_schema_get_n_items ();
void rtcom_el_db_schema_get_mappings (const gchar **out_selection,
    GHashTable **out_mapping, GHashTable **out_typing);
void rtcom_el_db_g_value_slice_free (gpointer p);
void rtcom_el_db_schema_update_row (rtcom_el_db_stmt_t stmt, GHashTable *row);
GHashTable *rtcom_el_db_schema_get_row (rtcom_el_db_stmt_t stmt);

gboolean rtcom_el_db_convert_from_db0 (const gchar *fname,
    const gchar *old_fname);

G_END_DECLS

#endif /* __RTCOM_EL_DB_H__ */

