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
 * @file eventlogger-attach-iter.h
 * @brief Defines an RTComElAttachIter object.
 *
 * An RTComElAttachIter lets you iterate through attachments
 * of an RTComElEvent.
 */

#ifndef __EVENT_LOGGER_ATTACH_ITER_H
#define __EVENT_LOGGER_ATTACH_ITER_H

#include <glib-object.h>

G_BEGIN_DECLS

#define RTCOM_TYPE_EL_ATTACH_ITER             (rtcom_el_attach_iter_get_type ())
#define RTCOM_EL_ATTACH_ITER(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), RTCOM_TYPE_EL_ATTACH_ITER, RTComElAttachIter))
#define RTCOM_EL_ATTACH_ITER_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), RTCOM_TYPE_EL_ATTACH_ITER, RTComElAttachIterClass))
#define RTCOM_IS_EL_ATTACH_ITER(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), RTCOM_TYPE_EL_ATTACH_ITER))
#define RTCOM_IS_EL_ATTACH_ITER_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), RTCOM_TYPE_EL_ATTACH_ITER))
#define RTCOM_EL_ATTACH_ITER_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), RTCOM_TYPE_EL_ATTACH_ITER, RTComElAttachIterClass))

typedef struct _RTComElAttachIterClass RTComElAttachIterClass;
typedef struct _RTComElAttachIter RTComElAttachIter;
typedef struct _RTComElAttachment RTComElAttachment;

struct _RTComElAttachIterClass
{
    GObjectClass parent_class;
};

struct _RTComElAttachIter
{
    GObject parent_instance;
};

/** The description of an attachment.
 * Contains all the information about an attachment.
 */
struct _RTComElAttachment {
    guint id;       /** The id of the attachment, in the database. */
    guint event_id; /** The event-id for the attachment. */
    gchar * path;   /** The path where the attachment is physically stored. */
    gchar * desc;   /** A description of the attachment. */
};

GType rtcom_el_attach_iter_get_type (void) G_GNUC_CONST;

/** Frees memory for an RTComElAttachment.
 * Call this rather than using free() yourself.
 * @param e The RTComElAttachment you want to free.
 */
void rtcom_el_free_attachment(
        RTComElAttachment * e);

/** Resets the iterator to its first event.
 * @param it The RTComElAttachIter.
 * @return TRUE if success, FALSE if failed.
 */
gboolean rtcom_el_attach_iter_first(
        RTComElAttachIter * it);

/** Advances the iterator to its next attachment.
 * @param it The RTComElAttachIter.
 * @return FALSE if there's no next attachment.
 */
gboolean rtcom_el_attach_iter_next(
        RTComElAttachIter * it);

/** Returns the attachment of an iterator.
 * Remember to free the RTComElAttach* when not needed anymore,
 * using rtcom_el_free_attachment.
 *
 * TODO: provide field accessor methods that access the result set directly,
 * to avoid extra memory allocation and copying.
 *
 * @see rtcom_el_free_attachment
 * @param it The RTComElAttachIter
 * @return An RTComElAttachment.
 */
RTComElAttachment * rtcom_el_attach_iter_get(
        RTComElAttachIter * it);

/**
 * rtcom_el_attachment_new:
 * @path: the absolute filename of the file to attach
 * @desc: the description of the attachment, or %NULL
 *
 * Return a #RTComElAttachment suitable for inclusion in
 * the @attachments parameter of rtcom_el_add_event_full().
 *
 * The @id and @event_id members of the struct are set to 0.
 *
 * Returns: a #RTComElAttachment to be freed with
 * rtcom_el_attachment_free()
 * Since: 0.77
 */
RTComElAttachment *rtcom_el_attachment_new (const gchar *path,
        const gchar *desc);

G_END_DECLS

#endif

/* vim: set ai et tw=75 ts=4 sw=4: */

