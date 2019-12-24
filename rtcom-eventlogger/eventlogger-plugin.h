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
 * @file eventlogger-plugin.h
 * @brief Header file used to create plugins.
 *
 * Include this file when developing plugins.
 */

#ifndef __EVENT_LOGGER_PLUGIN_H
#define __EVENT_LOGGER_PLUGIN_H

#include <glib.h>
#include <glib-object.h>
#include <gmodule.h>

#include "rtcom-eventlogger/eventlogger-iter.h"

typedef struct _RTComElService RTComElService;
typedef struct _RTComElEventType RTComElEventType;
typedef struct _RTComElFlag RTComElFlag;

/**
 * Describes a service.
 * This structure maps a service in the database.
 */
struct _RTComElService {
    guint id;        /** The id of the service. Can be queried as service-id. */
    gchar * name;    /** The name of the service. It's unique. */
    guint plugin_id; /** The id of the plugin that installed this service. */
    gchar * desc;    /** A short description of the service. */
};

/**
 * Describes an event-type.
 * This structure maps an event-type in the database.
 */
struct _RTComElEventType {
    guint id;        /** The id of the event-type. Can be queried as event-type-id. */
    gchar * name;    /** The name of the event-type. It's unique. */
    guint plugin_id; /** The id of the plugin that installed this event-type. */
    gchar * desc;    /** A short description of the event-type. */
};

/**
 * Describes a flag.
 * This structure maps a flag in the database.
 */
struct _RTComElFlag {
    guint id;         /** The id of the flag. */
    guint service_id; /** The id of the service this flag serves. */
    gchar * name;     /** The name of the flag. It's unique. */
    guint value;      /** The value of this flag. */
    gchar * desc;     /** A short description of the flag. */
};

typedef gboolean (*PluginInitFunc)(gpointer); /* Not mandatory */
typedef gchar * (*PluginNameFunc)();
typedef gchar * (*PluginDescFunc)();
typedef RTComElService * (*PluginServiceFunc)();
typedef GList * (*PluginEventTypesFunc)();
typedef GList * (*PluginFlagsFunc)();      /* Not mandatory */
typedef gboolean (*PluginGetValueFunc)(    /* Not mandatory */
        RTComElIter * it,
        const gchar *,
        GValue *);

typedef struct _RTComElPlugin RTComElPlugin;
struct _RTComElPlugin {
    GModule * module;
    guint id;
    PluginInitFunc init;
    PluginNameFunc get_name;
    PluginDescFunc get_desc;
    PluginServiceFunc get_service;
    PluginEventTypesFunc get_event_types;
    PluginFlagsFunc get_flags;
    PluginGetValueFunc get_value;
};

/**
 * Creates a new RTComElService.
 * Please free it with rtcom_el_service_free.
 * @see rtcom_el_service_free
 * @param name The name.
 * @param desc The description.
 * @returns The newly allocated RTComElService.
 */
RTComElService * rtcom_el_service_new(
        const gchar * name,
        const gchar * desc);

/**
 * Frees the memory for an RTComElService.
 * @param service The service to free.
 */
void rtcom_el_service_free(
        RTComElService * service);

/**
 * Creates a new RTComElEventType.
 * Please free it with rtcom_el_eventtype_free.
 * @see rtcom_el_eventtype_free
 * @param name The name.
 * @param desc The description.
 * @returns The newly allocated RTComElEventType.
 */
RTComElEventType * rtcom_el_eventtype_new(
        const gchar * name,
        const gchar * desc);

/**
 * Frees the memory for an RTComElEventType.
 * @param event_type The RTComElEventType to free.
 */
void rtcom_el_eventtype_free(
        RTComElEventType * event_type);

/**
 * Creates a new RTComElFlag.
 * Please free it with rtcom_el_flag_free.
 * @see rtcom_el_flag_free
 * @param name The name.
 * @param value The value.
 * @param desc The description.
 * @returns The newly allocated RTComElFlag.
 */
RTComElFlag * rtcom_el_flag_new(
        const gchar * name,
        guint value,
        const gchar * desc);

/**
 * Frees the memory for an RTComElFlag.
 * @param flag The RTComElFlag to free.
 */
void rtcom_el_flag_free(
        RTComElFlag * flag);

#endif

/* vim: set ai et tw=75 ts=4 sw=4: */

