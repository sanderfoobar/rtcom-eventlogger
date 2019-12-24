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

#include "rtcom-eventlogger/eventlogger-plugin.h"

RTComElService * rtcom_el_service_new(
        const gchar * name,
        const gchar * desc)
{
    RTComElService * service = g_new0(RTComElService, 1);
    g_return_val_if_fail(service, NULL);

    service->name = g_strdup(name);
    if(!service->name)
    {
        g_free(service);
        return NULL;
    }

    service->desc = g_strdup(desc);
    if(!service->desc)
    {
        g_free(service->name);
        g_free(service);
        return NULL;
    }

    return service;
}

void rtcom_el_service_free(
        RTComElService * service)
{
    g_return_if_fail(service);

    g_free(service->name);
    g_free(service->desc);
    g_free(service);
}

RTComElEventType * rtcom_el_eventtype_new(
        const gchar * name,
        const gchar * desc)
{
    RTComElEventType * event_type = g_new0(RTComElEventType, 1);
    g_return_val_if_fail(event_type, NULL);

    event_type->name = g_strdup(name);
    if(!event_type->name)
    {
        g_free(event_type);
        return NULL;
    }

    event_type->desc = g_strdup(desc);
    if(!event_type->desc)
    {
        g_free(event_type->name);
        g_free(event_type);
        return NULL;
    }

    return event_type;
}

void rtcom_el_eventtype_free(
        RTComElEventType * event_type)
{
    g_return_if_fail(event_type);

    g_free(event_type->name);
    g_free(event_type->desc);
    g_free(event_type);
}

RTComElFlag * rtcom_el_flag_new(
        const gchar * name,
        guint value,
        const gchar * desc)
{
    RTComElFlag * flag = g_new0(RTComElFlag, 1);
    g_return_val_if_fail(flag, NULL);

    flag->name = g_strdup(name);
    if(!flag->name)
    {
        g_free(flag);
        return NULL;
    }

    flag->value = value;

    flag->desc = g_strdup(desc);
    if(!flag->desc)
    {
        g_free(flag->name);
        g_free(flag);
        return NULL;
    }

    return flag;
}

void rtcom_el_flag_free(
        RTComElFlag * flag)
{
    g_return_if_fail(flag);

    g_free(flag->name);
    g_free(flag->desc);
    g_free(flag);
}

/* vim: set ai et tw=75 ts=4 sw=4: */

