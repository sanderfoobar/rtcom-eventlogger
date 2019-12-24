#include <glib.h>
#include <stdio.h>
#include <string.h>

#include "rtcom-eventlogger/eventlogger.h"

#define ENSURE_ARG(x) \
    do {\
        if(!x)\
        {\
            fprintf(stderr, "Argument %s is required for this command.\n", #x);\
            return -1;\
        }\
    } while(0);

int
main(int argc, char * argv[])
{
    RTComEl      * el         = NULL;
    RTComElEvent * ev         = NULL;

    /* Command line options */
    gchar * command           = NULL,
          * service           = NULL,
          * event_type        = NULL,
          * remote_ebook_uid  = NULL,
          * local_uid        = NULL,
          * local_name       = NULL,
          * remote_uid  = NULL,
          * remote_name = NULL,
          * channel           = NULL,
          * free_text         = NULL,
          * group_uid         = NULL;

    time_t  start_time        = 0,
            end_time          = 0;

    gint    flags             = 0,
            event_id          = 0;

    gchar * flag_value        = NULL,
          * vcard_field       = NULL;

    GOptionEntry options[] =
    {
        {"command", 0, 0, G_OPTION_ARG_STRING, &command, "Command", "[add|delete|set-flag|unset-flag|count]"},
        {"service", 0, G_OPTION_FLAG_OPTIONAL_ARG, G_OPTION_ARG_STRING, &service, "Service", "s"},
        {"event-type", 0, G_OPTION_FLAG_OPTIONAL_ARG, G_OPTION_ARG_STRING, &event_type, "Event type", "e"},
        {"start-time", 0, G_OPTION_FLAG_OPTIONAL_ARG, G_OPTION_ARG_INT, &start_time, "Start time", "t"},
        {"end-time", 0, G_OPTION_FLAG_OPTIONAL_ARG, G_OPTION_ARG_INT, &end_time, "End time", "t"},
        {"flags", 0, G_OPTION_FLAG_OPTIONAL_ARG, G_OPTION_ARG_INT, &flags, "Flags", "f"},
        {"remote-ebook-uid", 0, G_OPTION_FLAG_OPTIONAL_ARG, G_OPTION_ARG_STRING, &remote_ebook_uid, "Remote EBook UID", "uid"},
        {"local-uid", 0, G_OPTION_FLAG_OPTIONAL_ARG, G_OPTION_ARG_STRING, &local_uid, "Local UID", "uid"},
        {"local-name", 0, G_OPTION_FLAG_OPTIONAL_ARG, G_OPTION_ARG_STRING, &local_name, "Local  name", "name"},
        {"remote-uid", 0, G_OPTION_FLAG_OPTIONAL_ARG, G_OPTION_ARG_STRING, &remote_uid, "Remote UID", "uid"},
        {"remote-name", 0, G_OPTION_FLAG_OPTIONAL_ARG, G_OPTION_ARG_STRING, &remote_name, "Remote name", "name"},
        {"channel", 0, G_OPTION_FLAG_OPTIONAL_ARG, G_OPTION_ARG_STRING, &channel, "Channel", "text"},
        {"free-text", 0, G_OPTION_FLAG_OPTIONAL_ARG, G_OPTION_ARG_STRING, &free_text, "Free text", "text"},
        {"group-uid", 0, G_OPTION_FLAG_OPTIONAL_ARG, G_OPTION_ARG_STRING, &group_uid, "Group UID", "uid"},
        {"event-id", 0, G_OPTION_FLAG_OPTIONAL_ARG, G_OPTION_ARG_INT, &event_id, "Event ID", "id"},
        {"flag-value", 0, G_OPTION_FLAG_OPTIONAL_ARG, G_OPTION_ARG_STRING, &flag_value, "Flag value", "value"},
        {"with-vcard-field", 0, G_OPTION_FLAG_OPTIONAL_ARG, G_OPTION_ARG_STRING, &vcard_field, "VCard field", "value"},
        { NULL }
    };

    GOptionContext * ctx;

    ctx = g_option_context_new("- rtcom-eventlogger client");
    g_option_context_add_main_entries(ctx, options, NULL);
    g_option_context_parse(ctx, &argc, &argv, NULL);

    ENSURE_ARG(command);

    g_type_init();
    el = rtcom_el_new();
    if(!RTCOM_IS_EL(el))
    {
        fprintf(stderr, "Couldn't create RTComEl. Aborting.");
        return -1;
    }

    if(!strcmp("add", command))
    {
        ENSURE_ARG(service);
        ENSURE_ARG(event_type);

        ev = rtcom_el_event_new();
        if(!ev)
        {
            fprintf(stderr, "Couldn't create RTComElEvent. Aborting.");
            return -1;
        }

        RTCOM_EL_EVENT_SET_FIELD(ev, service, service);
        RTCOM_EL_EVENT_SET_FIELD(ev, event_type, event_type);
        RTCOM_EL_EVENT_SET_FIELD(ev, flags, flags);

        if(remote_ebook_uid) RTCOM_EL_EVENT_SET_FIELD(ev, remote_ebook_uid, remote_ebook_uid);
        if(local_uid)        RTCOM_EL_EVENT_SET_FIELD(ev, local_uid, local_uid);
        if(local_name)       RTCOM_EL_EVENT_SET_FIELD(ev, local_name, local_name);
        if(start_time)       RTCOM_EL_EVENT_SET_FIELD(ev, start_time, start_time);
        if(end_time)         RTCOM_EL_EVENT_SET_FIELD(ev, end_time, end_time);
        if(remote_uid)       RTCOM_EL_EVENT_SET_FIELD(ev, remote_uid, remote_uid);
        if(remote_name)      RTCOM_EL_EVENT_SET_FIELD(ev, remote_name, remote_name);
        if(channel)          RTCOM_EL_EVENT_SET_FIELD(ev, channel, channel);
        if(free_text)        RTCOM_EL_EVENT_SET_FIELD(ev, free_text, free_text);
        if(group_uid)        RTCOM_EL_EVENT_SET_FIELD(ev, group_uid, group_uid);

        event_id = rtcom_el_add_event(el, ev, NULL);
        if(event_id > 0)
        {
            g_message("Event added with id %d.", event_id);
            if(vcard_field)
                rtcom_el_add_header(el, event_id, "vcard-field", vcard_field, NULL);
        }
        else
        {
            fprintf(stderr, "Error adding event.");
            return -1;
        }
    }
    else if(!strcmp("delete", command))
    {
        gboolean ret = FALSE;

        if(event_id > 0)
            ret = rtcom_el_delete_event(el, event_id, NULL);
        else if(service)
            ret = rtcom_el_delete_by_service(el, service);
        else
            ret = rtcom_el_delete_all(el);

        g_message("Action %s.", ret ? "succedeed" : "failed");
    }
    else if(!strcmp("set-flag", command))
    {
        ENSURE_ARG(event_id);
        ENSURE_ARG(flag_value);

        rtcom_el_set_event_flag(el, event_id, flag_value, NULL);
    }
    else if(!strcmp("unset-flag", command))
    {
        ENSURE_ARG(event_id);
        ENSURE_ARG(flag_value);

        rtcom_el_unset_event_flag(el, event_id, flag_value, NULL);
    }
    else if(!strcmp("count", command))
    {
        gint n;

        n = rtcom_el_count_by_service(el, service);
        g_message("Number of events of service %s: %d.", service, n);
    }

    g_object_unref(el);
    g_option_context_free(ctx);

    return 0;
}

/* vim: set ai et tw=75 ts=4 sw=4: */

