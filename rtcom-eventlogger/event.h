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
 * @file event.h
 * @brief Defines an RTComElEvent structure.
 *
 * RTComElEvent desribes an event.
 */

#ifndef __EVENT_H
#define __EVENT_H

#include "rtcom-eventlogger/eventlogger-types.h"

G_BEGIN_DECLS

typedef struct _RTComElEvent RTComElEvent;
struct _RTComElEvent
{
    guint _mask;

    /* Raw items from db */
    gint fld_id;
    gint fld_service_id;
    gint fld_event_type_id;
    time_t fld_storage_time;
    time_t fld_start_time;
    time_t fld_end_time;
    gboolean fld_is_read;
    gint fld_flags;
    gint fld_bytes_sent;
    gint fld_bytes_received;
    gchar * fld_local_uid;
    gchar * fld_local_name;
    gchar * fld_remote_uid;
    gchar * fld_remote_name;
    gchar * fld_remote_ebook_uid;
    gchar * fld_channel;
    gchar * fld_free_text;
    gchar * fld_group_uid;

    /* Artificially constructed items */
    gchar * fld_service;
    gchar * fld_event_type;

    gchar * fld_additional_text;
    gchar * fld_icon_name;
    gchar * fld_pango_markup;

    gboolean fld_outgoing;
};

enum
{
    _is_set_id                 = 1 << 0,
    _is_set_service_id         = 1 << 1,
    _is_set_event_type_id      = 1 << 2,
    _is_set_storage_time       = 1 << 3,
    _is_set_start_time         = 1 << 4,
    _is_set_end_time           = 1 << 5,
    _is_set_is_read            = 1 << 6,
    _is_set_flags              = 1 << 7,
    _is_set_bytes_sent         = 1 << 8,
    _is_set_bytes_received     = 1 << 9,
    _is_set_remote_ebook_uid   = 1 << 10,
    _is_set_local_uid          = 1 << 11,
    _is_set_local_name         = 1 << 12,
    _is_set_remote_uid         = 1 << 13,
    _is_set_remote_name        = 1 << 14,
    _is_set_channel            = 1 << 15,
    _is_set_free_text          = 1 << 16,
    _is_set_group_uid          = 1 << 17,

    _is_set_service            = 1 << 18,
    _is_set_event_type         = 1 << 19,
    _is_set_additional_text    = 1 << 20,
    _is_set_icon_name          = 1 << 21,
    _is_set_pango_markup       = 1 << 22,
    _is_set_outgoing           = 1 << 23
};

#define RTCOM_EL_EVENT_INIT(ev) ((ev)->_mask = 0)
#define RTCOM_EL_EVENT_IS_SET(ev, FIELD) ((ev)->_mask & _is_set_ ## FIELD)
#define RTCOM_EL_EVENT_GET_FIELD(ev, FIELD) ((ev)->fld_ ## FIELD)
#define RTCOM_EL_EVENT_SET_FIELD(ev, FIELD, value) \
    do {\
        (ev)->_mask |= _is_set_ ## FIELD; \
        (ev)->fld_ ## FIELD = value;\
    } while(0)
#define RTCOM_EL_EVENT_UNSET_FIELD(ev, FIELD) (ev)->_mask &= ~ _is_set_ ## FIELD;

/**
 * Creates a new RTComElEvent.
 * If the event was created statically, then RTCOM_EL_EVENT_INIT must be
 * used.
 * Note that if you set some fields using g_strdup, you are responsible of
 * freeing them.
 * @see rtcom_el_event_free
 * @return A newly allocated RTComElEvent.
 */
RTComElEvent * rtcom_el_event_new(void);

/**
 * Compares two RTComElEvent's.
 * @param first The first operand
 * @param second The second operand
 * @return TRUE if equal, FALSE otherwise
 */
gboolean rtcom_el_event_equals(
        RTComElEvent * first,
        RTComElEvent * second);

/**
 * Frees the memory allocated for an #RTComElEvent.
 * @param ev The structure to free.
 */
void rtcom_el_event_free(
        RTComElEvent * ev);

/**
 * Frees the contents of an #RTComElEvent, assuming that all strings are
 * either %NULL or allocated with g_strdup(), and fills the struct with
 * zero bytes.
 */
void rtcom_el_event_free_contents(
        RTComElEvent *ev);

G_END_DECLS

#endif

/* vim: set ai et tw=75 ts=4 sw=4: */

