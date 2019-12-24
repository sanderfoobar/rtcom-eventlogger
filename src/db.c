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

#include <glib/gstdio.h>
#include <errno.h>
#include <sched.h>

#include "rtcom-eventlogger/db.h"
#include "rtcom-eventlogger/eventlogger.h"
#include "rtcom-eventlogger/eventlogger-types.h"

typedef struct {
  gchar *name;
  GType type;
  gchar *column;
} EventField;

/* This table encodes the field ordering in the result, API field name,
 * expected type and the SQL column name of the field. */
static EventField fields[] = {
  { "service", G_TYPE_STRING, "Services.name" },
  { "event-type", G_TYPE_STRING, "EventTypes.name" },
  { "id", G_TYPE_INT, "Events.id" },
  { "service-id", G_TYPE_INT, "Events.service_id" },
  { "event-type-id", G_TYPE_INT, "Events.event_type_id" },
  { "storage-time", G_TYPE_INT, "Events.storage_time" },
  { "start-time", G_TYPE_INT, "Events.start_time" },
  { "end-time", G_TYPE_INT, "Events.end_time" },
  { "flags", G_TYPE_INT, "Events.flags" },
  { "is-read", G_TYPE_BOOLEAN, "Events.is_read" },
  { "bytes-sent", G_TYPE_INT, "Events.bytes_sent" },
  { "bytes-received", G_TYPE_INT, "Events.bytes_received" },
  { "local-uid", G_TYPE_STRING, "Events.local_uid" },
  { "local-name", G_TYPE_STRING, "Events.local_name" },
  { "group-uid", G_TYPE_STRING, "Events.group_uid" },
  { "remote-ebook-uid", G_TYPE_STRING, "Remotes.abook_uid" },
  { "remote-uid", G_TYPE_STRING, "Remotes.remote_uid" },
  { "remote-name", G_TYPE_STRING, "Remotes.remote_name" },
  /* Used most of the time, so we might as well special-case preload it. */
  { "message-token", G_TYPE_STRING, "Headers.value" },
  /* FIXME: these should really be in plugins */
  { "channel", G_TYPE_STRING, "Events.channel" },
  { "outgoing", G_TYPE_BOOLEAN, "Events.outgoing" },
  { "free-text", G_TYPE_STRING, "Events.free_text" },
  { NULL, 0, NULL }
};

/* This piece of SQL defines unique_remote to be a string that is unique
 * for every address book contact, and for every possibly-distinct contact
 * who is not in the address book.
 *
 * If the contact is in the abook, unique_remote is 'ab:' plus their e-d-s
 * ID; if not, unique_remote is 'lr:' plus the local and remote IDs joined
 * with ';' (in practice, the local ID is a Mission Control account name,
 * which cannot contain ';', so this is unambiguous).
 *
 * This means that the same remote user ID accessed via two accounts is
 * considered to be different - this is necessary since a username on Skype
 * and the same username on AIM are fairly likely to belong to different
 * people, and the same nickname on two IRC networks might well belong to
 * different people too.
 */
#define UNIQUE_REMOTE " CASE abook_uid IS NOT NULL " \
        "WHEN 1 THEN " \
            "('ab:' || abook_uid) " \
        "ELSE " \
            "('lr:' || Events.local_uid || ';' || Events.remote_uid) " \
        "END AS unique_remote "

#define REQUIRED_USER_VERSION 1

static const gchar *db_schema_sql[] = {
    "PRAGMA user_version = 1;",
    /* Services */
    "CREATE TABLE IF NOT EXISTS Services (" \
    "id INTEGER PRIMARY KEY," \
    "name TEXT NOT NULL UNIQUE," \
    "plugin_id INTEGER," \
    "desc TEXT" \
    ");",
    /* Services is a small table, no need for plugin_id idx */
    "CREATE INDEX IF NOT EXISTS idx_srv_plugin_id ON Services(plugin_id);",
    /* EventTypes */
    "CREATE TABLE IF NOT EXISTS EventTypes (" \
    "id INTEGER PRIMARY KEY," \
    "name TEXT NOT NULL UNIQUE," \
    "plugin_id INTEGER," \
    "desc TEXT" \
    ");",
    /* EventTypes is a small table, no need for plugin_id idx */
    "CREATE INDEX IF NOT EXISTS idx_et_plugin_id ON EventTypes(plugin_id);",
    /* Flags */
    "CREATE TABLE IF NOT EXISTS Flags (" \
    "id INTEGER PRIMARY KEY," \
    "service_id INTEGER NOT NULL," \
    "name TEXT NOT NULL UNIQUE," \
    "value INTEGER NOT NULL," \
    "desc TEXT" \
    ");",
    /* Flags is a small table, no need for plugin_id idx */
    /* Plugins */
    "CREATE TABLE IF NOT EXISTS Plugins (" \
    "id INTEGER PRIMARY KEY," \
    "name TEXT NOT NULL UNIQUE," \
    "desc TEXT NOT NULL" \
    ");",
    /* Attachments */
    "CREATE TABLE IF NOT EXISTS Attachments (" \
    "id INTEGER PRIMARY KEY," \
    "event_id INTEGER NOT NULL," \
    "path TEXT NOT NULL," \
    "desc TEXT" \
    ");",
    "CREATE INDEX IF NOT EXISTS idx_att_event_id ON Attachments(event_id);",
    /* Headers */
    "CREATE TABLE IF NOT EXISTS Headers (" \
    "id INTEGER PRIMARY KEY," \
    "event_id INTEGER NOT NULL," \
    "name TEXT NOT NULL," \
    "value TEXT NOT NULL," \
    "UNIQUE(event_id, name)" \
    ");",
    /* FIXME: not needed because of above compound key? */
    "CREATE INDEX IF NOT EXISTS idx_hdr_event_id ON Headers(event_id);",
    /* Remote contacts */
    "CREATE TABLE IF NOT EXISTS Remotes (" \
    "local_uid TEXT NOT NULL," \
    "remote_uid TEXT NOT NULL," \
    "remote_name TEXT," \
    "abook_uid TEXT," \
    "UNIQUE(local_uid,remote_uid)" \
    ");",
    /* Events */
    "CREATE TABLE IF NOT EXISTS Events (" \
    "id INTEGER PRIMARY KEY," \
    "service_id INTEGER NOT NULL," \
    "event_type_id INTEGER NOT NULL," \
    "storage_time INTEGER NOT NULL," \
    "start_time INTEGER NOT NULL," \
    "end_time INTEGER," \
    "is_read INTEGER DEFAULT 0," \
    "outgoing BOOL DEFAULT 0," \
    "flags INTEGER DEFAULT 0," \
    "bytes_sent INTEGER DEFAULT 0," \
    "bytes_received INTEGER DEFAULT 0," \
    "local_uid TEXT," \
    "local_name TEXT," \
    "remote_uid TEXT," \
    "channel TEXT," \
    "free_text TEXT," \
    "group_uid TEXT" \
    ");",
    /* Group cache */
    "CREATE TABLE IF NOT EXISTS GroupCache (" \
    "event_id INTEGER UNIQUE NOT NULL," \
    "service_id INTEGER NOT NULL," \
    "group_uid TEXT NOT NULL," \
    "total_events INTEGER DEFAULT 0," \
    "read_events INTEGER DEFAULT 0," \
    "flags INTEGER DEFAULT 0," \
    "CONSTRAINT factor UNIQUE(service_id, group_uid)" \
    ");",
    "CREATE INDEX IF NOT EXISTS idx_ev_service_id ON Events(service_id);",
    "CREATE INDEX IF NOT EXISTS idx_ev_event_type_id ON Events(event_type_id);",
    "CREATE INDEX IF NOT EXISTS idx_ev_group_uid ON Events(group_uid);",
    "CREATE INDEX IF NOT EXISTS idx_ev_remote_uid ON Events(remote_uid);",
    "CREATE INDEX IF NOT EXISTS idx_gc_group_uid ON GroupCache(group_uid);",
    /* equivalent to ON DELETE CASCADE for Services and EventTypes */
    "CREATE TRIGGER IF NOT EXISTS fkd_services_event_types_plugin_id " \
       "BEFORE DELETE ON Plugins FOR EACH ROW BEGIN " \
           "DELETE FROM Services WHERE plugin_id = OLD.id; " \
           "DELETE FROM EventTypes WHERE plugin_id = OLD.id; "\
       "END;",
    /* equivalent to ON DELETE CASCADE for Flags */
    "CREATE TRIGGER IF NOT EXISTS fkd_flags_service_id BEFORE DELETE ON Services " \
       "FOR EACH ROW BEGIN " \
           "DELETE FROM Flags WHERE service_id = OLD.id; "\
       "END;",
    /* equivalent to ON DELETE CASCADE for Headers and Attachments */
    "CREATE TRIGGER IF NOT EXISTS fkd_headers_atts_event_id BEFORE DELETE ON Events " \
       "FOR EACH ROW BEGIN " \
           "DELETE FROM Headers WHERE event_id = OLD.id; " \
           "DELETE FROM Attachments WHERE event_id = OLD.id; " \
       "END;",
    "CREATE TRIGGER IF NOT EXISTS gc_update_ev_add1 BEFORE INSERT ON Events " \
       "FOR EACH ROW WHEN NEW.group_uid IS NOT NULL BEGIN " \
           "INSERT OR IGNORE INTO GroupCache (event_id, service_id, group_uid, " \
           "total_events, read_events, flags) VALUES (0, NEW.service_id, " \
           "NEW.group_uid, 0, 0, 0); " \
       "END;",
    /* remove old groupcache update triggers in existing database */
    "DROP TRIGGER IF EXISTS gc_update_ev_add2;",
    "DROP TRIGGER IF EXISTS gc_update_ev_add3;",
    "DROP TRIGGER IF EXISTS gc_update_ev_update;",
    /* this is updated version of gc_update_ev_add2, renamed so we don't
     * drop/recreate trigger on every db open. */
    "CREATE TRIGGER IF NOT EXISTS gc_update_ev_add4 AFTER INSERT ON Events " \
       "FOR EACH ROW WHEN NEW.group_uid IS NOT NULL BEGIN " \
           "UPDATE GroupCache SET event_id = NEW.id, total_events = total_events + 1, " \
           "read_events = read_events + NEW.is_read, flags = flags | NEW.flags " \
           "WHERE group_uid = NEW.group_uid; " \
       "END;",
    "CREATE TRIGGER IF NOT EXISTS gc_update_ev_update AFTER UPDATE ON Events " \
       "FOR EACH ROW WHEN NEW.group_uid IS NOT NULL BEGIN " \
           "UPDATE GroupCache SET " \
               "read_events = read_events - OLD.is_read + NEW.is_read, "\
               "flags = (flags & (~OLD.flags)) | NEW.flags " \
               "WHERE group_uid = NEW.group_uid; " \
        "END;",
    NULL };

/* Busy looping is handled in rtcom_el_db_exec, here we just make sure
 * SQLITE_BUSY gets returned soon, rather than blocking in
 * sqlite3_step() for a long time. */
static int
_db_busy_handler (void *dummy1, int busy_loop_count)
{
  if (busy_loop_count < 10)
    {
      sched_yield ();
      return 1;
    }
  else
    {
      return 0;
    }
}

const gchar **
rtcom_el_db_schema_get_sql ()
{
  return db_schema_sql;
}

void
rtcom_el_db_schema_get_mappings (const gchar **out_selection,
    GHashTable **out_mapping, GHashTable **out_typing)
{
  static GHashTable *mapping = NULL;
  static GHashTable *typing = NULL;
  static gchar *selection = NULL;

  if (G_UNLIKELY (mapping == NULL))
    {
      GPtrArray *sel = g_ptr_array_sized_new (sizeof(fields) / sizeof(EventField));
      gint i;

      g_assert (selection == NULL);
      g_assert (typing == NULL);

      mapping = g_hash_table_new (g_str_hash, g_str_equal);
      typing = g_hash_table_new (g_str_hash, g_str_equal);

      for (i = 0; fields[i].name != NULL; i++)
        {
          g_ptr_array_add (sel, fields[i].column);
          g_hash_table_insert (mapping, fields[i].name, fields[i].column);
          g_hash_table_insert (typing, fields[i].name, 
              GUINT_TO_POINTER (fields[i].type));
        }

      g_ptr_array_add (sel, UNIQUE_REMOTE);
      g_ptr_array_add (sel, NULL);
      selection = g_strjoinv (", ", (gchar **) sel->pdata);
      g_ptr_array_free (sel, TRUE);
  }

  if (out_mapping != NULL)
      *out_mapping = mapping;

  if (out_typing != NULL)
      *out_typing = typing;

  if (out_selection != NULL)
      *out_selection = selection;
}

#ifdef SQL_TRACING
static void
trace_cb (void *dummy, const char *sql)
{
  g_debug ("[run]: %s", sql);
}

static void
profile_cb (void *dummy, const char *sql, sqlite3_uint64 runtime)
{
  g_debug ("[finished]: (%lluus) %s", (unsigned long long) runtime, sql);
}
#endif /* SQL_TRACING */


static rtcom_el_db_t
_internal_open (const gchar *fname, gboolean try_repairing);

static rtcom_el_db_t
_handle_corrupted (const gchar *fname, gboolean try_repairing,
    GError *err_to_clear)
{
    if (err_to_clear != NULL);
        g_error_free (err_to_clear);

    if (try_repairing)
      {
        g_warning ("%s: repairing corrupted database", G_STRFUNC);
        if (!g_unlink (fname))
            return _internal_open (fname, FALSE);
      }
    else
      {
        g_warning ("%s: deleting corrupted database", G_STRFUNC);
        g_unlink (fname);
      }

    return NULL;
}

static gboolean
_internal_convert_v0 (rtcom_el_db_t db)
{
  gint plugin_id = 0;
  gint service_id = 0;
  int i;
  const gchar *services[] = { "CHAT", "CALL", "SMS", NULL };
  struct {
      const gchar *name;
      const gchar *desc;
  } eventtypes[] = {
      { "RTCOM_EL_EVENTTYPE_CHAT_NOTICE", "Notice" },
      { "RTCOM_EL_EVENTTYPE_CHAT_ACTION", "Action message" },
      { "RTCOM_EL_EVENTTYPE_CHAT_AUTOREPLY", "Autoreply message" },
      { "RTCOM_EL_EVENTTYPE_CHAT_JOIN", "Group chat joined" },
      { "RTCOM_EL_EVENTTYPE_CHAT_LEAVE", "Group chat leave" },
      { "RTCOM_EL_EVENTTYPE_CHAT_TOPIC", "Group chat topic change" },
      { NULL, NULL }
  };
  struct {
      const gchar *name;
      gint value;
      const gchar *desc;
  } flags[] = {
      { "RTCOM_EL_FLAG_CHAT_GROUP", 1, "Groupchat message" },
      { "RTCOM_EL_FLAG_CHAT_ROOM", 2, "Groupchat is room with channel_id" },
      { "RTCOM_EL_FLAG_CHAT_OPAQUE", 4, "Channel identifier is opaque" },
      { "RTCOM_EL_FLAG_CHAT_OFFLINE", 8, "Offline message" },
      { NULL, 0, NULL }
  };

#define runsql(x) rtcom_el_db_exec (db, NULL, NULL, (x), NULL)
#define vrunsql(x...) rtcom_el_db_exec_printf (db, NULL, NULL, NULL, x)

  /* This is done in transaction started by the caller, but we will
   * commit/rollback the transaction ourselves at the end. */

  /* Add two new columns to Events table. */
  if (!runsql ("ALTER TABLE Events ADD COLUMN outgoing BOOL DEFAULT 0;"))
    goto err;

  if (!runsql ("ALTER TABLE Events ADD COLUMN mc_profile BOOL DEFAULT 0;"))
    goto err;

  /* Set outgoing column for any outbound events. */
  if (!runsql ("UPDATE Events SET outgoing = 1 WHERE event_type_id IN "
      "(SELECT id FROM EventTypes WHERE name LIKE '%_OUTBOUND');"))
    goto err;

  /* For each recognised service, map all events to a single event
   * type (*_INBOUND), rename the event type to the new name and
   * delete the other now obsolete, event type (_OUTBOUND). */
  for (i = 0; services[i] != NULL; i++)
    {
      gint id = 0;
      gint old_id = 0;

      if (!rtcom_el_db_exec_printf (db, rtcom_el_db_single_int, &id, NULL,
        "SELECT id FROM EventTypes WHERE name = "
          "'RTCOM_EL_EVENTTYPE_%s_INBOUND'", services[i]))
        goto err;

      if (!id)
        goto err;

      if (!rtcom_el_db_exec_printf (db, rtcom_el_db_single_int, &old_id, NULL,
        "SELECT id FROM EventTypes WHERE name = "
          "'RTCOM_EL_EVENTTYPE_%s_OUTBOUND'", services[i]))
        goto err;

      if (!old_id)
        goto err;

      if (!vrunsql ("UPDATE Events SET event_type_id = %d WHERE "
          "event_type_id = %d", id, old_id))
        goto err;

      if (!vrunsql ("UPDATE EventTypes SET name = 'RTCOM_EL_EVENTTYPE_%s%s' "
          "WHERE name = 'RTCOM_EL_EVENTTYPE_%s_INBOUND'", services[i],
          (g_strcmp0 (services[i], "CALL")) ? "_MESSAGE" : "", services[i]))
        goto err;

      if (!vrunsql ("DELETE FROM EventTypes WHERE name = "
        "'RTCOM_EL_EVENTTYPE_%s_OUTBOUND'", services[i]));
    }

  if (!rtcom_el_db_exec (db, rtcom_el_db_single_int, &plugin_id,
      "SELECT plugin_id FROM EventTypes WHERE name = "
          "'RTCOM_EL_EVENTTYPE_CHAT_MESSAGE' LIMIT 1", NULL))
    goto err;

  if (!plugin_id)
    goto err;

  /* Add new CHAT event types */
  for (i = 0; eventtypes[i].name != NULL; i++)
    {
      if (!vrunsql ("INSERT INTO EventTypes (name, plugin_id, desc) VALUES "
          "(%Q, %d, %Q)", eventtypes[i].name, plugin_id, eventtypes[i].desc))
        goto err;
    }

  if (!rtcom_el_db_exec_printf (db, rtcom_el_db_single_int, &service_id, NULL,
      "SELECT id FROM Services WHERE plugin_id = %d", plugin_id))
    goto err;

  if (!service_id)
    goto err;

  /* Add new CHAT flags */
  for (i = 0; flags[i].name != NULL; i++)
    {
      if (!vrunsql ("INSERT INTO Flags (service_id, name, value, desc) VALUES "
          "(%d, %Q, %d, %Q)", service_id, flags[i].name, flags[i].value,
          flags[i].desc))
        goto err;
    }

  /* Now, if we've updated S3 db, we have to clean up some old triggers; this
   * is copy/paste from the db initialisation. */

  runsql ("DROP TRIGGER IF EXISTS gc_update_ev_add2;");
  runsql ("DROP TRIGGER IF EXISTS gc_update_ev_add3;");
  runsql ("DROP TRIGGER IF EXISTS gc_update_ev_update;");

  runsql ("CREATE TRIGGER IF NOT EXISTS gc_update_ev_add4 AFTER INSERT ON Events " \
       "FOR EACH ROW WHEN NEW.group_uid IS NOT NULL BEGIN " \
           "UPDATE GroupCache SET event_id = NEW.id, total_events = total_events + 1, " \
           "read_events = read_events + NEW.is_read, flags = flags | NEW.flags " \
           "WHERE group_uid = NEW.group_uid; " \
       "END;");

  runsql ("CREATE TRIGGER IF NOT EXISTS gc_update_ev_update AFTER UPDATE ON Events " \
       "FOR EACH ROW WHEN NEW.group_uid IS NOT NULL BEGIN " \
           "UPDATE GroupCache SET " \
               "read_events = read_events - OLD.is_read + NEW.is_read, "\
               "flags = (flags & (~OLD.flags)) | NEW.flags " \
               "WHERE group_uid = NEW.group_uid; " \
        "END;");

  /* We have already set user_version to 1, but the backup process
   * overwrote that, so we have to do it again. */
  if (!runsql ("PRAGMA user_version = 1"))
    goto err;

  if (!rtcom_el_db_commit (db, NULL))
    goto err;

  return TRUE;

err:
  g_warning("%s: sqlite error: %s", G_STRFUNC,
      sqlite3_errmsg (db));
  rtcom_el_db_rollback (db, NULL);
  return FALSE;

#undef runsql
#undef vrunsql
}

gboolean
rtcom_el_db_convert_from_db0 (const gchar *fname, const gchar *old_fname)
{
  rtcom_el_db_t db = NULL;
  rtcom_el_db_t old_db = NULL;
  sqlite3_backup *bkp;
  gint ret;
  int cnt;
  gboolean success;
  gchar *temp_fname;
  GError *err = NULL;

  /* To speed things up in most likely case (new database already exists),
   * we first check whether the new database file already exists. */
  if (g_access (fname, 0) == 0)
      return TRUE;

  /* If we can't reach the old database, there's nothing for us to do. */
  if (g_access (old_fname, 0) == -1)
      return TRUE;

  ret = sqlite3_open (old_fname, &old_db);

  /* Can't even open old db (no way of telling whether it's corrupted or
   * not as of yet), so just ignore it. */
  if (ret != SQLITE_OK)
      return TRUE;

  temp_fname = g_strconcat (fname, ".temp", NULL);

  ret = sqlite3_open (temp_fname, &db);
  if (ret != SQLITE_OK)
    {
      sqlite3_close (old_db);
      g_free (temp_fname);
      return FALSE;
    }

  /* We're doing the conversion to a temporary db, so it doesn't
   * matter if we get interrupted. But we want to be done as quickly
   * as possible to avoid it (and make a successfull conversion
   * in the process).
   * These settings are active only during the conversion, since the
   * database gets closed and reopened afterwards. */
  rtcom_el_db_exec (db, NULL, NULL, "PRAGMA journal_mode = OFF;", NULL);

  /* We try to get an exclusive lock on the new database. If we fail,
   * due to db/table being locked, this means another process is
   * doing the conversion. */
  if (!rtcom_el_db_transaction (db, TRUE, &err))
    {
      sqlite3_close (old_db);
      sqlite3_close (db);

      /* If the failure is due to db format error, this means another
       * process was killed while database was being created, we may
       * safely overwrite it. */
      if ((err->code == SQLITE_CORRUPT) ||
          (err->code == SQLITE_FORMAT) ||
          (err->code == SQLITE_NOTADB) ||
          (err->code == SQLITE_ABORT) ||
          (err->code == SQLITE_INTERRUPT))
        {
          g_warning ("%s: temporary db corrupted, redoing upgrade", G_STRFUNC);
          g_unlink (temp_fname);
          g_free (temp_fname);
          g_error_free (err);

          /* with corrupted temp file out of the way, retry */
          return rtcom_el_db_convert_from_db0 (fname, old_fname);
        }
      else
        {
          /* we better wait until some other process converts it */
          g_warning ("%s: database upgrade in progress, will wait", G_STRFUNC);
          g_free (temp_fname);
          g_error_free (err);
          return FALSE;
        }
    }

  /* Start backing up old database into a new one (with the same format). */
  bkp = sqlite3_backup_init (db, "main", old_db, "main");
  if (!bkp)
    {
      sqlite3_close (old_db);
      sqlite3_close (db);
      g_unlink (temp_fname);
      g_free (temp_fname);
      return FALSE;
    }

  /* Try doing the backup, but don't retry forever if a source database is
   * locked. */
  for (cnt = 0; cnt < 100; cnt++)
    {
      ret = sqlite3_backup_step (bkp, -1);

      if ((ret != SQLITE_BUSY) && (ret != SQLITE_LOCKED))
          break;

      g_usleep (G_USEC_PER_SEC / 100);
    }

  ret = sqlite3_backup_finish (bkp);

  /* We couldn't make it. Better remove the corrupted file, too. */
  if (ret != SQLITE_OK)
    {
      sqlite3_close (old_db);
      sqlite3_close (db);
      g_unlink (temp_fname);
      g_free (temp_fname);
      return FALSE;
    }

  sqlite3_close (old_db);

  /* Now we can do the conversion. */
  success = _internal_convert_v0 (db);

  sqlite3_close (db);

  /* If everything went OK, the database was successfully converted.
   * Let's move it to correct filename. Otherwise, remove the
   * corrupted conversion file and give up. */
  if (success)
    {
      g_rename (temp_fname, fname);
    }
  else
    {
      g_unlink (temp_fname);
    }

  g_free (temp_fname);
  return success;
}


/* Opens a new SQLite3 database, creating and initialising it if
 * neccessary. If the existing database is corrupted, it's deleted
 * and a new database is created.
 */
static rtcom_el_db_t
_internal_open (const gchar *fname, gboolean try_repairing)
{
  rtcom_el_db_t db = NULL;
  gint user_version = 0;
  gint ret;
  GError *err = NULL;
  const gchar **db_schema = rtcom_el_db_schema_get_sql ();

  ret = sqlite3_open (fname, &db);

  if (ret != SQLITE_OK)
    {
      g_warning ("%s: can't open SQLite3 db: %s",
          G_STRFUNC, fname);

      if (db != NULL)
          sqlite3_close (db);

      if ((ret == SQLITE_CORRUPT) || (ret == SQLITE_FORMAT) ||
          (ret == SQLITE_NOTADB))
        {
          return _handle_corrupted (fname, try_repairing, NULL);
        }
      return NULL;
    }

  sqlite3_busy_handler (db, _db_busy_handler, NULL);

#ifdef SQL_TRACING
  sqlite3_trace (db, trace_cb, NULL);
  sqlite3_profile (db, profile_cb, NULL);
#endif

  /* Quick check to see if the database is valid. */
  if (!rtcom_el_db_exec (db, NULL, NULL, "PRAGMA quick_check;", &err))
    {
      sqlite3_close (db);

      if (err->code == RTCOM_EL_DATABASE_CORRUPTED)
        {
          return _handle_corrupted (fname, try_repairing, err);
        }
      else
        {
          g_error_free (err);
          return NULL;
        }
    }

  rtcom_el_db_exec (db, rtcom_el_db_single_int, &user_version,
      "PRAGMA user_version;", NULL);

  /* If schema hasn't been defined, we can attempt to do so. Race condition
     here is mostly harmless because CREATEs are guarded by IF NOT EXIST. But,
     we're doing the detection to avoid slow startup and messing with other
     app' queries that might be happening at the time. */
  if (user_version < REQUIRED_USER_VERSION)
    {
      /* If schema hasn't been defined, we can attempt to do so. Race condition
       * here is harmless because CREATEs are guarded by IF NOT EXIST. But,
       * we're doing the detection to avoid slow startup. */
      gint i;

      g_chmod (fname, S_IRUSR | S_IWUSR);

      /* If we fail here and database is corrupted or schema is not properly
       * created, it will be recreated next time anyways. So we don't really
       * need the journal. */
      rtcom_el_db_exec (db, NULL, NULL, "PRAGMA journal_mode = MEMORY;", NULL);

      if (!rtcom_el_db_transaction (db, TRUE, &err))
        {
          /* If db is in use, that means schema is already
           * installed, or is being installed right now.
           * So, nothing more to do. */
          if (err->code == RTCOM_EL_TEMPORARY_ERROR)
            {
              g_error_free (err);
              goto db_schema_ready;
            }

          g_warning ("%s: can't initialise db schema", G_STRFUNC);
          sqlite3_close (db);

          if (err->code == RTCOM_EL_DATABASE_CORRUPTED)
            {
              return _handle_corrupted (fname, try_repairing, err);
            }

          g_error_free (err);
          return NULL;
        }

      for (i = 0; db_schema[i]; i++)
        {
          if (!rtcom_el_db_exec (db, NULL, NULL, db_schema[i], &err))
            {
              if (err->code == RTCOM_EL_TEMPORARY_ERROR)
                {
                  g_error_free (err);
                  goto db_schema_ready;
                }

              g_warning ("%s: can't initialise db schema", G_STRFUNC);
              sqlite3_close (db);

              if (err->code == RTCOM_EL_DATABASE_CORRUPTED)
                {
                  return _handle_corrupted (fname, try_repairing, err);
                }

              g_error_free (err);
              return NULL;
            }
        }

      /* We have exclusive lock so it's not a temporary error. */
      if (!rtcom_el_db_commit (db, NULL))
        {
          g_warning ("%s: can't initialise db schema", G_STRFUNC);
          sqlite3_close (db);
          return NULL;
        }
    }

db_schema_ready:

  rtcom_el_db_exec (db, NULL, NULL, "PRAGMA journal_mode = TRUNCATE;", NULL);
  rtcom_el_db_exec (db, NULL, NULL, "PRAGMA synchronous = OFF;", NULL);
  return db;
}

/* Public wrapper for the opener function, with enabled db
 * reparation if needed. */
rtcom_el_db_t
rtcom_el_db_open (const gchar *fname)
{
  return _internal_open (fname, TRUE);
}

void
rtcom_el_db_close (rtcom_el_db_t db)
{
  g_assert (db);
  sqlite3_close (db);
}

/* Fetches a single integer value from a row of data */
void
rtcom_el_db_single_int (gpointer data, gpointer user_data)
{
  sqlite3_stmt *stmt = data;
  gint *val = user_data;

  g_assert (stmt);
  g_assert (val);

  *val = sqlite3_column_int (stmt, 0);
}

/* Do one iteration of SQLite statement, possibly stepping it
 * multiple times if the database is busy/locked. Guard against
 * infite looping on deadlocks. */
gint
rtcom_el_db_iterate (rtcom_el_db_t db, rtcom_el_db_stmt_t stmt,
    GError **error)
{
  int ret = SQLITE_OK;
  GTimer *timer = g_timer_new ();

  while (1)
    {
      ret = sqlite3_step (stmt);

      if ((ret != SQLITE_BUSY) && (ret != SQLITE_LOCKED))
          break;

      if (g_timer_elapsed (timer, NULL) > RTCOM_EL_DB_MAX_BUSYLOOP_TIME)
      {
          g_debug ("%s: database locked while executing: %s", G_STRFUNC,
              sqlite3_sql (stmt));
          g_set_error (error, RTCOM_EL_ERROR, RTCOM_EL_TEMPORARY_ERROR,
             "Database locked");
          g_timer_reset (timer);
          return SQLITE_BUSY;
      }

      sched_yield();
    }

  g_timer_destroy (timer);

  switch (ret)
   {
      case SQLITE_DONE:
      case SQLITE_ROW:
      case SQLITE_OK:
          /* All is well. */
          break;

      case SQLITE_FULL:
      case SQLITE_IOERR:
          g_debug ("%s: database full or I/O error", G_STRFUNC);
          g_set_error (error, RTCOM_EL_ERROR, RTCOM_EL_DATABASE_FULL,
             "Database full");
          break;

      case SQLITE_CORRUPT:
      case SQLITE_FORMAT:
      case SQLITE_NOTADB:
          g_debug ("%s: database corrupted", G_STRFUNC);
          g_set_error (error, RTCOM_EL_ERROR, RTCOM_EL_DATABASE_CORRUPTED,
             "Database corrupted");
          break;

      default:
          g_debug ("%s: runtime error while executing \"%s\": %s", G_STRFUNC,
              sqlite3_sql (stmt), sqlite3_errmsg (db));
          g_set_error (error, RTCOM_EL_ERROR, RTCOM_EL_INTERNAL_ERROR,
             "SQL error: %s", sqlite3_errmsg (db));
    }

  return ret;
}


/* Executes an SQL statement, and optionally calls
 * the callback for every row of the result. Returns TRUE
 * if statement was successfully executed, FALSE on error. */
gboolean
rtcom_el_db_exec (rtcom_el_db_t db, GFunc cb, gpointer user_data,
    const gchar *sql, GError **error)
{
  rtcom_el_db_stmt_t stmt;
  int ret;

  g_assert (db);
  g_assert (sql);

  ret = sqlite3_prepare_v2 (db, sql, -1, &stmt, NULL);
  switch (ret)
  {
      case SQLITE_DONE:
      case SQLITE_ROW:
      case SQLITE_OK:
          /* All is well. */
          break;

      case SQLITE_FULL:
      case SQLITE_IOERR:
          g_debug ("%s: database full or I/O error", G_STRFUNC);
          g_set_error (error, RTCOM_EL_ERROR, RTCOM_EL_DATABASE_FULL,
             "Database full");
          return FALSE;

      case SQLITE_CORRUPT:
      case SQLITE_FORMAT:
      case SQLITE_NOTADB:
          g_debug ("%s: database corrupted", G_STRFUNC);
          g_set_error (error, RTCOM_EL_ERROR, RTCOM_EL_DATABASE_CORRUPTED,
             "Database corrupted");
          return FALSE;

      default:
          g_warning ("%s: can't compile SQL statement \"%s\": %s", G_STRFUNC, sql,
             sqlite3_errmsg (db));
          g_set_error (error, RTCOM_EL_ERROR, RTCOM_EL_INTERNAL_ERROR,
             "Can't compile SQL statement: %s", sqlite3_errmsg (db));
          return FALSE;
    }

  do
    {
      ret = rtcom_el_db_iterate (db, stmt, error);

      if (ret == SQLITE_ROW)
          if (cb != NULL)
              cb ((gpointer) stmt, user_data);
    }
  while (ret == SQLITE_ROW);

  sqlite3_finalize (stmt);

  if ((ret == SQLITE_DONE) || (ret == SQLITE_OK))
      return TRUE;
  else
      return FALSE;
}

/* Builds a SQL statement from a format string with support for
 * safe %q and %Q string value quoting, and executes the statement. */
gboolean
rtcom_el_db_exec_printf (rtcom_el_db_t db, GFunc cb, gpointer user_data,
    GError **error, const gchar *fmt, ...)
{
  va_list ap;
  char *sql;
  gboolean ret;

  g_assert (db);
  g_assert (fmt);

  va_start (ap, fmt);
  sql = sqlite3_vmprintf (fmt, ap);
  va_end (ap);

  if (sql == NULL)
    {
      g_debug ("%s: can't prepare SQL statement", G_STRFUNC);
      g_set_error (error, RTCOM_EL_ERROR, RTCOM_EL_INTERNAL_ERROR,
         "Can't prepare SQL statement");
      return FALSE;
    }

  ret = rtcom_el_db_exec (db, cb, user_data, sql, error);

  sqlite3_free (sql);
  return ret;
}

/* Starts a new transaction. SQLite doesn't return error for nested
 * BEGINs, so we guard against it manually. Note that this is
 * not threadsafe. */
gboolean
rtcom_el_db_transaction (rtcom_el_db_t db, gboolean exclusive,
    GError **error)
{
  gboolean ret;

  g_assert (db);

  /* If autocommit is not enabled, we're inside a transaction already. */
  if (!sqlite3_get_autocommit (db))
    {
      g_warning ("%s: refusing to start nested transaction", G_STRFUNC);
      return FALSE;
    }

  if (exclusive)
      ret = rtcom_el_db_exec (db, NULL, NULL, "BEGIN EXCLUSIVE;", error);
  else
      ret = rtcom_el_db_exec (db, NULL, NULL, "BEGIN DEFERRED;", error);

  return ret;
}

gboolean
rtcom_el_db_commit (rtcom_el_db_t db, GError **error)
{
  g_assert (db);

  /* Check that we're really inside a transaction. */
  if (sqlite3_get_autocommit (db))
    {
      g_warning ("%s: called outside of transaction", G_STRFUNC);
      return FALSE;
    }

  return rtcom_el_db_exec (db, NULL, NULL, "COMMIT;", error);
}

gboolean
rtcom_el_db_rollback (rtcom_el_db_t db, GError **error)
{
  g_assert (db);

  /* Check that we're really inside a transaction. */
  if (sqlite3_get_autocommit (db))
    {
      g_warning ("%s: called outside of transaction", G_STRFUNC);
      return FALSE;
    }

  return rtcom_el_db_exec (db, NULL, NULL, "ROLLBACK;", error);
}

static void
_map_name_id_slave (rtcom_el_db_stmt_t stmt, GHashTable *table)
{
  g_hash_table_insert(table,
      g_strdup((gchar *) sqlite3_column_text(stmt, 1)),
      GINT_TO_POINTER (sqlite3_column_int(stmt, 0)));
}

GHashTable *
rtcom_el_db_cache_lookup_table (rtcom_el_db_t db, const gchar *tname)
{
  GHashTable *t;

  g_assert (db);
  g_assert (tname);

  t = g_hash_table_new_full (g_str_hash, g_str_equal, g_free,
      NULL);

  if (!rtcom_el_db_exec_printf (db, (GFunc) _map_name_id_slave, t, NULL,
      "SELECT id, name FROM %s;", tname))
    {
      g_hash_table_destroy (t);
      return NULL;
    }

  return t;
}

void
rtcom_el_db_g_value_slice_free (gpointer p)
{
  GValue *v = p;

  if (G_VALUE_TYPE (v) != G_TYPE_INVALID)
      g_value_unset (v);

  g_slice_free (GValue, v);
}

GHashTable *
rtcom_el_db_schema_get_row (rtcom_el_db_stmt_t stmt)
{
  gint i;
  GHashTable *row = g_hash_table_new_full (g_str_hash, g_str_equal, NULL,
      rtcom_el_db_g_value_slice_free);

  for (i = 0; fields[i].name != NULL; i++)
    {
      GValue *val = g_slice_new0 (GValue);
      g_hash_table_insert (row, fields[i].name, val);
    }

  rtcom_el_db_schema_update_row (stmt, row);
  return row;
}

void
rtcom_el_db_schema_update_row (rtcom_el_db_stmt_t stmt,
    GHashTable *row)
{
  gint i;

  g_assert (stmt);
  g_assert (row);

  for (i = 0; fields[i].name != NULL; i++)
    {
      GValue *val = g_hash_table_lookup (row, fields[i].name);
      if (G_VALUE_TYPE (val) != G_TYPE_INVALID)
          g_value_unset (val);

      g_value_init (val, fields[i].type);

      switch (fields[i].type)
        {
          case G_TYPE_INT:
              g_value_set_int (val, sqlite3_column_int (stmt, i));
              break;
          case G_TYPE_BOOLEAN:
              g_value_set_boolean (val, 0 != sqlite3_column_int (stmt, i));
              break;
          case G_TYPE_STRING:
              g_value_set_string (val,
                  (const gchar *) sqlite3_column_text (stmt, i));
              break;

          default:
              /* If we got here, that means get_schema() is buggy. */
              g_assert_not_reached ();
        }
    }
}

