// SPDX-License-Identifier: GPL-3.0-or-later

#include "plugin_logsmanagement.h"

static void logsmanagement_plugin_main_cleanup(void *ptr) {
    struct netdata_static_thread *static_thread = (struct netdata_static_thread *)ptr;
    static_thread->enabled = NETDATA_MAIN_THREAD_EXITING;

    info("cleaning up...");

    static_thread->enabled = NETDATA_MAIN_THREAD_EXITED;
}

void *logsmanagement_plugin_main(void *ptr){
	netdata_thread_cleanup_push(logsmanagement_plugin_main_cleanup, ptr);


	RRDSET *st = rrdset_create_localhost(
            "system"
            , "logsmanagement"
            , NULL
            , "logsmanagement"
            , NULL
            , "Logs Management main"
            , "units ??"
            , "idlejitter.plugin"
            , NULL
            , NETDATA_CHART_PRIO_SYSTEM_IDLEJITTER
            , localhost->rrd_update_every
            , RRDSET_TYPE_LINE
    );

    RRDDIM *rd_min = rrddim_add(st, "min", NULL, 1, 1, RRD_ALGORITHM_ABSOLUTE);

	while(!netdata_exit){
		rrdset_next(st);
		usec_t error_min = 100;
		rrddim_set_by_pointer(st, rd_min, error_min);
		rrdset_done(st);
	}

    netdata_thread_cleanup_pop(1);
    return NULL;
}