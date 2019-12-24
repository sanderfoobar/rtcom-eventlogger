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

#include "rtcom-eventlogger/event.h"

#include <string.h>

RTComElEvent * rtcom_el_event_new()
{
    RTComElEvent * ev = g_slice_new0 (RTComElEvent);
    return ev;
}

void rtcom_el_event_free(
        RTComElEvent * ev)
{
    g_return_if_fail(ev != NULL);
    g_slice_free(RTComElEvent, ev);
}

gboolean rtcom_el_event_equals(
        RTComElEvent * first,
        RTComElEvent * second)
{
    if(RTCOM_EL_EVENT_IS_SET(first, id) &&
       RTCOM_EL_EVENT_IS_SET(second, id))
    {
        if(RTCOM_EL_EVENT_GET_FIELD(first, id) !=
           RTCOM_EL_EVENT_GET_FIELD(second, id))
        {
            g_debug("id differs");
            return FALSE;
        }
    }

    if(RTCOM_EL_EVENT_IS_SET(first, service_id) &&
       RTCOM_EL_EVENT_IS_SET(second, service_id))
    {
        if(RTCOM_EL_EVENT_GET_FIELD(first, service_id) !=
           RTCOM_EL_EVENT_GET_FIELD(second, service_id))
        {
            g_debug("service_id differs");
            return FALSE;
        }
    }

    if(RTCOM_EL_EVENT_IS_SET(first, event_type_id) &&
       RTCOM_EL_EVENT_IS_SET(second, event_type_id))
    {
        if(RTCOM_EL_EVENT_GET_FIELD(first, event_type_id) !=
           RTCOM_EL_EVENT_GET_FIELD(second, event_type_id))
        {
            g_debug("event_type_id differs");
            return FALSE;
        }
    }

    if(RTCOM_EL_EVENT_IS_SET(first, storage_time) &&
       RTCOM_EL_EVENT_IS_SET(second, storage_time))
    {
        if(RTCOM_EL_EVENT_GET_FIELD(first, storage_time) !=
           RTCOM_EL_EVENT_GET_FIELD(second, storage_time))
        {
            g_debug("storage_time differs");
            return FALSE;
        }
    }

    if(RTCOM_EL_EVENT_IS_SET(first, start_time) &&
       RTCOM_EL_EVENT_IS_SET(second, start_time))
    {
        if(RTCOM_EL_EVENT_GET_FIELD(first, start_time) !=
           RTCOM_EL_EVENT_GET_FIELD(second, start_time))
        {
            g_debug("start_time differs");
            return FALSE;
        }
    }

    if(RTCOM_EL_EVENT_IS_SET(first, end_time) &&
       RTCOM_EL_EVENT_IS_SET(second, end_time))
    {
        if(RTCOM_EL_EVENT_GET_FIELD(first, end_time) !=
           RTCOM_EL_EVENT_GET_FIELD(second, end_time))
        {
            g_debug("end_time differs");
            return FALSE;
        }
    }

    if(RTCOM_EL_EVENT_IS_SET(first, flags) &&
       RTCOM_EL_EVENT_IS_SET(second, flags))
    {
        if(RTCOM_EL_EVENT_GET_FIELD(first, flags) !=
           RTCOM_EL_EVENT_GET_FIELD(second, flags))
        {
            g_debug("flags differs");
            return FALSE;
        }
    }

    if(RTCOM_EL_EVENT_IS_SET(first, bytes_sent) &&
       RTCOM_EL_EVENT_IS_SET(second, bytes_sent))
    {
        if(RTCOM_EL_EVENT_GET_FIELD(first, bytes_sent) !=
           RTCOM_EL_EVENT_GET_FIELD(second, bytes_sent))
        {
            g_debug("bytes_sent differs");
            return FALSE;
        }
    }

    if(RTCOM_EL_EVENT_IS_SET(first, bytes_received) &&
       RTCOM_EL_EVENT_IS_SET(second, bytes_received))
    {
        if(RTCOM_EL_EVENT_GET_FIELD(first, bytes_received) !=
           RTCOM_EL_EVENT_GET_FIELD(second, bytes_received))
        {
            g_debug("bytes_received differs");
            return FALSE;
        }
    }

    if(RTCOM_EL_EVENT_IS_SET(first, remote_ebook_uid) &&
       RTCOM_EL_EVENT_IS_SET(second, remote_ebook_uid))
    {
        if(strcmp(RTCOM_EL_EVENT_GET_FIELD(first, remote_ebook_uid),
                  RTCOM_EL_EVENT_GET_FIELD(second, remote_ebook_uid)))
        {
            g_debug("remote_ebook_uid differs");
            return FALSE;
        }
    }

    if(RTCOM_EL_EVENT_IS_SET(first, remote_ebook_uid) &&
       RTCOM_EL_EVENT_IS_SET(second, remote_ebook_uid))
    {
        if(strcmp(RTCOM_EL_EVENT_GET_FIELD(first, remote_ebook_uid),
                  RTCOM_EL_EVENT_GET_FIELD(second, remote_ebook_uid)))
        {
            g_debug("remote_ebook_uid differs");
            return FALSE;
        }
    }

    if(RTCOM_EL_EVENT_IS_SET(first, local_uid) &&
       RTCOM_EL_EVENT_IS_SET(second, local_uid))
    {
        if(strcmp(RTCOM_EL_EVENT_GET_FIELD(first, local_uid),
                  RTCOM_EL_EVENT_GET_FIELD(second, local_uid)))
        {
            g_debug("local_uid differs");
            return FALSE;
        }
    }

    if(RTCOM_EL_EVENT_IS_SET(first, local_name) &&
       RTCOM_EL_EVENT_IS_SET(second, local_name))
    {
        if(strcmp(RTCOM_EL_EVENT_GET_FIELD(first, local_name),
                  RTCOM_EL_EVENT_GET_FIELD(second, local_name)))
        {
            g_debug("local_name differs");
            return FALSE;
        }
    }

    if(RTCOM_EL_EVENT_IS_SET(first, remote_uid) &&
       RTCOM_EL_EVENT_IS_SET(second, remote_uid))
    {
        if(strcmp(RTCOM_EL_EVENT_GET_FIELD(first, remote_uid),
                  RTCOM_EL_EVENT_GET_FIELD(second, remote_uid)))
        {
            g_debug("remote_uid differs");
            return FALSE;
        }
    }

    if(RTCOM_EL_EVENT_IS_SET(first, remote_name) &&
       RTCOM_EL_EVENT_IS_SET(second, remote_name))
    {
        if(strcmp(RTCOM_EL_EVENT_GET_FIELD(first, remote_name),
                  RTCOM_EL_EVENT_GET_FIELD(second, remote_name)))
        {
            g_debug("remote_name differs");
            return FALSE;
        }
    }

    if(RTCOM_EL_EVENT_IS_SET(first, channel) &&
       RTCOM_EL_EVENT_IS_SET(second, channel))
    {
        if(strcmp(RTCOM_EL_EVENT_GET_FIELD(first, channel),
                  RTCOM_EL_EVENT_GET_FIELD(second, channel)))
        {
            g_debug("channel differs");
            return FALSE;
        }
    }

    if(RTCOM_EL_EVENT_IS_SET(first, free_text) &&
       RTCOM_EL_EVENT_IS_SET(second, free_text))
    {
        if(strcmp(RTCOM_EL_EVENT_GET_FIELD(first, free_text),
                  RTCOM_EL_EVENT_GET_FIELD(second, free_text)))
        {
            g_debug("free_text differs");
            return FALSE;
        }
    }

    if(RTCOM_EL_EVENT_IS_SET(first, group_uid) &&
       RTCOM_EL_EVENT_IS_SET(second, group_uid))
    {
        if(strcmp(RTCOM_EL_EVENT_GET_FIELD(first, group_uid),
                  RTCOM_EL_EVENT_GET_FIELD(second, group_uid)))
        {
            g_debug("group_uid differs");
            return FALSE;
        }
    }

    return TRUE;
}

void
rtcom_el_event_free_contents (RTComElEvent *ev)
{
    g_return_if_fail (ev != NULL);

    g_free (ev->fld_local_uid);
    g_free (ev->fld_local_name);
    g_free (ev->fld_remote_uid);
    g_free (ev->fld_remote_name);
    g_free (ev->fld_remote_ebook_uid);
    g_free (ev->fld_channel);
    g_free (ev->fld_free_text);
    g_free (ev->fld_group_uid);
    g_free (ev->fld_service);
    g_free (ev->fld_event_type);
    g_free (ev->fld_additional_text);
    g_free (ev->fld_icon_name);
    g_free (ev->fld_pango_markup);

    memset (ev, '\0', sizeof (*ev));
}

/* vim: set ai et tw=75 ts=4 sw=4: */

