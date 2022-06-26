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
 * @file eventlogger-types.h
 * @brief Defines some useful types.
 *
 * The user doesn't need to include this, as it's indirectly included.
 */

#ifndef __EVENTLOGGER_TYPES_H
#define __EVENTLOGGER_TYPES_H

#include <glib.h>

/**
 * Error messages.
 */
typedef enum {
    RTCOM_EL_INTERNAL_ERROR, /** This should never happen. Contact the developer if you get this. */
    RTCOM_EL_INVALID_ARGUMENT_ERROR, /** You supplied a wrong argument to the function. */
    RTCOM_EL_TEMPORARY_ERROR, /** Database is locked at the moment, try again later. */
    RTCOM_EL_DATABASE_FULL, /* No space on the device */
    RTCOM_EL_DATABASE_CORRUPTED, /* Database image is corrupted */
} RTComElError;

#define RTCOM_EL_COLUMN_SIZE 20
typedef enum {
    RTCOM_EL_COLUMN_ID,
    RTCOM_EL_COLUMN_SERVICE_ID,
    RTCOM_EL_COLUMN_EVENTTYPE_ID,
    RTCOM_EL_COLUMN_STORAGE_TIME,
    RTCOM_EL_COLUMN_START_TIME,
    RTCOM_EL_COLUMN_END_TIME,
    RTCOM_EL_COLUMN_IS_READ,
    RTCOM_EL_COLUMN_FLAGS,
    RTCOM_EL_COLUMN_BYTES_SENT,
    RTCOM_EL_COLUMN_BYTES_RECEIVED,
    RTCOM_EL_COLUMN_REMOTE_EBOOK_UID,
    RTCOM_EL_COLUMN_LOCAL_UID,
    RTCOM_EL_COLUMN_LOCAL_NAME,
    RTCOM_EL_COLUMN_REMOTE_UID,
    RTCOM_EL_COLUMN_REMOTE_NAME,
    RTCOM_EL_COLUMN_CHANNEL,
    RTCOM_EL_COLUMN_FREE_TEXT,
    RTCOM_EL_COLUMN_GROUP_UID,
    /* Joined */
    RTCOM_EL_COLUMN_SERVICE_NAME,
    RTCOM_EL_COLUMN_EVENT_TYPE_NAME
} RTComElColumn;

/**
 * Operations used when querying.
 */
typedef enum {
    RTCOM_EL_OP_EQUAL,         /** Test if operands are equal. */
    RTCOM_EL_OP_NOT_EQUAL,     /** Test if operands are different. */
    RTCOM_EL_OP_GREATER,       /** Test if the first operand is greater. */
    RTCOM_EL_OP_GREATER_EQUAL, /** Test if the first operand is greater or equal. */
    RTCOM_EL_OP_LESS,          /** Test if the first operand is smaller. */
    RTCOM_EL_OP_LESS_EQUAL,    /** Test if the first operand is smaller or equal. */
    RTCOM_EL_OP_IN_STRV,       /** Tests if the first operand is one of the strings in the array */
    RTCOM_EL_OP_STR_ENDS_WITH, /** Tests if the first operand (a string) ends with the given string.
                                   NOTE: not supported when querying for "service", "event-type". */
    RTCOM_EL_OP_STR_LIKE       /** Tests if the first operand (a string) is present
                                   NOTE: not supported when querying for "service", "event-type". Case-insensitive. */

} RTComElOp;

#define RTCOM_EL_FLAG_GENERIC_READ 1<<0

#endif

/* vim: set ai et tw=75 ts=4 sw=4: */

