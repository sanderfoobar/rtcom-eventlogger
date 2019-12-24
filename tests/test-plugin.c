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

#include <glib.h>
#include <glib-object.h>
#include <gmodule.h>
#include <string.h>

#include "rtcom-eventlogger/eventlogger-plugin.h"

#define PLUGIN_NAME  "TEST"
#define PLUGIN_DESC  "Test plugin"
#define SERVICE_NAME "RTCOM_EL_SERVICE_TEST"
#define SERVICE_DESC "Service for testing framework's functionalities."

const gchar * g_module_check_init(
        GModule * module)
{
    g_message("Plugin registered: %s.", PLUGIN_NAME);
    return NULL; /* NULL means success */
}

const gchar * rtcom_el_plugin_name(void)
{
    return PLUGIN_NAME;
}

void rtcom_el_plugin_init(void)
{
}

const gchar * rtcom_el_plugin_desc(void)
{
    return PLUGIN_DESC;
}

RTComElService * rtcom_el_plugin_service(void)
{
    RTComElService * service = rtcom_el_service_new(
            SERVICE_NAME,
            SERVICE_DESC);
    return service;
}

GList * rtcom_el_plugin_eventtypes(void)
{
    GList * event_types = NULL;

    event_types = g_list_append(
            event_types,
            rtcom_el_eventtype_new(
                "RTCOM_EL_EVENTTYPE_TEST_ET1",
                "Some event type."));
    event_types = g_list_append(
            event_types,
            rtcom_el_eventtype_new(
                "RTCOM_EL_EVENTTYPE_TEST_ET2",
                "Some other event type."));

    event_types = g_list_first(event_types);

    return event_types;
}

GList * rtcom_el_plugin_flags(void)
{
    GList * flags = NULL;

    flags = g_list_append(
            flags,
            rtcom_el_flag_new(
                "RTCOM_EL_FLAG_TEST_FLAG1",
                2,
                "Some flag"));
    flags = g_list_append(
            flags,
            rtcom_el_flag_new(
                "RTCOM_EL_FLAG_TEST_FLAG2",
                4,
                "Some other flag"));
    flags = g_list_append(
            flags,
            rtcom_el_flag_new(
                "RTCOM_EL_FLAG_TEST_FLAG3",
                8,
                "Even some another flag"));

    flags = g_list_first(flags);

    return flags;
}

gboolean rtcom_el_plugin_get_value(
        RTComElIter * it,
        const gchar * item,
        GValue * value)
{
    gchar * header_value = NULL;

    g_return_val_if_fail(it, FALSE);
    g_return_val_if_fail(item, FALSE);
    g_return_val_if_fail(value, FALSE);

    GString * add = NULL;

    GString * markup = NULL;
    GValue id = {0};
    GValue local_name = {0};
    GValue local_uid = {0};

    if(!strcmp(item, "Foo"))
    {
        header_value = rtcom_el_iter_get_header_raw(
                it,
                item);

        g_value_init(value, G_TYPE_STRING);

        if(header_value)
        {
            g_value_set_string(value, header_value);
            g_free(header_value);
            return TRUE;
        }
        else
        {
            g_debug("Plugin %s couldn't find item %s", PLUGIN_NAME, item);
            g_value_set_static_string(value, NULL);
            return TRUE;
        }
    }
    else if(!strcmp(item, "additional-text"))
    {
        add = g_string_new("");
        rtcom_el_iter_get_raw(it, "id", &id);
        g_string_append_printf(add, "%d", g_value_get_int(&id));
        g_value_unset(&id);
        g_string_append(add, ": Hello from the Test plugin!");

        g_value_init(value, G_TYPE_STRING);
        g_value_set_string(value, add->str);

        g_string_free(add, TRUE);
        return TRUE;
    }
    else if(!strcmp(item, "pango-markup"))
    {
        markup = g_string_new("");
        rtcom_el_iter_get_raw(it, "id", &id);
        rtcom_el_iter_get_raw(it, "local-name", &local_name);
        rtcom_el_iter_get_raw(it, "local-uid", &local_uid);
        g_string_append_printf(
                markup,
                "%d: <b>%s</b>\n<small>Hello from the Test plugin! UID: %s</small>",
                g_value_get_int(&id),
                g_value_get_string(&local_name),
                g_value_get_string(&local_uid));

        g_value_unset(&local_name);
        g_value_unset(&local_uid);

        g_value_init(value, G_TYPE_STRING);
        g_value_set_string(value, markup->str);

        g_string_free(markup, TRUE);
        return TRUE;
    }
    else if(!strcmp(item, "icon-path"))
    {
        g_value_init(value, G_TYPE_STRING);
        g_value_set_string(value, "");
        return TRUE;
    }

    return FALSE;
}

/* vim: set ai et tw=75 ts=4 sw=4: */

