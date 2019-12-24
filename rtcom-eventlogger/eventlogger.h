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
 * @file eventlogger.h
 * @brief API for the framework.
 *
 * This is the only file the user needs to include directly.
 */

/**
 * @mainpage rtcom-eventlogger
 *
 * @section abstract_sect Abstract
 *
 * The rtcom-eventlogger is a library whose purpose is to serve a
 * general framework for storing and accessing a persistent log.
 *
 * @section service_sect Service sets
 *
 * Each event is identified by a certain service. A log entry will,
 * therefore, have a particular meaning according to the service it
 * represents. Here’s the list of the default services, with a brief
 * usage suggestion for each. These services are provided via plugins
 * located in /usr/lib/rtcom-eventlogger/. More plugins can be developed.
 * The plugins path is /usr/lib/rtcom-eventlogger:$HOME/.rtcom-eventlogger/plugins.
 * The RTCOM_EL_PLUGINS_PATH can be used to override that.
 *
 * @section sms_sect SMS
 *
 * SMSs should be stored completely in the log. Source, recipient, timestamp,
 * delivery timestamp, and full text.
 *
 * @section mms_sect MMS
 *
 * Same for MMS. The full text should be stored, plus attachments if present.
 *
 * @section chat_sect Chat
 *
 * We can identify chats with three different types:
 *   -# one-to-one conversations
 *   -# ad-hoc chat rooms
 *   -# persistent chat rooms
 *
 * One-to-one conversations should be stored completely, so to provide means to
 * reconstruct the conversation afterwards, and eventually provide a backlog.
 * Ad-hoc chat rooms are trickier, fundamentally for two reasons: they have no
 * meaningful qualified name, and the number of participants is variable. Whereas
 * a one-to-one conversation can be unequivocally identified by its participants,
 * that is the pair (P1, P2), and a persistent chat room (like an IRC channel)
 * can be unequivocally identified by its name and server
 * (e.g. irc.freenode.net/\#c), there are no proper means to identify an ad-hoc
 * chat room. They have random unique names assigned by a server, and the number
 * of participants is variable to the point where the final participants of an
 * ad-hoc  chat room could be totally different from the initial ones.
 * For these reasons, it’s advisable store the name of the ad-hoc chat room in
 * order to be able to at least group together messages belonging to the same
 * room, and to omit the storing of the people present in the ad-hoc chat room
 * when the message being logged is stored.
 * One-to-one conversations should be stored completely.
 * Persistent chat rooms, though, can (and often do) have a high number of
 * participants, and therefore a pretty high volume of traffic, which could clog
 * the storing resources, and perhaps trigger the rotation mechanism (employed
 * to prevent the database from growing too much) when there’s still some
 * important unread event waiting to be noticed by the user. Although the system
 * will allow a user (e.g. a chat client) to store all, it’s highly
 * recommended not to do it. A good compromise might be to only store private
 * messages for the user in a particular persistent chat room.
 *
 * @section email_sect Email
 *
 * For reasons that exceed the scope of this document, emails should not be
 * completely stored, but only the headers should be available.
 *
 * @section blog_sect Blog entries
 *
 * There shouldn’t be a reason to store the store a whole blog entry (which
 * may contain several attachments, like a video) in the log. Perhaps just
 * an excerpt of the text should suffice.
 *
 * @section network_sect Network data
 *
 * GPRS or other networking event should be stored completely. An obvious
 * example is data tranfers.
 *
 * @section calls_sect Phone calls
 *
 * Inbound, missed, and outbound calls should be logged.
 *
 * @section db_sect Database schema
 *
 * @image html images/db.png
 *
 * @section example_sect Examples
 *
 * @include tests/check_el.c
 *
 */

#ifndef __EL_H
#define __EL_H

#include <glib-object.h>

#include "rtcom-eventlogger/event.h"
#include "rtcom-eventlogger/eventlogger-types.h"
#include "rtcom-eventlogger/eventlogger-iter.h"
#include "rtcom-eventlogger/eventlogger-attach-iter.h"

G_BEGIN_DECLS

#define RTCOM_TYPE_EL             (rtcom_el_get_type ())
#define RTCOM_EL(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), RTCOM_TYPE_EL, RTComEl))
#define RTCOM_EL_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), RTCOM_TYPE_EL, RTComElClass))
#define RTCOM_IS_EL(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), RTCOM_TYPE_EL))
#define RTCOM_IS_EL_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), RTCOM_TYPE_EL))
#define RTCOM_EL_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), RTCOM_TYPE_EL, RTComElClass))

#define RTCOM_EL_ERROR rtcom_el_error_quark ()
GQuark rtcom_el_error_quark ();

typedef struct _RTComElClass RTComElClass;
typedef struct _RTComEl RTComEl;

#include "rtcom-eventlogger/eventlogger-query.h"

struct _RTComElClass
{
    GObjectClass parent_class;
};

struct _RTComEl
{
    GObject parent_instance;
};

struct _RTComElRemote {
    gchar *local_uid;
    gchar *remote_uid;
    gchar *abook_uid;
    gchar *remote_name;

};
typedef struct _RTComElRemote RTComElRemote;

GType rtcom_el_get_type (void) G_GNUC_CONST;

/**
 * Creates a new RTComEl.
 * @return A newly allocated RTComEl. It's a GObject, so use
 * g_object_unref to free.
 */
RTComEl * rtcom_el_new (void);

/**
 * Returns a new reference to the shared instance of a singleton
 * RTComEl object. This method is not thread-safe.
 * @return A new reference to an RTComEl object. Should be unreffed
 * after use.
 */
RTComEl * rtcom_el_get_shared (void);

/** Stores an event.
 * @param el The RTComEl object.
 * @param event An RTComElEvent object.
 * @param error A location fo the possible error message. Can be NULL if not interesting.
 * @return The id of the new event. -1 in case of error.
 */
gint rtcom_el_add_event(
        RTComEl * el,
        RTComElEvent * event,
        GError ** error);

/** Stores an event and all its headers/attachments in one atomic operation.
 * @param el The RTComEl object.
 * @param ev An RTComElEvent object.
 * @param headers A (gchar *name -> gchar *value) mapping of event headers
 * @param attachments A list of RTComElAttachments to add to the event
 * @param error A location for the possible error message
 * @return The ID of new event or -1 in case of failure.
 */
gint rtcom_el_add_event_full(
        RTComEl * el,
        RTComElEvent * ev,
        GHashTable *headers,
        GList *attachments,
        GError ** error);

/** Returns the group-uid of the event you added last.
 * This is useful if you start logging a chat conversation, get a group-uid
 * from here after logging the first message, and then keep using the same
 * group-uid for the rest of the messages in the same conversation.
 * @param el The #RTComEl object
 * return The group-uid of an added event, or NULL if not applicable.
 */
const gchar * rtcom_el_get_last_group_uid(
        RTComEl * el);

/** Adds a custom header to an event.
 * @param el The RTComEl object.
 * @param event_id The id of the event you want to add the header to.
 * @param key The key (or name) for the header.
 * @param value The value for the header.
 * @param error A location for the possible error message. Can be NULL if not interesting.
 * @return The header id, or -1 in case of error.
 */
gint rtcom_el_add_header(
        RTComEl * el,
        gint event_id,
        const gchar * key,
        const gchar * value,
        GError ** error);

/** Adds an attachment to an event.
 * @param el The RTComEl object.
 * @param event_id The id of the event you want to add the attachment to.
 * @param path The path where the file you want to attach is located.
 * @param desc A description for the attachment. Can be NULL.
 * @param error A location for the possible error message. Can be NULL if not interesting.
 * @return The attachment id or -1 in case of error.
 */
gint rtcom_el_add_attachment(
        RTComEl * el,
        gint event_id,
        const gchar * path,
        const gchar * desc,
        GError ** error);

/** Marks an event as read/unread.
 * @param el The RTComEl object.
 * @param event_id The id of the event you want to mark as read/unread.
 * @param read TRUE if you want to mark read, FALSE if you want to mark unread.
 * @param error A location for the possible error message. Can be NULL if not interesting.
 * @return 0 if success, -1 if failure.
 */
gint rtcom_el_set_read_event(
        RTComEl * el,
        gint event_id,
        gboolean read,
        GError ** error);

/** Marks multiple events as read/unread.
 * @param el The RTComEl object.
 * @param event_ids An array of ids of events. Must be NULL terminated.
 * @param read TRUE if you want to mark read, FALSE if you want to mark unread.
 * @param error A location for the possible error message. Can be NULL if not interesting.
 * Returns 0 if success, -1 if failure.
 */
gint rtcom_el_set_read_events(
        RTComEl * el,
        gint * event_ids,
        gboolean read,
        GError ** error);

/**
 * Sets a flag for an event.
 * @param el The #RTComEl object
 * @param event_id The id of the event
 * @param flag The name of the flag
 * @param error A location for the possible error message. Can be NULL if
 * not interesting.
 * @return 0 if success, -1 if failure
 */
gint rtcom_el_set_event_flag(
        RTComEl * el,
        gint event_id,
        const gchar * flag,
        GError ** error);

/**
 * Unsets a flag for an event.
 * @param el The #RTComEl object
 * @param event_id The id of the event
 * @param flag The name of the flag
 * @param error A location for the possible error message. Can be NULL if
 * not interesting.
 * @return 0 if success, -1 if failure
 */
gint rtcom_el_unset_event_flag(
        RTComEl * el,
        gint event_id,
        const gchar * flag,
        GError ** error);

/**
 * Sets the end-time property of an event.
 * @param el The #RTComEl object
 * @param event_id The id of the event
 * @param end_time The end-time property
 * @error A location for the possible error message. Can be NULL if not
 * interesting
 * @return TRUE in case of success, FALSE otherwise
 */
gboolean rtcom_el_set_end_time(
        RTComEl * el,
        gint event_id,
        time_t end_time,
        GError ** error);

/** Retrieves events from the database.
 * Returns an iterator to the first event in the constructed
 * query, or NULL if an error occurred or none found.
 * @param el The #RTComEl object
 * @param query The #RTComElQuery to perform
 * @return An iterator to the events.
 */
RTComElIter * rtcom_el_get_events(
        RTComEl * el,
        RTComElQuery * query);

/** Retrieves events from the database in an atomic way.
 * Like rtcom_el_get_events, but the returned iterator is
 * guarded by transactional brackets, so the table contents are
 * guaranteed not to be changed by other processess. Care must
 * be taken that the iterator is properly unreffed to release
 * the database lock, and that no event addition/modification
 * is done in the meantime (because there's no support for
 * nested transactions).
 * Returns an iterator to the first event in the constructed
 * query, or NULL if an error occurred or none found.
 * @param el The #RTComEl object
 * @param query The #RTComElQuery to perform
 * @return An iterator to the events.
 */
RTComElIter * rtcom_el_get_events_atomic(
        RTComEl * el,
        RTComElQuery * query);

/**
 * Gets all headers of an event from the database.
 * @param el The #RTComEl object
 * @param event_id The id of the event whose headers you want to fetch
 * @return An hash table of string:string (key:value)
 */
GHashTable * rtcom_el_fetch_event_headers(
        RTComEl * el,
        gint event_id);

/**
 * Gets all event-ids that match a certain key:value in the Headers table..
 * @param el The #RTComEl object
 * @param key The header key
 * @param val The header value
 * @return An array of gints, whose last element is -1, or NULL in case of
 * error. Please free the array with g_free.
 */
gint * rtcom_el_get_events_by_header(
        RTComEl * el,
        const gchar * key,
        const gchar * val);

/** Retrieves the id of a service.
 * @param el The RTComEl object.
 * @param service The name of the service.
 * @return The service-id, or -1 if not found.
 */
gint rtcom_el_get_service_id(
        RTComEl * el,
        const gchar * service);

/** Retrieves the id of an event-type.
 * @param el The RTComEl object.
 * @param eventtype The name of the event-type.
 * @return The id of the event-type, or -1 if not found.
 */
gint rtcom_el_get_eventtype_id(
        RTComEl * el,
        const gchar * eventtype);

/** Retrieve the value of a flag.
 *
 * FIXME: use of a db table to map strings to integer flags is an utter waste
 * of CPU time. Define and document your flag values, use them consistently
 * throughout your database. If you <strong>must</strong>, then at least
 * prepare the query once per lifetime of a RTComEl and bind flag strings to it.
 *
 * @param el The RTComEl object.
 * @param flag The name of the flag.
 * @return The value of the flag, or -1 if not found.
 */
gint rtcom_el_get_flag_value(
        RTComEl * el,
        const gchar * flag);

/**
 * Retrieve all the unique remote_ebook_uid's in the db.
 * @param el The #RTComEl object
 * @return a GList containing a const gchar * for each unique remote_ebook_uid
 * in the db
 */
GList * rtcom_el_get_unique_remote_ebook_uids(
        RTComEl * el);

/**
 * Retrieve all the unique remote_uid's in the db.
 * @param el The #RTComEl object
 * @return a GList containing a const gchar * for each unique remote_uid
 * in the db
 */
GList * rtcom_el_get_unique_remote_uids(
        RTComEl * el);

/**
 * Retrieve all the unique remote_name's in the db.
 * @param el The #RTComEl object
 * @return a GList containing a const gchar * for each unique remote_name
 * in the db
 */
GList * rtcom_el_get_unique_remote_names(
        RTComEl * el);

/**
 * Retrieve all the unique account identifiers in the db.
 * An account identifier is a string made of the concatenation
 * of the remote-ebook-uid and the "mc-account-name" header, if present.
 * @param el The #RTComEl object
 * @return a GList containing a const gchar * for each unique account
 * identifiers in the db
 */
GList * rtcom_el_get_unique_account_ids(
        RTComEl * el);

/**
 * Returns information about a group of events.
 * @param el The #RTComEl object
 * @param group_uid The group_uid that identidies the group
 * @param total_events A placeholder for the number of events in the group
 * @param unread_events A placeholder for the number of unread evetents in
 * the group
 * @param group_flags The boolean OR of the flags of all the events in the
 * group
 * @return TRUE in case of success, FALSE otherwise
 */
gboolean rtcom_el_get_group_info(
        RTComEl * el,
        const gchar * group_uid,
        gint * total_events,
        gint * unread_events,
        gint * group_flags);

/**
 * Returns the id of the most recent event in a group.
 * @param el The #RTComEl object
 * @param group_uid the UID of the group
 * @return the id of the most recent event in the group
 */
gint rtcom_el_get_group_most_recent_event_id(
        RTComEl * el,
        const gchar * group_uid);

/**
 * Returns the number of events for a certain remote-ebook-uid.
 * @param el The #RTComEl object
 * @param remote_ebook_uid the remote-ebook-uid
 * @return The number of events in the database
 */
gint rtcom_el_get_contacts_events_n(
        RTComEl * el,
        const gchar * remote_ebook_uid);

/**
 * Returns the number of events for a certain pair of local_uid and
 * remote_uid.
 * @param el The #RTComEl object
 * @param local_uid The local_uid
 * @param remote_uid The remote_uid
 * @return The number of events, or -1 in case of error.
 */
gint rtcom_el_get_local_remote_uid_events_n(
        RTComEl * el,
        const gchar * local_uid,
        const gchar * remote_uid);

/**
 * Removes an event from the database.
 * @param el The #RTComEl object
 * @param event_id The id of the event you want to remove
 * @param error A location for the possible error message. Can be NULL if not interesting
 * @return -1 in case of error, or 0 in case of success
 */
gint rtcom_el_delete_event(
        RTComEl * el,
        gint event_id,
        GError ** error);

/**
 * Removes all the events matching a query.
 * @param el The #RTComEl object
 * @param query A prepared #RTComElQuery
 * @param error A location for the possible error message. Can be NULL if
 * not interesting.
 * @return FALSE in case of error, TRUE in case of success
 */
gboolean rtcom_el_delete_events(
        RTComEl * el,
        RTComElQuery * query,
        GError ** error);

/**
 * Removes all events matching a service.
 * @param el The #RTComEl object
 * @param service The service
 * @return TRUE in case of success, FALSE otherwise
 */
gboolean rtcom_el_delete_by_service(
        RTComEl * el,
        const gchar * service);

/**
 * Removes all events matching specified group uids
 * @param el The #RTComEl object
 * @param group_uids A NULL-terminated array of strings
 * @return TRUE in case of success, FALSE otherwise
 */
gboolean rtcom_el_delete_by_group_uids(
        RTComEl * el,
        const gchar ** group_uids);

/**
 * Removes all events from the db.
 * @param el The #RTComEl object
 * @return TRUE in case of success, FALSE otherwise
 */
gboolean rtcom_el_delete_all(
        RTComEl * el);

/**
 * Gets the number of events for a specific service. Could be used for
 * statistical purposes.
 * @param el The #RTComEl object
 * @param service The service. If NULL, than this function will return the
 * number of all events in the database.
 * @return the number of events for the service, or -1 in case of error.
 */
gint rtcom_el_count_by_service(
        RTComEl * el,
        const gchar * service);

/**
 * Returns the id of a Flag.
 * @param el The #RTComEl object
 * @param flag The name of the Flag
 * @return -1 in case of error, or the id of the requested Flag
 */
gint rtcom_el_get_flag_id(
        RTComEl * el,
        const gchar * flag);


/**
 * Updates the remote contact data.
 * @param el the #RTComEl object
 * @param local_uid the ID for the account on the contact is being changed
 * @param remote_uid the remote contact ID
 * @param new_abook_uid new abook ID for this contact
 * @param new_remote_name new remote name (to be cached) for this contact
 * @param error A GError** to be set on failure
 * @return TRUE on succes, FALSE in case of failure
 */
gboolean rtcom_el_update_remote_contact(RTComEl *el,
    const gchar *local_uid, const gchar *remote_uid,
    const gchar *new_abook_uid, const gchar *new_remote_name,
    GError **error);

/* For backwards compatibility */
#define rtcom_eventlogger_update_remote_contact(a,b,c,d,e,f) \
	rtcom_el_update_remote_contact(a,b,c,d,e,f)

/**
 * A plural version of rtcom_el_update_remote_contact() for batch updates
 * @param el the #RTComEl object
 * @param contacts a Glist* of #RTComElRemote structs
 * @param error A GError** to be set on failure
 * @return TRUE on succes, FALSE in case of failure
 */
gboolean rtcom_el_update_remote_contacts(RTComEl *el,
    GList *contacts, GError **error);

/**
 * Removes any association with the specified abook uid.
 * @param el the #RTComEl object
 * @param abook_uid the abook ID for the contact
 * @param error A GError** to be set on failure
 * @return TRUE on success, FALSE in case of failure
 */
gboolean rtcom_el_remove_abook_uid (RTComEl *el,
    const gchar *abook_uid, GError **error);

/**
 * A plural version of rtcom_el_remove_abook_uid() for batch updates.
 * @param el the #RTComEl object
 * @param uids a GList* of gchar * abook uids to be cleared
 * @param error a GError** to be set on failure
 * @return TRUE on success, FALSE in case of failure
 */
gboolean rtcom_el_remove_abook_uids (RTComEl *el, GList *uids, GError **error);

/** Fire EventUpdated DBus signal (only to be used from plugins, to signal their state changed).
 * @param el The RTComEl object.
 * @param event_id The id of the event that should be signalled as updated.
 * @return 0 if success, -1 if failure.
 */
gint rtcom_el_fire_event_updated(
        RTComEl * el,
        gint event_id);

G_END_DECLS

#endif

/* vim: set ai et tw=75 ts=4 sw=4: */

