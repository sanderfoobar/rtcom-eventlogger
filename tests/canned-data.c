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

#include "canned-data.h"

#include "rtcom-eventlogger/eventlogger.h"

#include <check.h>

#define SERVICE "RTCOM_EL_SERVICE_TEST"
#define EVENT_TYPE "RTCOM_EL_EVENTTYPE_TEST_ET1"

static struct {
    const gchar *local_uid;
    const gchar *remote_uid;
    const gchar *remote_name;
    const gchar *free_text;
    const gchar *remote_ebook_uid;
    const gchar *group_uid;
} events[] = {

    /* Various 1-1 messages on XMPP */
      { "gabble/jabber/alice", "bob@example.com", "Bob", "Hi Alice", NULL },
      { "gabble/jabber/alice", "chris@example.com", "Chris",
        "Hello from Chris", "abook-chris" },
      { "gabble/jabber/alice", "dave@example.com", "Dave", "Hello from Dave",
        "abook-dave" },
      { "gabble/jabber/alice", "bob@example.com", "Bob", "Are you there?",
        NULL},
      { "gabble/jabber/alice", "eve@example.com", "Eve", "I am online", NULL },

    /* A three-way MSN chat involving Christine, Frank and Alice; Christine
     * is the same person (same abook entry) as Chris above */
      { "butterfly/msn/alice", "christine@msn.invalid", "Christine",
        "Hello again from Chris (under a different name)", "abook-chris",
        "group(chris+frank)" },
      { "butterfly/msn/alice", "christine@msn.invalid", "Christine",
        "Shall we go to the pub tonight?", "abook-chris",
        "group(chris+frank)" },
      { "butterfly/msn/alice", "frank@msn.invalid", "Frank",
        "Yes!", NULL, "group(chris+frank)" },

    /* A two-way MSN chat involving Bob and Alice */
      { "butterfly/msn/alice", "bob@example.com", "Bob",
        "Or are you using this account?", NULL, "group(bob)" },

      { NULL }
};

void
add_canned_events (RTComEl *el)
{
    RTComElEvent *ev = rtcom_el_event_new ();
    gint i;

    for (i = 0; i < num_canned_events (); i++)
    {
        RTCOM_EL_EVENT_SET_FIELD (ev, service, g_strdup (SERVICE));
        RTCOM_EL_EVENT_SET_FIELD (ev, event_type, g_strdup (EVENT_TYPE));
        RTCOM_EL_EVENT_SET_FIELD (ev, start_time, (i + 1) * 1000);
        RTCOM_EL_EVENT_SET_FIELD (ev, end_time, 0);
        RTCOM_EL_EVENT_SET_FIELD (ev, local_uid,
            g_strdup (events[i].local_uid));
        RTCOM_EL_EVENT_SET_FIELD (ev, local_name, g_strdup ("Alice"));
        RTCOM_EL_EVENT_SET_FIELD (ev, remote_uid,
            g_strdup (events[i].remote_uid));
        RTCOM_EL_EVENT_SET_FIELD (ev, remote_name,
            g_strdup (events[i].remote_name));
        RTCOM_EL_EVENT_SET_FIELD (ev, free_text,
            g_strdup (events[i].free_text));
        RTCOM_EL_EVENT_SET_FIELD (ev, remote_ebook_uid,
            g_strdup (events[i].remote_ebook_uid));
        RTCOM_EL_EVENT_SET_FIELD (ev, group_uid,
            g_strdup (events[i].group_uid));

        fail_unless (rtcom_el_add_event (el, ev, NULL) >= 0);
        rtcom_el_event_free_contents (ev);
    }

    rtcom_el_event_free (ev);
}

gint
num_canned_events (void)
{
    return G_N_ELEMENTS (events) - 1;
}
