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
 * @example check_el.c
 * Shows how to initialize the framework.
 */

#include "rtcom-eventlogger/eventlogger.h"

#include <glib.h>
#include <glib/gstdio.h>

#include <check.h>

#include <sqlite3.h>

#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include "canned-data.h"
#include "fail.h"

#define SERVICE "RTCOM_EL_SERVICE_TEST"
#define EVENT_TYPE "RTCOM_EL_EVENTTYPE_TEST_ET1"
#define FLAGS 0
#define BYTES_SENT 10
#define BYTES_RECEIVED 9
#define REMOTE_EBOOK_UID "ebook-uid-1"
#define LOCAL_UID "ext-salvatore.iovene@nokia.com"
#define LOCAL_NAME "Salvatore Iovene"

/* FIXME: OMG, syntax! */
#define REMOTE_UID "1@foo.org"
#define REMOTE_NAME "1,2"

#define CHANNEL "chavo"
#define FREE_TEXT "Test free_text"

#define HEADER_KEY "Foo"
#define HEADER_VAL "Bar"

#define ATTACH_DESC "Foo attachment."

#define REMOTE_EBOOK_UID_2 "ebook-uid-2"
#define REMOTE_UID_2 "ext-salvatore.iovene-2@nokia.com"
#define REMOTE_NAME_2 "Salvatore Iovene 2"

#define REMOTE_EBOOK_UID_3 "ebook-uid-3"
#define REMOTE_UID_3 "ext-salvatore.iovene-3@nokia.com"
#define REMOTE_NAME_3 "Salvatore Iovene 3"

static RTComEl *el = NULL;

static RTComElEvent *
event_new_lite (void)
{
    RTComElEvent *ev;

    ev = rtcom_el_event_new();
    g_return_val_if_fail (ev != NULL, NULL);

    RTCOM_EL_EVENT_SET_FIELD(ev, service, g_strdup (SERVICE));
    RTCOM_EL_EVENT_SET_FIELD(ev, event_type, g_strdup (EVENT_TYPE));
    RTCOM_EL_EVENT_SET_FIELD(ev, local_uid, g_strdup (LOCAL_UID));
    RTCOM_EL_EVENT_SET_FIELD(ev, start_time, time(NULL));

    return ev;
}

static RTComElEvent *
event_new_full (time_t t)
{
    RTComElEvent *ev;

    ev = rtcom_el_event_new();
    g_return_val_if_fail (ev != NULL, NULL);

    /* Setting everything here for testing purposes, but usually
     * you wouldn't need to. */
    /* FIXME: RTComElEvent structure:
     * 1) OMG, it's full of string IDs that want to be quarks or enums;
     * 2) it's painful to care about string member ownership.
     */
    RTCOM_EL_EVENT_SET_FIELD(ev, service, g_strdup (SERVICE));
    RTCOM_EL_EVENT_SET_FIELD(ev, event_type, g_strdup (EVENT_TYPE));
    RTCOM_EL_EVENT_SET_FIELD(ev, start_time, t);
    RTCOM_EL_EVENT_SET_FIELD(ev, end_time, t);
    RTCOM_EL_EVENT_SET_FIELD(ev, flags, FLAGS);
    RTCOM_EL_EVENT_SET_FIELD(ev, bytes_sent, BYTES_SENT);
    RTCOM_EL_EVENT_SET_FIELD(ev, bytes_received, BYTES_RECEIVED);
    RTCOM_EL_EVENT_SET_FIELD(ev, local_uid, g_strdup (LOCAL_UID));
    RTCOM_EL_EVENT_SET_FIELD(ev, local_name, g_strdup (LOCAL_NAME));
    RTCOM_EL_EVENT_SET_FIELD(ev, remote_uid, g_strdup (REMOTE_UID));
    RTCOM_EL_EVENT_SET_FIELD(ev, remote_name, g_strdup (REMOTE_NAME));
    RTCOM_EL_EVENT_SET_FIELD(ev, channel, g_strdup (CHANNEL));
    RTCOM_EL_EVENT_SET_FIELD(ev, free_text, g_strdup (FREE_TEXT));

    return ev;
}

static void
core_setup (void)
{
#if !GLIB_CHECK_VERSION(2,35,0)
    g_type_init ();
#endif
    const gchar *home;
    gchar *fn;

    home = g_getenv ("RTCOM_EL_HOME");
    if (!home)
        home = g_get_home_dir();

    fn = g_build_filename (home, ".rtcom-eventlogger", "el-v1.db", NULL);
    g_unlink (fn);
    g_free (fn);

    /* Make a new EL instance for the tests (this will repopulate the DB) */
    el = rtcom_el_new ();
    g_assert (el != NULL);

    /* Push in some canned data (the service-specific plugins are able to
     * provide more realistic data) */
    add_canned_events (el);
}

static void
core_teardown (void)
{
    /* Leave the tables in place, so el.db can be inspected */
    g_assert (el != NULL);
    g_object_unref (el);
    el = NULL;
}

static gint
iter_count_results (RTComElIter *it)
{
    gint i = 1;

    if (it == NULL)
    {
        return 0;
    }

    if (!rtcom_el_iter_first (it))
    {
        return 0;
    }

    while (rtcom_el_iter_next (it))
    {
        i += 1;
    }

    return i;
}

START_TEST(test_add_event)
{
    RTComElQuery * query = NULL;
    RTComElEvent * ev = NULL;
    gint event_id = -1;
    gint service_id = -1;
    gint event_type_id = -1;
    RTComElIter * it = NULL;
    GHashTable * values = NULL;
    time_t t = 0;

    t = time (NULL);

    ev = event_new_full (t);
    if(!ev)
    {
        fail("Failed to create event.");
    }

    event_id = rtcom_el_add_event(el, ev, NULL);
    fail_if (event_id < 0, "Failed to add event");

    query = rtcom_el_query_new(el);

    /* exercise its GObject properties */
    {
        RTComEl *q_el = NULL;
        gboolean q_is_caching = 0xDEADBEEF;
        gint q_limit = -42;
        gint q_offset = -42;
        gint q_group_by = -42;

        g_object_get (query,
                "el", &q_el,
                "is-caching", &q_is_caching,
                "limit", &q_limit,
                "offset", &q_offset,
                "group-by", &q_group_by,
                NULL);

        fail_unless (q_el == el);
        fail_unless (q_is_caching == FALSE);
        fail_unless (q_limit == -1);
        fail_unless (q_offset == 0);
        fail_unless (q_group_by == RTCOM_EL_QUERY_GROUP_BY_NONE);
    }

    if(!rtcom_el_query_prepare(
                query,
                "id", event_id, RTCOM_EL_OP_EQUAL,
                NULL))
    {
        fail("Failed to prepare the query.");
    }

    it = rtcom_el_get_events(el, query);
    g_object_unref(query);

    fail_unless(it != NULL, "Failed to get iterator");

    fail_unless(rtcom_el_iter_get_values(it, "service-id",
        &service_id, "event-type-id", &event_type_id, NULL));

    fail_unless (rtcom_el_get_service_id(el, SERVICE) == service_id);
    fail_unless (rtcom_el_get_eventtype_id(el, EVENT_TYPE) == event_type_id);

    /* exercise its GObject properties */
    {
        gpointer el_db = NULL;
        RTComEl *it_el = NULL;
        gpointer it_query = NULL;
        gpointer it_db = NULL;
        gpointer it_stmt = NULL;
        gchar *it_sql = NULL;
        GHashTable *it_plugins = NULL;
        gboolean it_atomic = 0xDEADBEEF;

        g_object_get (el,
                "db", &el_db,
                NULL);

        g_object_get (it,
                "el", &it_el,
                "query", &it_query,
                "sqlite3-database", &it_db,
                "sqlite3-statement", &it_stmt,
                "plugins-table", &it_plugins,
                "atomic", &it_atomic,
                NULL);

        fail_unless (it_el == el);
        fail_unless (it_query != NULL);
        fail_unless (it_db != NULL);
        fail_unless (it_db == el_db);
        fail_unless (it_stmt != NULL);
        fail_unless (it_plugins != NULL);
        fail_unless (it_atomic == TRUE || it_atomic == FALSE);

        g_free (it_sql);
    }

    fail_unless(rtcom_el_iter_first(it), "Failed to start iterator");

    values = rtcom_el_iter_get_value_map(
            it,
            "start-time",
            "end-time",
            "flags",
            "bytes-sent",
            "bytes-received",
            "local-uid",
            "local-name",
            "remote-uid",
            "remote-name",
            "channel",
            "free-text",
            NULL);

    if(!values)
    {
        fail("Failed to get values.");
    }

    rtcom_fail_unless_intcmp(t, ==,
            g_value_get_int(g_hash_table_lookup(values, "start-time")));
    rtcom_fail_unless_intcmp(t, ==,
            g_value_get_int(g_hash_table_lookup(values, "end-time")));

    rtcom_fail_unless_intcmp(FLAGS, ==,
            g_value_get_int(g_hash_table_lookup(values, "flags")));

    rtcom_fail_unless_intcmp(BYTES_SENT, ==,
            g_value_get_int(g_hash_table_lookup(values, "bytes-sent")));
    rtcom_fail_unless_intcmp(BYTES_RECEIVED, ==,
            g_value_get_int(g_hash_table_lookup(values, "bytes-received")));
    rtcom_fail_unless_strcmp(LOCAL_UID, ==,
            g_value_get_string(g_hash_table_lookup(values, "local-uid")));
    rtcom_fail_unless_strcmp(LOCAL_NAME, ==,
            g_value_get_string(g_hash_table_lookup(values, "local-name")));
    rtcom_fail_unless_strcmp(REMOTE_UID, ==,
            g_value_get_string(g_hash_table_lookup(values, "remote-uid")));
    rtcom_fail_unless_strcmp(REMOTE_NAME, ==,
            g_value_get_string(g_hash_table_lookup(values, "remote-name")));
    rtcom_fail_unless_strcmp(CHANNEL, ==,
            g_value_get_string(g_hash_table_lookup(values, "channel")));
    rtcom_fail_unless_strcmp(FREE_TEXT, ==,
            g_value_get_string(g_hash_table_lookup(values, "free-text")));

    fail_if(rtcom_el_iter_next(it), "Iterator should only return one row");

    g_hash_table_destroy(values);
    g_object_unref(it);
    rtcom_el_event_free_contents (ev);
    rtcom_el_event_free (ev);
}
END_TEST

START_TEST(test_add_full)
{
    const time_t time = 1000000;
    RTComElEvent *ev;
    RTComElQuery *query;
    RTComElIter *it;
    RTComElAttachIter *att_it;
    RTComElAttachment *att;
    GHashTable *headers;
    GList *attachments = NULL;
    gint fd;
    gint event_id;
    gchar *path1;
    gchar *path2;
    gchar *contents;
    gsize length;
    gchar *basename1;
    gchar *basename2;

    headers = g_hash_table_new_full (g_str_hash, g_str_equal,
            g_free, g_free);
    g_hash_table_insert (headers, g_strdup (HEADER_KEY),
            g_strdup ("add_full"));

    fd = g_file_open_tmp ("attachment1.XXXXXX", &path1, NULL);
    fail_unless (fd >= 0);
    fail_unless (path1 != NULL);
    close (fd);
    fail_unless (g_file_set_contents (path1, "some text\n", -1, NULL));

    fd = g_file_open_tmp ("attachment2.XXXXXX", &path2, NULL);
    fail_unless (fd >= 0);
    fail_unless (path2 != NULL);
    close (fd);
    fail_unless (g_file_set_contents (path2, "other text\n", -1, NULL));

    attachments = g_list_prepend (attachments,
            rtcom_el_attachment_new (path1, "some file"));
    attachments = g_list_prepend (attachments,
            rtcom_el_attachment_new (path2, NULL));

    ev = event_new_full (time);
    fail_unless (ev != NULL, "Failed to create event.");

    event_id = rtcom_el_add_event_full (el, ev,
            headers, attachments, NULL);

    fail_if (event_id < 0, "Failed to add event");

    g_unlink (path1);
    g_unlink (path2);
    g_hash_table_destroy (headers);
    headers = NULL;
    g_list_foreach (attachments, (GFunc) rtcom_el_free_attachment, NULL);
    g_list_free (attachments);
    attachments = NULL;

    /* now iterate over the attachments */

    query = rtcom_el_query_new(el);
    fail_unless (rtcom_el_query_prepare (query,
                "id", event_id, RTCOM_EL_OP_EQUAL,
                NULL));

    it = rtcom_el_get_events(el, query);
    g_object_unref(query);

    fail_unless (it != NULL, "Failed to get iterator");
    fail_unless (rtcom_el_iter_first(it), "Failed to start iterator");

    fail_unless (rtcom_el_iter_get_values (it, HEADER_KEY, &contents, NULL));
    rtcom_fail_unless_strcmp ("add_full", ==, contents);
    g_free (contents);

    att_it = rtcom_el_iter_get_attachments(it);
    fail_unless (att_it != NULL, "Failed to get attachment iterator");

    g_object_unref (it);

    fail_unless (rtcom_el_attach_iter_first(att_it),
                 "Failed to start attachment iterator");

    att = rtcom_el_attach_iter_get (att_it);
    fail_if (att == NULL, "failed to get attachment data");

    fail_unless (event_id == att->event_id,
                 "attachment event ID doesn't match");
    rtcom_fail_unless_strcmp (basename1 = g_path_get_basename (path2), ==,
                              basename2 = g_path_get_basename (att->path));
    g_free(basename1);
    g_free(basename2);
    rtcom_fail_unless_strcmp (NULL, ==, att->desc);
    fail_unless (g_file_get_contents (att->path, &contents, &length, NULL));
    rtcom_fail_unless_uintcmp (length, ==, strlen ("other text\n"));
    rtcom_fail_unless_strcmp (contents, ==, "other text\n");
    g_free (contents);
    rtcom_el_free_attachment (att);

    fail_unless (rtcom_el_attach_iter_next (att_it),
                 "Failed to advance attachment iterator");

    att = rtcom_el_attach_iter_get (att_it);
    fail_if (att == NULL, "failed to get attachment data");

    fail_unless (event_id == att->event_id,
                 "attachment event ID doesn't match");
    rtcom_fail_unless_strcmp (basename1 = g_path_get_basename (path1), ==,
                              basename2 = g_path_get_basename (att->path));
    g_free(basename1);
    g_free(basename2);
    rtcom_fail_unless_strcmp ("some file", ==, att->desc);
    fail_unless (g_file_get_contents (att->path, &contents, &length, NULL));
    rtcom_fail_unless_uintcmp (length, ==, strlen ("some text\n"));
    rtcom_fail_unless_strcmp (contents, ==, "some text\n");
    g_free (contents);
    rtcom_el_free_attachment (att);

    fail_if (rtcom_el_attach_iter_next (att_it));

    g_object_unref (att_it);

    g_free (path1);
    g_free (path2);

    rtcom_el_event_free_contents (ev);
    rtcom_el_event_free (ev);
}
END_TEST

START_TEST(test_header)
{
    RTComElQuery * query = NULL;
    RTComElEvent * ev = NULL;
    gint event_id = -1;
    gint header_id = -1;
    RTComElIter * it = NULL;
    GHashTable *headers;
    gchar *contents;

    ev = event_new_lite ();
    if(!ev)
    {
        fail("Failed to create event.");
    }

    event_id = rtcom_el_add_event(el, ev, NULL);
    fail_if (event_id < 0, "Failed to add event");

    header_id = rtcom_el_add_header(
            el, event_id,
            HEADER_KEY,
            HEADER_VAL,
            NULL);
    fail_if (header_id < 0, "Failed to add header");

    query = rtcom_el_query_new(el);
    if(!rtcom_el_query_prepare(
                query,
                "id", event_id, RTCOM_EL_OP_EQUAL,
                NULL))
    {
        fail("Failed to prepare the query.");
    }

    it = rtcom_el_get_events(el, query);
    g_object_unref(query);

    fail_unless(it != NULL, "Failed to get iterator");
    fail_unless(rtcom_el_iter_first(it), "Failed to start iterator");

    fail_unless(rtcom_el_iter_get_values(it, HEADER_KEY, &contents, NULL));
    rtcom_fail_unless_strcmp(HEADER_VAL, ==, contents);
    g_free (contents);

    fail_if(rtcom_el_iter_next(it), "Iterator should only return one row");

    g_object_unref(it);
    rtcom_el_event_free_contents (ev);
    rtcom_el_event_free (ev);

    headers = rtcom_el_fetch_event_headers (el, event_id);

    fail_unless (headers != NULL);
    rtcom_fail_unless_intcmp (g_hash_table_size (headers), ==, 1);
    rtcom_fail_unless_strcmp (g_hash_table_lookup (headers, HEADER_KEY),
            ==, HEADER_VAL);

    g_hash_table_destroy (headers);
}
END_TEST

START_TEST(test_attach)
{
    RTComElQuery * query = NULL;
    RTComElEvent * ev = NULL;
    gint event_id = -1;
    gint attachment_id = -1;
    RTComElIter * it = NULL;
    RTComElAttachIter * att_it = NULL;
    RTComElAttachment *att = NULL;
    gchar *contents;
    gsize length;
    GError *error = NULL;
    gchar *attach_path;
    gint fd;
    gchar *basename1;
    gchar *basename2;

    ev = event_new_lite ();
    fail_if (ev == NULL, "Failed to create event");

    event_id = rtcom_el_add_event(el, ev, NULL);
    fail_if (event_id < 0, "Failed to add event");

    attachment_id = rtcom_el_add_attachment(
            el, event_id,
            "/nonexistent", ATTACH_DESC,
            &error);
    fail_if (attachment_id != -1, "Should have failed to add nonexistent "
            "attachment");
    rtcom_fail_unless_uintcmp (error->domain, ==, RTCOM_EL_ERROR);
    rtcom_fail_unless_intcmp (error->code, ==, RTCOM_EL_INTERNAL_ERROR);
    g_clear_error (&error);

    query = rtcom_el_query_new(el);
    if(!rtcom_el_query_prepare(
                query,
                "id", event_id, RTCOM_EL_OP_EQUAL,
                NULL))
    {
        fail("Failed to prepare the query.");
    }

    it = rtcom_el_get_events(el, query);
    g_object_unref(query);

    att_it = rtcom_el_iter_get_attachments(it);
    fail_unless (att_it == NULL, "Should start with no attachments");

    g_object_unref(it);

    fd = g_file_open_tmp ("attachment.XXXXXX", &attach_path, NULL);
    fail_unless (fd >= 0);
    fail_unless (attach_path != NULL);
    close (fd);
    fail_unless (g_file_set_contents (attach_path, "lalala", 6, NULL));

    attachment_id = rtcom_el_add_attachment(
            el, event_id,
            attach_path, ATTACH_DESC,
            NULL);
    fail_if (attachment_id < 0, "Failed to add attachment");

    g_unlink (attach_path);

    query = rtcom_el_query_new(el);
    if(!rtcom_el_query_prepare(
                query,
                "id", event_id, RTCOM_EL_OP_EQUAL,
                NULL))
    {
        fail("Failed to prepare the query.");
    }

    it = rtcom_el_get_events(el, query);
    g_object_unref(query);

    fail_unless (it != NULL, "Failed to get iterator");
    fail_unless (rtcom_el_iter_first(it), "Failed to start iterator");

    att_it = rtcom_el_iter_get_attachments(it);
    fail_unless (att_it != NULL, "Failed to get attachment iterator");

    /* Exercise the attachment iterator's GObject properties in a basic way */
    {
        gpointer el_db;
        gpointer ai_db;
        gpointer ai_stmt;

        g_object_get (el,
                "db", &el_db,
                NULL);

        g_object_get (att_it,
                "sqlite3-database", &ai_db,
                "sqlite3-statement", &ai_stmt,
                NULL);

        fail_unless (ai_db != NULL);
        fail_unless (ai_db == el_db);
        fail_unless (ai_stmt != NULL);
    }

    fail_unless (rtcom_el_attach_iter_first(att_it),
                 "Failed to start attachment iterator");

    att = rtcom_el_attach_iter_get (att_it);
    fail_if (att == NULL, "failed to get attachment data");

    fail_unless (event_id == att->event_id,
                 "attachment event ID doesn't match");
    rtcom_fail_unless_strcmp(basename1 = g_path_get_basename(attach_path), ==,
                             basename2 = g_path_get_basename(att->path));
    g_free(basename1);
    g_free(basename2);
    rtcom_fail_unless_strcmp(ATTACH_DESC, ==, att->desc);
    fail_unless (g_file_get_contents (att->path, &contents, &length, NULL));
    rtcom_fail_unless_uintcmp (length, ==, 6);
    rtcom_fail_unless_strcmp (contents, ==, "lalala");
    g_free (contents);

    fail_if (rtcom_el_attach_iter_next (att_it));

    g_free (attach_path);
    rtcom_el_free_attachment (att);
    g_object_unref(att_it);
    g_object_unref(it);
    rtcom_el_event_free_contents (ev);
    rtcom_el_event_free (ev);
}
END_TEST

START_TEST(test_read)
{
    RTComElQuery * query = NULL;
    RTComElEvent * ev = NULL;
    gint event_id = -1;
    RTComElIter * it = NULL;
    gint i;
    gboolean is_read;
    gint count;
    gint ids[4] = { 0, 0, 0, 0 }; /* three IDs plus 0-terminator */

    ev = event_new_full (time (NULL));
    if(!ev)
    {
        fail("Failed to create event.");
    }

    event_id = rtcom_el_add_event(el, ev, NULL);
    fail_if (event_id < 0, "Failed to add event");

    /* All events are initially unread */

    rtcom_el_set_read_event(el, event_id, TRUE, NULL);

    query = rtcom_el_query_new(el);
    rtcom_el_query_set_limit(query, 5);
    if(!rtcom_el_query_prepare(
                query,
                "is-read", TRUE, RTCOM_EL_OP_EQUAL,
                NULL))
    {
        fail("Failed to prepare the query.");
    }

    it = rtcom_el_get_events(el, query);
    g_object_unref(query);

    fail_unless(it != NULL, "Failed to get iterator");
    fail_unless(rtcom_el_iter_first(it), "Failed to start iterator");

    fail_unless(rtcom_el_iter_get_values(it, "is-read", &is_read, NULL));
    fail_unless(is_read == TRUE, "is-read flag doesn't match");

    /* At this point exactly one event has is-read = TRUE */
    count = iter_count_results (it);
    rtcom_fail_unless_intcmp (count, ==, 1);

    g_object_unref(it);

    /* Mark the three most recently added events as read (that should be
     * the one we just added, plus two more) */
    query = rtcom_el_query_new (el);
    rtcom_el_query_set_limit (query, 3);
    fail_unless (rtcom_el_query_prepare(query,
                NULL));
    it = rtcom_el_get_events(el, query);
    g_object_unref (query);

    fail_unless (it != NULL);
    fail_unless (rtcom_el_iter_first (it));
    i = 0;

    for (i = 0; i < 3; i++)
    {
        if (i > 0)
        {
            fail_unless (rtcom_el_iter_next (it));
        }

        fail_unless (rtcom_el_iter_get_values (it, "id", ids + i, NULL));
    }

    fail_if (rtcom_el_iter_next (it), "Iterator should run out after 3");

    rtcom_fail_unless_intcmp (ids[0], ==, event_id);

    g_object_unref(it);

    rtcom_fail_unless_intcmp (rtcom_el_set_read_events (el, ids, TRUE, NULL),
            ==, 0);

    query = rtcom_el_query_new (el);
    fail_unless (rtcom_el_query_prepare(query,
                "is-read", TRUE, RTCOM_EL_OP_EQUAL,
                NULL));
    it = rtcom_el_get_events(el, query);
    g_object_unref (query);
    count = iter_count_results (it);

    rtcom_fail_unless_intcmp (count, ==, 3);
    g_object_unref(it);

    rtcom_fail_unless_intcmp (rtcom_el_set_read_events (el, ids, FALSE, NULL),
            ==, 0);

    query = rtcom_el_query_new (el);
    fail_unless (rtcom_el_query_prepare(query,
                "is-read", TRUE, RTCOM_EL_OP_EQUAL,
                NULL));
    it = rtcom_el_get_events(el, query);
    g_object_unref (query);
    fail_unless (it == NULL, "all read flags should have been unset");

    rtcom_el_event_free_contents (ev);
    rtcom_el_event_free (ev);
}
END_TEST

START_TEST(test_flags)
{
    RTComElQuery * query = NULL;
    RTComElEvent * ev = NULL;
    gint event_id = -1;
    RTComElIter * it = NULL;
    gint test_flag1 = 0;
    gint flags = 0;

    ev = event_new_full (time (NULL));
    if(!ev)
    {
        fail("Failed to create event.");
    }

    event_id = rtcom_el_add_event(el, ev, NULL);
    fail_if (event_id < 0, "Failed to add event");

    rtcom_el_set_event_flag(el, event_id, "RTCOM_EL_FLAG_TEST_FLAG1", NULL);

    query = rtcom_el_query_new(el);
    rtcom_el_query_set_limit(query, 5);
    if(!rtcom_el_query_prepare(
                query,
                "id", event_id, RTCOM_EL_OP_EQUAL,
                NULL))
    {
        fail("Failed to prepare the query.");
    }

    it = rtcom_el_get_events(el, query);
    g_object_unref(query);

    fail_unless(it != NULL, "Failed to get iterator");
    fail_unless(rtcom_el_iter_first(it), "Failed to start iterator");

    fail_unless(rtcom_el_iter_get_values(it, "flags", &flags, NULL));

    test_flag1 = rtcom_el_get_flag_value(el, "RTCOM_EL_FLAG_TEST_FLAG1");

    fail_if ((flags & test_flag1) == 0, "flags don't match");

    g_object_unref(it);
    rtcom_el_event_free_contents (ev);
    rtcom_el_event_free (ev);
}
END_TEST


START_TEST(test_get)
{
    RTComElQuery * query = NULL;
    RTComElEvent * ev = NULL;
    RTComElEvent *result = NULL;
    gint event_id = -1;
    RTComElIter * it = NULL;

    ev = event_new_full (time (NULL));
    if(!ev)
    {
        fail("Failed to create event.");
    }

    event_id = rtcom_el_add_event(el, ev, NULL);
    fail_if (event_id < 0, "Fail to add event");

    query = rtcom_el_query_new(el);
    if(!rtcom_el_query_prepare(
                query,
                "id", event_id, RTCOM_EL_OP_EQUAL,
                NULL))
    {
        fail("Failed to prepare the query.");
    }

    it = rtcom_el_get_events(el, query);
    g_object_unref(query);

    fail_unless(it != NULL, "Failed to get iterator");
    fail_unless(rtcom_el_iter_first(it), "Failed to start iterator");

    result = rtcom_el_event_new ();
    fail_unless (result != NULL, "failed to create result event");

    fail_unless (rtcom_el_iter_get_full (it, result),
                 "Failed to get event from iterator");

    fail_unless (rtcom_el_event_equals (ev, result),
                 "Retrieved event doesn't match created one");

    g_object_unref(it);
    rtcom_el_event_free_contents (result);
    rtcom_el_event_free (result);
    rtcom_el_event_free_contents (ev);
    rtcom_el_event_free (ev);
}
END_TEST

START_TEST(test_unique_remotes)
{
    RTComElEvent * ev = NULL;
    gint event_id = -1;
    GList * remote_ebook_uids = NULL;
    GList * remote_uids = NULL;
    GList * remote_names = NULL;
    GList *iter = NULL;

    ev = event_new_full (time (NULL));
    if(!ev)
    {
        fail("Failed to create event.");
    }

    RTCOM_EL_EVENT_SET_FIELD(ev, remote_ebook_uid, g_strdup (REMOTE_EBOOK_UID));

    event_id = rtcom_el_add_event(el, ev, NULL);
    fail_if (event_id < 0, "Fail to add event");

    /* mikhailz: a living API horror? */
    g_free (RTCOM_EL_EVENT_GET_FIELD(ev, remote_ebook_uid));
    g_free (RTCOM_EL_EVENT_GET_FIELD(ev, remote_uid));
    g_free (RTCOM_EL_EVENT_GET_FIELD(ev, remote_name));
    RTCOM_EL_EVENT_SET_FIELD(ev, remote_ebook_uid, g_strdup (REMOTE_EBOOK_UID_2));
    RTCOM_EL_EVENT_SET_FIELD(ev, remote_uid, g_strdup (REMOTE_UID_2));
    RTCOM_EL_EVENT_SET_FIELD(ev, remote_name, g_strdup (REMOTE_NAME_2));

    /* The group_uid field was allocated in the last add_event,
     * because we didn't provide a group_uid of our own.
     */
    g_free(RTCOM_EL_EVENT_GET_FIELD(ev, group_uid));
    RTCOM_EL_EVENT_UNSET_FIELD(ev, group_uid);

    event_id = rtcom_el_add_event(el, ev, NULL);
    fail_if (event_id < 0, "Fail to add event");

    g_free (RTCOM_EL_EVENT_GET_FIELD(ev, remote_ebook_uid));
    g_free (RTCOM_EL_EVENT_GET_FIELD(ev, remote_uid));
    g_free (RTCOM_EL_EVENT_GET_FIELD(ev, remote_name));
    RTCOM_EL_EVENT_SET_FIELD(ev, remote_ebook_uid, g_strdup (REMOTE_EBOOK_UID_3));
    RTCOM_EL_EVENT_SET_FIELD(ev, remote_uid, g_strdup (REMOTE_UID_3));
    RTCOM_EL_EVENT_SET_FIELD(ev, remote_name, g_strdup (REMOTE_NAME_3));

    /* The group_uid field was allocated in the last add_event,
     * because we didn't provide a group_uid of our own */
    g_free(RTCOM_EL_EVENT_GET_FIELD(ev, group_uid));
    RTCOM_EL_EVENT_UNSET_FIELD(ev, group_uid);

    event_id = rtcom_el_add_event(el, ev, NULL);
    fail_if (event_id < 0, "Fail to add event");

    remote_ebook_uids = rtcom_el_get_unique_remote_ebook_uids(el);
    fail_if (remote_ebook_uids == NULL, "Fail to get unique remote_ebook_uids");

    fail_if (g_list_length(remote_ebook_uids) < 2,
             "remote_ebook_uids's length doesn't match");

    for (iter = remote_ebook_uids; iter != NULL; iter = iter->next)
        g_debug("Unique remote_ebook_uid: %s", (const guchar *) iter->data);

    remote_uids =  rtcom_el_get_unique_remote_uids(el);
    fail_if (remote_uids == NULL, "Fail to get unique remote_uids");

    fail_if (g_list_length(remote_uids) < 2, "remote_uids's length doesn't match");

    for (iter = remote_uids; iter != NULL; iter = iter->next)
        g_debug("Unique remote_uid: %s", (const guchar *) iter->data);

    remote_names = rtcom_el_get_unique_remote_names(el);
    fail_if (remote_names == NULL, "Fail to get unique remote_names");

    fail_if (g_list_length(remote_names) < 2,
             "remote_names's length doesn't match");

    for (iter = remote_names; iter != NULL; iter = iter->next)
        g_debug("Unique remote_name: %s", (const guchar *) iter->data);

    rtcom_el_event_free_contents (ev);
    rtcom_el_event_free (ev);
    g_list_foreach(remote_ebook_uids, (GFunc) g_free, NULL);
    g_list_free(remote_ebook_uids);
    g_list_foreach(remote_uids, (GFunc) g_free, NULL);
    g_list_free(remote_uids);
    g_list_foreach(remote_names, (GFunc) g_free, NULL);
    g_list_free(remote_names);
}
END_TEST

START_TEST(test_get_string)
{
    RTComElQuery * query = NULL;
    RTComElEvent * ev = NULL;
    gint event_id = -1;
    RTComElIter * it = NULL;
    gint header_id;
    gchar *bar;

    ev = event_new_full (time (NULL));
    if(!ev)
    {
        fail("Failed to create event.");
    }

    event_id = rtcom_el_add_event(el, ev, NULL);
    fail_if (event_id < 0, "Fail to add event");

    header_id = rtcom_el_add_header(
            el, event_id,
            HEADER_KEY,
            HEADER_VAL,
            NULL);
    fail_if (header_id < 0, "Failed to add header");

    query = rtcom_el_query_new(el);
    if(!rtcom_el_query_prepare(
                query,
                "id", event_id, RTCOM_EL_OP_EQUAL,
                NULL))
    {
        fail("Failed to prepare the query.");
    }

    it = rtcom_el_get_events(el, query);
    g_object_unref(query);

    fail_unless(it != NULL, "Failed to get iterator");
    fail_unless(rtcom_el_iter_first(it), "Failed to start iterator");

    bar = GUINT_TO_POINTER (0xDEADBEEF);
    fail_if(rtcom_el_iter_get_values(it, "there is no such key", &bar, NULL),
            "Shouldn't be able to get a missing value as a string");
    fail_unless(bar == GUINT_TO_POINTER (0xDEADBEEF),
            "bar should be left untouched in this case");

    fail_unless(rtcom_el_iter_get_values(it, HEADER_KEY,  &bar, NULL));
    fail_if(bar == NULL);
    fail_if(bar == GUINT_TO_POINTER (0xDEADBEEF));
    rtcom_fail_unless_strcmp(bar, ==, HEADER_VAL);

    g_free(bar);
    g_object_unref(it);
    rtcom_el_event_free_contents (ev);
    rtcom_el_event_free (ev);
}
END_TEST

START_TEST(test_get_int)
{
    RTComElQuery * query = NULL;
    RTComElEvent * ev = NULL;
    gint event_id = -1;
    RTComElIter * it = NULL;
    gint retrieved;

    ev = event_new_full (time (NULL));
    if(!ev)
    {
        fail("Failed to create event.");
    }

    event_id = rtcom_el_add_event(el, ev, NULL);
    fail_if (event_id < 0, "Fail to add event");

    query = rtcom_el_query_new(el);
    if(!rtcom_el_query_prepare(
                query,
                "id", event_id, RTCOM_EL_OP_EQUAL,
                NULL))
    {
        fail("Failed to prepare the query.");
    }

    it = rtcom_el_get_events(el, query);
    g_object_unref(query);

    fail_unless(it != NULL, "Failed to get iterator");
    fail_unless(rtcom_el_iter_first(it), "Failed to start iterator");

    fail_unless(rtcom_el_iter_get_values(it, "bytes-sent", &retrieved, NULL),
            "Failed to get bytes-sent");
    rtcom_fail_unless_intcmp(retrieved, ==, BYTES_SENT);

    g_object_unref(it);
    rtcom_el_event_free_contents (ev);
    rtcom_el_event_free (ev);
}
END_TEST

START_TEST(test_ends_with)
{
    RTComElQuery * query = NULL;
    RTComElIter * it = NULL;
    gchar *contents;

    query = rtcom_el_query_new(el);
    if(!rtcom_el_query_prepare(
                query,
                "remote-name", "ve", RTCOM_EL_OP_STR_ENDS_WITH,
                NULL))
    {
        fail("Failed to prepare the query.");
    }

    it = rtcom_el_get_events(el, query);
    g_object_unref(query);

    fail_unless(it != NULL, "Failed to get iterator");
    fail_unless(rtcom_el_iter_first(it), "Failed to start iterator");

    fail_unless(rtcom_el_iter_get_values(it, "free-text", &contents, NULL));

    /* It is an API guarantee that we're sorting by event ID, which ascends
     * as time goes on, so Eve's recent message comes before Dave's older
     * message */
    rtcom_fail_unless_strcmp("I am online", ==, contents);
    g_free(contents);

    fail_unless (rtcom_el_iter_next (it));

    fail_unless(rtcom_el_iter_get_values(it, "free-text", &contents, NULL));

    rtcom_fail_unless_strcmp("Hello from Dave", ==, contents);

    g_free(contents);

    fail_if (rtcom_el_iter_next (it));

    g_object_unref(it);
}
END_TEST

START_TEST(test_like)
{
    RTComElQuery * query = NULL;
    RTComElIter * it = NULL;
    gchar *contents;

    query = rtcom_el_query_new(el);
    if(!rtcom_el_query_prepare(
        query,
        "free-text", "AM oNLi", RTCOM_EL_OP_STR_LIKE,
        NULL))
    {
        fail("Failed to prepare the query.");
    }

    it = rtcom_el_get_events(el, query);
    g_object_unref(query);

    fail_unless(it != NULL, "Failed to get iterator");
    fail_unless(rtcom_el_iter_first(it), "Failed to start iterator");

    fail_unless(rtcom_el_iter_get_values(it, "free-text", &contents, NULL));

    rtcom_fail_unless_strcmp("I am online", ==, contents);
    g_free(contents);

    g_object_unref(it);
}
END_TEST

START_TEST(test_delete_events)
{
    RTComElQuery * query = NULL;
    RTComElEvent * ev = NULL;
    RTComElIter *it;
    gint event_id = -1;
    gint count;
    gboolean success;

    /* there are initially only the canned events */
    query = rtcom_el_query_new(el);
    fail_unless (rtcom_el_query_prepare (query,
                NULL));
    it = rtcom_el_get_events (el, query);
    g_object_unref (query);
    fail_unless (it != NULL);
    fail_unless (RTCOM_IS_EL_ITER (it));
    count = iter_count_results (it);
    rtcom_fail_unless_intcmp (count, ==, num_canned_events ());
    g_object_unref (it);

    ev = event_new_full (time (NULL));
    if(!ev)
    {
        fail("Failed to create event.");
    }

    event_id = rtcom_el_add_event(el, ev, NULL);
    fail_if (event_id < 0, "Failed to add event");

    query = rtcom_el_query_new(el);
    if(!rtcom_el_query_prepare(
                query,
                "id", event_id, RTCOM_EL_OP_EQUAL,
                NULL))
    {
        fail("Failed to prepare the query.");
    }

    success = rtcom_el_delete_events(el, query, NULL);
    g_object_unref(query);

    fail_unless (success, "Failed to delete stuff");

    /* check that we deleted only what we wanted to delete */

    rtcom_el_event_free_contents (ev);
    rtcom_el_event_free (ev);

    query = rtcom_el_query_new(el);
    fail_unless (rtcom_el_query_prepare (query,
                "id", event_id, RTCOM_EL_OP_EQUAL,
                NULL));
    it = rtcom_el_get_events (el, query);
    g_object_unref (query);
    fail_unless (it == NULL);
    count = iter_count_results (it);
    rtcom_fail_unless_intcmp (count, ==, 0);

    query = rtcom_el_query_new(el);
    fail_unless (rtcom_el_query_prepare (query,
                NULL));
    it = rtcom_el_get_events (el, query);
    g_object_unref (query);
    fail_unless (it != NULL);
    fail_unless (RTCOM_IS_EL_ITER (it));
    count = iter_count_results (it);
    rtcom_fail_unless_intcmp (count, ==, num_canned_events ());
    g_object_unref (it);
}
END_TEST

START_TEST(test_in_strv)
{
    RTComElQuery * query = NULL;
    RTComElIter * it = NULL;
    gchar *contents;
    const gchar * const interesting_people[] = { "Chris", "Dave", NULL };

    query = rtcom_el_query_new(el);
    if(!rtcom_el_query_prepare(
                query,
                "remote-name", &interesting_people, RTCOM_EL_OP_IN_STRV,
                NULL))
    {
        fail("Failed to prepare the query.");
    }

    it = rtcom_el_get_events(el, query);
    g_object_unref(query);

    fail_unless(it != NULL, "Failed to get iterator");
    fail_unless(rtcom_el_iter_first(it), "Failed to start iterator");

    fail_unless(rtcom_el_iter_get_values(it, "free-text", &contents, NULL));

    /* It is an API guarantee that we're sorting by event ID, which ascends
     * as time goes on, so Dave's recent message comes before Chris's older
     * message */

    rtcom_fail_unless_strcmp("Hello from Dave", ==, contents);

    g_free (contents);

    fail_unless (rtcom_el_iter_next (it));

    fail_unless(rtcom_el_iter_get_values(it, "free-text", &contents, NULL));

    rtcom_fail_unless_strcmp("Hello from Chris", ==, contents);

    fail_if (rtcom_el_iter_next (it));

    g_free (contents);
    g_object_unref(it);
}
END_TEST

START_TEST(test_delete_event)
{
    RTComElQuery * query = NULL;
    RTComElEvent * ev = NULL;
    RTComElIter *it;
    gint event_id = -1;
    gint count;
    gint ret;

    /* there are initially only the canned events */
    query = rtcom_el_query_new(el);
    fail_unless (rtcom_el_query_prepare (query,
                NULL));
    it = rtcom_el_get_events (el, query);
    g_object_unref (query);
    fail_unless (it != NULL);
    fail_unless (RTCOM_IS_EL_ITER (it));
    count = iter_count_results (it);
    rtcom_fail_unless_intcmp (count, ==, num_canned_events ());
    g_object_unref (it);

    ev = event_new_full (time (NULL));
    if(!ev)
    {
        fail("Failed to create event.");
    }

    event_id = rtcom_el_add_event(el, ev, NULL);
    fail_if (event_id < 0, "Failed to add event");

    ret = rtcom_el_delete_event(el, event_id, NULL);
    rtcom_fail_unless_intcmp (ret, ==, 0);

    /* check that we deleted only what we wanted to delete */

    rtcom_el_event_free_contents (ev);
    rtcom_el_event_free (ev);

    query = rtcom_el_query_new(el);
    fail_unless (rtcom_el_query_prepare (query,
                "id", event_id, RTCOM_EL_OP_EQUAL,
                NULL));
    it = rtcom_el_get_events (el, query);
    g_object_unref (query);
    fail_unless (it == NULL);
    count = iter_count_results (it);
    rtcom_fail_unless_intcmp (count, ==, 0);

    query = rtcom_el_query_new(el);
    fail_unless (rtcom_el_query_prepare (query,
                NULL));
    it = rtcom_el_get_events (el, query);
    g_object_unref (query);
    fail_unless (it != NULL);
    fail_unless (RTCOM_IS_EL_ITER (it));
    count = iter_count_results (it);
    rtcom_fail_unless_intcmp (count, ==, num_canned_events ());
    g_object_unref (it);
}
END_TEST

START_TEST(test_string_equals)
{
    RTComElQuery * query = NULL;
    RTComElIter * it = NULL;
    gchar *contents;

    query = rtcom_el_query_new(el);
    if(!rtcom_el_query_prepare(
                query,
                "local-uid", "butterfly/msn/alice", RTCOM_EL_OP_NOT_EQUAL,
                "remote-name", "Bob", RTCOM_EL_OP_EQUAL,
                NULL))
    {
        fail("Failed to prepare the query.");
    }

    it = rtcom_el_get_events(el, query);
    g_object_unref(query);

    fail_unless(it != NULL, "Failed to get iterator");
    fail_unless(rtcom_el_iter_first(it), "Failed to start iterator");

    fail_unless(rtcom_el_iter_get_values(it, "free-text", &contents, NULL),
        "Failed to get values.");

    /* It is an API guarantee that we're sorting by event ID, which ascends
     * as time goes on, so Bob's recent message comes before his older
     * message */

    rtcom_fail_unless_strcmp("Are you there?", ==, contents);
    g_free(contents);

    fail_unless (rtcom_el_iter_next (it));

    fail_unless(rtcom_el_iter_get_values(it, "free-text", &contents, NULL),
        "Failed to get values.");

    rtcom_fail_unless_strcmp("Hi Alice", ==, contents);
    g_free(contents);

    g_object_unref(it);
}
END_TEST

START_TEST(test_int_ranges)
{
    RTComElQuery * query = NULL;
    RTComElIter * it = NULL;
    gchar *contents;

    query = rtcom_el_query_new(el);
    if(!rtcom_el_query_prepare(
                query,
                "start-time", 0, RTCOM_EL_OP_GREATER,
                "start-time", 4000, RTCOM_EL_OP_LESS_EQUAL,
                NULL))
    {
        fail("Failed to prepare the query.");
    }

    it = rtcom_el_get_events(el, query);
    g_object_unref(query);

    fail_unless(it != NULL, "Failed to get iterator");
    fail_unless(rtcom_el_iter_first(it), "Failed to start iterator");

    fail_unless(rtcom_el_iter_get_values(it, "free-text", &contents, NULL),
            "Failed to get values");

    /* It is an API guarantee that we're sorting by event ID, which ascends
     * as time goes on */

    rtcom_fail_unless_strcmp("Are you there?", ==, contents);
    g_free(contents);

    fail_unless (rtcom_el_iter_next (it));

    fail_unless(rtcom_el_iter_get_values(it, "free-text", &contents, NULL),
            "Failed to get values");

    rtcom_fail_unless_strcmp("Hello from Dave", ==, contents);
    g_free(contents);

    fail_unless (rtcom_el_iter_next (it));

    fail_unless(rtcom_el_iter_get_values(it, "free-text", &contents, NULL),
            "Failed to get values");

    rtcom_fail_unless_strcmp("Hello from Chris", ==, contents);
    g_free(contents);

    fail_unless (rtcom_el_iter_next (it));

    fail_unless(rtcom_el_iter_get_values(it, "free-text", &contents, NULL),
            "Failed to get values");

    rtcom_fail_unless_strcmp("Hi Alice", ==, contents);
    g_free(contents);

    fail_if (rtcom_el_iter_next (it));

    g_object_unref(it);
}
END_TEST

START_TEST(test_group_by_uids)
{
    RTComElQuery * query = NULL;
    RTComElIter * it = NULL;
    gchar *s;

    query = rtcom_el_query_new(el);
    rtcom_el_query_set_group_by (query, RTCOM_EL_QUERY_GROUP_BY_UIDS);
    fail_unless(rtcom_el_query_prepare(query,
                "remote-uid", "f", RTCOM_EL_OP_LESS,
                NULL));

    it = rtcom_el_get_events(el, query);
    g_object_unref(query);

    fail_unless(it != NULL, "Failed to get iterator");

    /* It is an API guarantee that we're sorting by event ID, which ascends
     * as time goes on: so this is the order we'll get */

    fail_unless(rtcom_el_iter_first(it), "Failed to start iterator");
    fail_unless (rtcom_el_iter_get_values (it, "remote-uid", &s, NULL));
    rtcom_fail_unless_strcmp("bob@example.com", ==, s);
    g_free (s);
    fail_unless (rtcom_el_iter_get_values (it, "local-uid", &s, NULL));
    rtcom_fail_unless_strcmp("butterfly/msn/alice", ==, s);
    g_free (s);

    fail_unless (rtcom_el_iter_next (it));
    fail_unless (rtcom_el_iter_get_values (it, "remote-uid", &s, NULL));
    rtcom_fail_unless_strcmp("christine@msn.invalid", ==, s);
    g_free (s);

    fail_unless (rtcom_el_iter_next (it));
    fail_unless (rtcom_el_iter_get_values (it, "remote-uid", &s, NULL));
    rtcom_fail_unless_strcmp("eve@example.com", ==, s);
    g_free (s);

    fail_unless (rtcom_el_iter_next (it));
    fail_unless (rtcom_el_iter_get_values (it, "remote-uid", &s, NULL));
    rtcom_fail_unless_strcmp("bob@example.com", ==, s);
    g_free (s);
    fail_unless (rtcom_el_iter_get_values (it, "free-text", &s, NULL));
    rtcom_fail_unless_strcmp("Are you there?", ==, s);
    g_free (s);

    fail_unless (rtcom_el_iter_next (it));
    fail_unless (rtcom_el_iter_get_values (it, "remote-uid", &s, NULL));
    rtcom_fail_unless_strcmp("dave@example.com", ==, s);
    g_free (s);

    fail_unless (rtcom_el_iter_next (it));
    fail_unless (rtcom_el_iter_get_values (it, "remote-uid", &s, NULL));
    rtcom_fail_unless_strcmp("chris@example.com", ==, s);
    g_free (s);

    /* Bob's first message does not appear here, because of the "group by" */

    fail_if (rtcom_el_iter_next (it), "Iterator should have expired");

    g_object_unref(it);
}
END_TEST

START_TEST(test_group_by_metacontacts)
{
    RTComElQuery * query = NULL;
    RTComElIter * it = NULL;
    gchar *s;

    query = rtcom_el_query_new(el);
    rtcom_el_query_set_group_by (query, RTCOM_EL_QUERY_GROUP_BY_CONTACT);
    fail_unless(rtcom_el_query_prepare(query,
                "remote-uid", "f", RTCOM_EL_OP_LESS,
                NULL));

    it = rtcom_el_get_events(el, query);
    g_object_unref(query);

    fail_unless(it != NULL, "Failed to get iterator");

    /* It is an API guarantee that we're sorting by event ID, which ascends
     * as time goes on: so this is the order we'll get */

    fail_unless(rtcom_el_iter_first(it), "Failed to start iterator");
    fail_unless (rtcom_el_iter_get_values (it, "remote-uid", &s, NULL));
    rtcom_fail_unless_strcmp("bob@example.com", ==, s);
    g_free (s);
    fail_unless (rtcom_el_iter_get_values (it, "local-uid", &s, NULL));
    rtcom_fail_unless_strcmp("butterfly/msn/alice", ==, s);
    g_free (s);

    fail_unless (rtcom_el_iter_next (it));
    fail_unless (rtcom_el_iter_get_values (it, "remote-uid", &s, NULL));
    rtcom_fail_unless_strcmp("christine@msn.invalid", ==, s);
    g_free (s);

    fail_unless (rtcom_el_iter_next (it));
    fail_unless (rtcom_el_iter_get_values (it, "remote-uid", &s, NULL));
    rtcom_fail_unless_strcmp("eve@example.com", ==, s);
    g_free (s);

    /* Bob's second message *does* appear here, because in the absence of an
     * unambiguous identifier from the address book, we cannot assume that
     * the MSN user bob@example.com is the same as the XMPP user
     * bob@example.com (this is a bit obscure in protocols with an
     * "@", but becomes more significant in protocols with a flat namespace
     * like AIM, Skype, Myspace, IRC etc.)
     */
    fail_unless (rtcom_el_iter_next (it));
    fail_unless (rtcom_el_iter_get_values (it, "remote-uid", &s, NULL));
    rtcom_fail_unless_strcmp("bob@example.com", ==, s);
    g_free (s);
    fail_unless (rtcom_el_iter_get_values (it, "local-uid", &s, NULL));
    rtcom_fail_unless_strcmp("gabble/jabber/alice", ==, s);
    g_free (s);

    fail_unless (rtcom_el_iter_next (it));
    fail_unless (rtcom_el_iter_get_values (it, "remote-uid", &s, NULL));
    rtcom_fail_unless_strcmp("dave@example.com", ==, s);
    g_free (s);

    /* Bob's first message does not appear here, because it is grouped
     * together with his second (it's the same local UID *and* remote UID).
     *
     * Christine's first message does not appear here either, because her
     * two different remote user IDs are tied together by a metacontact. */

    fail_if (rtcom_el_iter_next (it), "Iterator should have expired");

    g_object_unref(it);
}
END_TEST

START_TEST(test_group_by_group)
{
    RTComElQuery * query = NULL;
    RTComElIter * it = NULL;
    gchar *s;

    query = rtcom_el_query_new(el);
    rtcom_el_query_set_group_by (query, RTCOM_EL_QUERY_GROUP_BY_GROUP);
    fail_unless(rtcom_el_query_prepare(query,
                /* This will match Bob, Christine, Dave, Eve and Frank */
                "remote-uid", "b", RTCOM_EL_OP_GREATER_EQUAL,
                "remote-uid", "g", RTCOM_EL_OP_LESS,
                NULL));

    it = rtcom_el_get_events(el, query);
    g_object_unref(query);

    fail_unless(it != NULL, "Failed to get iterator");

    /* It is an API guarantee that we're sorting by event ID, which ascends
     * as time goes on: so this is the order we'll get */

    fail_unless(rtcom_el_iter_first(it), "Failed to start iterator");
    fail_unless (rtcom_el_iter_get_values (it, "remote-uid", &s, NULL));
    rtcom_fail_unless_strcmp("bob@example.com", ==, s);
    g_free (s);
    fail_unless (rtcom_el_iter_get_values (it, "local-uid", &s, NULL));
    rtcom_fail_unless_strcmp("butterfly/msn/alice", ==, s);
    g_free (s);
    fail_unless (rtcom_el_iter_get_values (it, "group-uid", &s, NULL));
    rtcom_fail_unless_strcmp("group(bob)", ==, s);
    g_free (s);

    fail_unless (rtcom_el_iter_next (it));
    fail_unless (rtcom_el_iter_get_values (it, "remote-uid", &s, NULL));
    rtcom_fail_unless_strcmp("frank@msn.invalid", ==, s);
    g_free (s);
    fail_unless (rtcom_el_iter_get_values (it, "local-uid", &s, NULL));
    rtcom_fail_unless_strcmp("butterfly/msn/alice", ==, s);
    g_free (s);
    fail_unless (rtcom_el_iter_get_values (it, "group-uid", &s, NULL));
    rtcom_fail_unless_strcmp("group(chris+frank)", ==, s);
    g_free (s);

    /* Christine's messages do not appear since Frank's is more recent.
     * Dave and Eve's messages, and Bob's XMPP message, do not appear
     * because they aren't from a groupchat. */

    fail_if (rtcom_el_iter_next (it), "Iterator should have expired");

    g_object_unref(it);
}
END_TEST

START_TEST(test_update_remote_contact)
{
    RTComElQuery *query_by_abook;
    RTComElQuery *query_by_name;
    RTComElIter *it;
    gint count;

    /* We've put Bob in the address book */
    fail_unless (rtcom_eventlogger_update_remote_contact (el,
                "gabble/jabber/alice", "bob@example.com",
                "abook-bob", "Robert", NULL));

    query_by_abook = rtcom_el_query_new(el);
    fail_unless(rtcom_el_query_prepare(query_by_abook,
                "remote-ebook-uid", "abook-bob", RTCOM_EL_OP_EQUAL,
                NULL));

    /* Now, Bob's two XMPP messages are attached to that uid */
    it = rtcom_el_get_events (el, query_by_abook);
    fail_unless (it != NULL, "Failed to get iterator");
    count = iter_count_results (it);
    rtcom_fail_unless_intcmp (count, ==, 2);

    /* Now put Bob's other identity in the address book */
    fail_unless (rtcom_eventlogger_update_remote_contact (el,
                "butterfly/msn/alice", "bob@example.com",
                "abook-bob", "Robert", NULL));

    g_object_unref (it);
    it = rtcom_el_get_events (el, query_by_abook);
    fail_unless (it != NULL, "Failed to get iterator");

    /* Bob's MSN message is attached to that uid too */
    count = iter_count_results (it);
    rtcom_fail_unless_intcmp (count, ==, 3);

    g_object_unref (it);

    /* All three events are now marked as from Robert */
    query_by_name = rtcom_el_query_new(el);
    fail_unless(rtcom_el_query_prepare(query_by_name,
                "remote-name", "Robert", RTCOM_EL_OP_EQUAL,
                NULL));

    it = rtcom_el_get_events (el, query_by_name);
    fail_unless (it != NULL, "Failed to get iterator");
    count = iter_count_results (it);
    rtcom_fail_unless_intcmp (count, ==, 3);

    /* When Robert is deleted from the address book, the name persists */
    fail_unless (rtcom_eventlogger_update_remote_contact (el,
                "gabble/jabber/alice", "bob@example.com",
                NULL, "Robert", NULL));
    fail_unless (rtcom_eventlogger_update_remote_contact (el,
                "butterfly/msn/alice", "bob@example.com",
                NULL, "Robert", NULL));

    g_object_unref (it);
    it = rtcom_el_get_events (el, query_by_name);
    fail_unless (it != NULL, "Failed to get iterator");

    count = iter_count_results (it);
    rtcom_fail_unless_intcmp (count, ==, 3);

    g_object_unref (it);

    g_object_unref (query_by_abook);
    g_object_unref (query_by_name);
}
END_TEST

extern void db_extend_el_suite (Suite *s);

Suite *
el_suite(void)
{
    Suite * s = suite_create ("rtcom-eventlogger");

    /* Low-level DB APi test cases */
    db_extend_el_suite(s);

    /* Core test case */
    TCase * tc_core = tcase_create("Core");
    tcase_add_checked_fixture(tc_core, core_setup, core_teardown);
    tcase_add_test(tc_core, test_add_event);
    tcase_add_test(tc_core, test_add_full);
    tcase_add_test(tc_core, test_header);
    tcase_add_test(tc_core, test_attach);
    tcase_add_test(tc_core, test_read);
    tcase_add_test(tc_core, test_flags);
    tcase_add_test(tc_core, test_get);
    tcase_add_test(tc_core, test_unique_remotes);
    tcase_add_test(tc_core, test_get_int);
    tcase_add_test(tc_core, test_get_string);
    tcase_add_test(tc_core, test_ends_with);
    tcase_add_test(tc_core, test_like);
    tcase_add_test(tc_core, test_delete_events);
    tcase_add_test(tc_core, test_delete_event);
    tcase_add_test(tc_core, test_in_strv);
    tcase_add_test(tc_core, test_string_equals);
    tcase_add_test(tc_core, test_int_ranges);
    tcase_add_test(tc_core, test_group_by_uids);
    tcase_add_test(tc_core, test_group_by_metacontacts);
    tcase_add_test(tc_core, test_group_by_group);
    tcase_add_test(tc_core, test_update_remote_contact);

    suite_add_tcase(s, tc_core);

    return s;
}

int main(void)
{
    int number_failed;
    Suite * s = el_suite();
    SRunner * sr = srunner_create(s);

    srunner_set_xml(sr, "/tmp/result.xml");
    srunner_run_all(sr, CK_NORMAL);
    number_failed = srunner_ntests_failed(sr);
    srunner_free (sr);

    return (number_failed == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}

/* vim: set ai et tw=75 ts=4 sw=4: */

