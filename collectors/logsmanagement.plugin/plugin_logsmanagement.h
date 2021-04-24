// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef NETDATA_PLUGIN_LOGSMANAGEMENT_H
#define NETDATA_PLUGIN_LOGSMANAGEMENT_H 1

#include "../../daemon/common.h"

#define NETDATA_PLUGIN_HOOK_LOGSMANAGEMENT \
    { \
        .name = "PLUGIN[logsmanagement]", \
        .config_section = LOG_MANAGEMENT_CONF, \
        .config_name = NULL, \
        .enabled = 1, \
        .thread = NULL, \
        .init_routine = NULL, \
        .start_routine = logsmanagement_plugin_main \
    },


extern void *logsmanagement_plugin_main(void *ptr);

#endif /* NETDATA_PLUGIN_LOGSMANAGEMENT_H */