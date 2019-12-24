#ifndef __EVENTLOGGER_MARSHALERS_H__
#define __EVENTLOGGER_MARSHALERS_H__

#include <glib-object.h>

G_BEGIN_DECLS

extern void rtcom_el_cclosure_marshal_VOID__INT_STRING_STRING_STRING_STRING_STRING(
        GClosure     *closure,
        GValue       *return_value,
        guint         n_param_values,
        const GValue *param_values,
        gpointer      invocation_hint,
        gpointer      marshal_data);

G_END_DECLS

#endif

/* vim: set ai et tw=75 ts=4 sw=4: */

