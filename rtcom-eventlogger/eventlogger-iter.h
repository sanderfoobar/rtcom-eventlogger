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
 * @file eventlogger-iter.h
 * @brief Describes an RTComElIter class.
 *
 * An RTComElIter lets you iterate through events.
 */

#ifndef __EVENT_LOGGER_ITER_H
#define __EVENT_LOGGER_ITER_H

#include <glib-object.h>
#include "rtcom-eventlogger/eventlogger-types.h"
#include "rtcom-eventlogger/eventlogger-attach-iter.h"
#include "rtcom-eventlogger/event.h"

G_BEGIN_DECLS

#define RTCOM_TYPE_EL_ITER             (rtcom_el_iter_get_type ())
#define RTCOM_EL_ITER(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), RTCOM_TYPE_EL_ITER, RTComElIter))
#define RTCOM_EL_ITER_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), RTCOM_TYPE_EL_ITER, RTComElIterClass))
#define RTCOM_IS_EL_ITER(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), RTCOM_TYPE_EL_ITER))
#define RTCOM_IS_EL_ITER_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), RTCOM_TYPE_EL_ITER))
#define RTCOM_EL_ITER_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), RTCOM_TYPE_EL_ITER, RTComMElIterClass))

typedef struct _RTComElIterClass RTComElIterClass;
typedef struct _RTComElIter RTComElIter;

struct _RTComElIterClass
{
    GObjectClass parent_class;
};

struct _RTComElIter
{
    GObject parent_instance;
};

GType rtcom_el_iter_get_type (void) G_GNUC_CONST;

/** Resets the iterator to its first event.
 * @param it The RTComElIter.
 * @return TRUE if success, FALSE if failed.
 */
gboolean rtcom_el_iter_first(
        RTComElIter * it);

/** Advances the iterator to its next event.
 * @param it The RTComElIter.
 * @return FALSE if there’s no next event.
 */
gboolean rtcom_el_iter_next(
        RTComElIter * it);

/** Returns a GValueArray* (or NULL if error), containing all the
 * requested items.
 * E.g.:
 * values = rtcom_el_iter_get_valuearray(it, "local-uid", "channel", NULL);
 * This will put the appropriate values in the GValueArray*. The property
 * name can be anything the appropriate plugin will understand.
 *
 * Note: This is deprecated function, use rtcom_el_iter_get_value_map() or
 * rtcom_el_iter_get_values() where possible.
 *
 * @param it The RTComElIter.
 * @return A GValueArray.
 */
G_GNUC_DEPRECATED GValueArray * rtcom_el_iter_get_valuearray(
        RTComElIter * it,
        ...);


/** Returns a GHashTable* (or NULL if error) containing all the requested
 * items.
 *
 * @param it The RTComElIter
 * @param ... variable number of strings (NULL terminated) representing
 * requested item names.
 * @return A GHashTable containing the (gchar * -> GValue) mapping with the
 * requested items. The hashtable is owned by the caller and should destroy
 * when no longer needed. */
GHashTable *rtcom_el_iter_get_value_map (RTComElIter *it, ...);

/** Returns the requested item values, similar to g_object_get().
 *
 * Example:
 *   gchar *service;
 *   gint event_id;
 *   if (rtcom_el_iter_get_values (it, "service", &service,
 *       "event-id", &event_id, NULL)) { ...success... }
 *
 * @param it The RTComElIter
 * @param ... variable number of (item name, buffer to store item value
 * reference) pairs. The buffer type must be appropriate for the value
 * extraced (one of gint *, gchar**, gboolean*).
 * @return TRUE on success, FALSE on failure.
 */
gboolean rtcom_el_iter_get_values (RTComElIter *it, ...);

/** Returns an iterator to the attachments of the event this
 * iterator points to.
 * @param it The iterator for this event.
 * @return An RTComElAttachIter.
 */
RTComElAttachIter * rtcom_el_iter_get_attachments(
        RTComElIter * it);

/* Plugin functions */

/**
 * Gets a raw field from the db.
 * This function should only be used by plugins.
 * @param it The iterator.
 * @param col The column name.
 * @param value A placeholder for the velua.
 * @return TRUE on success, FALSE on failure.
 */
gboolean rtcom_el_iter_get_raw(
        RTComElIter * it,
        const gchar * col,
        GValue * value);

/**
 * Gets a RTComElEvent representing the current iterator. Note: this
 * calls into plugin and can result in additional SQL queries, avoid
 * calling it in tight loops. Consider using rtcom_el_iter_get_columns()
 * where possible.
 * @param it The iterator
 * @param ev A pointer to the RTComElEvent to populate, which should be
 *  zero-filled
 * @return TRUE in case of success, FALSE in case of failure
 */
gboolean rtcom_el_iter_get_full(
        RTComElIter * it,
        RTComElEvent * ev);

/**
 * Deprecated name of rtcom_el_iter_get_full(). Consider using
 * rtcom_el_get_columns() if possible. */
G_GNUC_DEPRECATED gboolean rtcom_el_iter_get(
        RTComElIter * it,
        RTComElEvent * ev);


/**
 * Gets a GHashTable of (gchar* -> GValue) mapping event fields
 * to their GValues. Both the keys and values are borrowed. Plugins
 * are *not* queried, only the fields directly from the database
 * are returned. This guarantees no additional SQL queries will be
 * done, so it can be used in tight loops.
 * @param it The iterator
 * @return GHashTable with the event fields
 */
const GHashTable *rtcom_el_iter_get_columns(RTComElIter *it);

/**
 * Gets a raw header value from the db.
 * This function should only be used by plugins.
 * @param it The iterator.
 * @param key The header's key.
 * @return The header's value.
 */
gchar * rtcom_el_iter_get_header_raw(
        RTComElIter * it,
        const gchar * key);


G_GNUC_DEPRECATED gboolean rtcom_el_iter_get_int(
        RTComElIter * it,
        const gchar * key,
        gint * ret);

G_GNUC_DEPRECATED gboolean rtcom_el_iter_dup_string(
        RTComElIter * it,
        const gchar * key,
        gchar ** ret);

G_END_DECLS

#endif

/* vim: set ai et tw=75 ts=4 sw=4: */

