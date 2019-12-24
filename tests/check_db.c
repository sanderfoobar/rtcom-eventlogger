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

#include "rtcom-eventlogger/eventlogger.h"
#include "rtcom-eventlogger/db.h"
#include "rtcom-eventlogger/eventlogger-types.h"

#include <glib.h>
#include <glib/gstdio.h>
#include <glib-object.h>
#include <stdlib.h>
#include <stdio.h>
#include <check.h>

static const gchar *fname = "/tmp/check_db.sqlite";

START_TEST(db_test_db)
{
  FILE *fp;
  rtcom_el_db_t db;
  gint cnt = -1;
  GError *err = NULL;
  GHashTable *t;

  /* Invalidate the database. Note that strings too short will result in IO
   * error instead of db corrupted error. */
  g_unlink (fname);
  fp = fopen (fname, "w");
  fprintf (fp, "illegaldata000000000000000000000000000000illegaldata");
  fclose (fp);

  /* Test that the open succeeds and db is initialised. */
  db = rtcom_el_db_open (fname);
  fail_unless (db != NULL);
  rtcom_el_db_close (db);

  db = rtcom_el_db_open (fname);
  fail_unless (db != NULL);

  /* Test that there are 0 events (but schema exists). */
  rtcom_el_db_exec (db, rtcom_el_db_single_int, &cnt,
      "SELECT COUNT(*) FROM Events;", NULL);
  fail_unless (cnt == 0);

  /* Test that internal error is returned on invalid query. */
  fail_if (rtcom_el_db_exec (db, NULL, NULL, "BOGUS;", &err));
  fail_unless (err != NULL);
  fail_unless (err->domain == RTCOM_EL_ERROR);
  fail_unless (err->code == RTCOM_EL_INTERNAL_ERROR);
  g_error_free (err);

  rtcom_el_db_transaction (db, FALSE, NULL);

  /* Test lookup tables. */
  rtcom_el_db_exec (db, NULL, NULL, "DELETE FROM Services", NULL);
  rtcom_el_db_exec (db, NULL, NULL,
      "INSERT INTO SERVICES (id, name) VALUES (1, 'One');", NULL);
  rtcom_el_db_exec (db, NULL, NULL,
      "INSERT INTO SERVICES (id, name) VALUES (2, 'Two');", NULL);
  rtcom_el_db_exec (db, NULL, NULL,
      "INSERT INTO SERVICES (id, name) VALUES (3, 'Three');", NULL);

  rtcom_el_db_commit (db, NULL);

  t = rtcom_el_db_cache_lookup_table (db, "Services");
  fail_unless (t != NULL);
  fail_unless (g_hash_table_size (t) == 3);
  fail_unless (GPOINTER_TO_INT (g_hash_table_lookup (t, "Three")) == 3);
  g_hash_table_destroy (t);

  rtcom_el_db_close (db);
}
END_TEST

struct schema_test_ctx {
    gchar *field;
    gchar *column;
    GType type;
};

static void
_verify_column_type (gpointer data, gpointer user_data)
{
  rtcom_el_db_stmt_t stmt = data;
  struct schema_test_ctx *ctx = user_data;
  const gchar *dbtyp;

  if (g_strcmp0 ((const gchar *) sqlite3_column_text (stmt, 1),
        ctx->column))
    return;

  dbtyp = (const gchar *) sqlite3_column_text (stmt, 2);
  if (!g_strcmp0 (dbtyp, "INTEGER"))
    {
      fail_unless ((ctx->type == G_TYPE_INT) ||
          (ctx->type == G_TYPE_BOOLEAN));
    }
  else if (!g_strcmp0 (dbtyp, "TEXT"))
    {
      fail_unless (ctx->type == G_TYPE_STRING);
    }
  else if (!g_strcmp0 (dbtyp, "BOOL"))
    {
      fail_unless (ctx->type == G_TYPE_BOOLEAN);
    }
  else
    {
      fail("Mismatched db type.");
    }
}

START_TEST(db_test_schema)
{
  rtcom_el_db_t db;
  GHashTable *map;
  GHashTable *typ;
  GList *li, *i;
  struct schema_test_ctx ctx = { NULL, G_TYPE_INVALID };

  /* DB should be available as a result of test_db */
  db = rtcom_el_db_open (fname);
  fail_unless (db != NULL);

  rtcom_el_db_schema_get_mappings (NULL, &map, &typ);
  fail_unless (g_hash_table_size (map) == g_hash_table_size (typ));

  /* Split fields into Table.column, get schema for table and verify
   * existence and type of column. */
  li = g_hash_table_get_keys (map);
  for (i = li; i; i = g_list_next (i))
    {
      gboolean ret;
      gchar *field = i->data;
      gchar **tmp;

      tmp = g_strsplit (g_hash_table_lookup (map, field), ".", 2);
      fail_unless (tmp[0] && tmp[1] && !tmp[2]);

      ctx.field = field;
      ctx.column = tmp[1];
      ctx.type = GPOINTER_TO_UINT (g_hash_table_lookup (typ, field));
      fail_unless (ctx.type != G_TYPE_INVALID);

      ret = rtcom_el_db_exec_printf (db, _verify_column_type, &ctx, NULL,
          "PRAGMA table_info(%s);", tmp[0]);
      fail_unless (ret == TRUE);

      g_strfreev (tmp);
    }

  g_list_free (li);

  rtcom_el_db_close (db);
}
END_TEST

static void
_cause_mischief (gpointer data, gpointer user_data)
{
  rtcom_el_db_t db = user_data;

  fail_if (rtcom_el_db_transaction (db, TRUE, NULL));
}

START_TEST(db_test_events)
{
  rtcom_el_db_t db;
  const gchar *sel;
  GHashTable *map;
  GHashTable *typ;
  gint i;

  db = rtcom_el_db_open (fname);
  fail_unless (db != NULL);

  rtcom_el_db_schema_get_mappings (&sel, &map, &typ);
  fail_unless (g_hash_table_size (map) == g_hash_table_size (typ));

  /* First start of transaction should succeed. */
  fail_unless (rtcom_el_db_transaction (db, FALSE, NULL));

  /* Second should fail because we don't support nested transactions. */
  fail_if (rtcom_el_db_transaction (db, FALSE, NULL));

  for (i = 0; i < 100; i++) {
    rtcom_el_db_exec (db, NULL, NULL,
        "INSERT INTO Events (service_id, event_type_id, storage_time, " \
            "start_time) VALUES (0, 0, 0, 0);", NULL);
  }

  rtcom_el_db_commit (db, NULL);

  i = 0;
  rtcom_el_db_exec (db, rtcom_el_db_single_int, &i,
      "SELECT COUNT(*) FROM Events", NULL);
  fail_unless (i == 100);

  /* Try to start a new transaction while an existing statement
   * is ongoing. This should fail. */
  rtcom_el_db_exec (db, _cause_mischief, &db,
      "SELECT COUNT(*) FROM Events", NULL);

  rtcom_el_db_exec (db, NULL, NULL, "DELETE FROM Events;", NULL);
  rtcom_el_db_exec (db, rtcom_el_db_single_int, &i,
      "SELECT COUNT(*) FROM Events", NULL);
  fail_unless (i == 0);

  /* Commit outside of transaction should fail. */
  fail_if (rtcom_el_db_commit (db, NULL));
  /* Ditto. */
  fail_if (rtcom_el_db_rollback (db, NULL));

  rtcom_el_db_close (db);
}
END_TEST

void
db_extend_el_suite (Suite *s)
{
    TCase *tc_db = tcase_create ("Database");

    tcase_add_test (tc_db, db_test_db);
    tcase_add_test (tc_db, db_test_schema);
    tcase_add_test (tc_db, db_test_events);

    suite_add_tcase (s, tc_db);
}

