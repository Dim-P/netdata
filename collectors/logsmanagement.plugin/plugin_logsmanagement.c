// SPDX-License-Identifier: GPL-3.0-or-later

#include "plugin_logsmanagement.h"
#include "../../logsmanagement/file_info.h"

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
            , "logsmanagement.plugin"
            , NULL
            , 132200
            , localhost->rrd_update_every
            , RRDSET_TYPE_AREA
    );

    RRDDIM *rd_min = rrddim_add(st, "min", NULL, 1, 1, RRD_ALGORITHM_INCREMENTAL);

    while(!p_file_infos_arr) sleep_usec(100000); // wait for p_file_infos_arr initialisation

    struct File_info *p_file_info = p_file_infos_arr->data[0];

    uv_mutex_lock(p_file_info->parser_mut);
    collected_number num_lines = p_file_info->parser_metrics->num_lines;
    p_file_info->parser_metrics->num_lines = 0;
    uv_mutex_unlock(p_file_info->parser_mut);

    rrddim_set_by_pointer(st, rd_min, num_lines);
	rrdset_done(st);

	while(!netdata_exit){
		uv_mutex_lock(p_file_info->parser_mut);
        num_lines = p_file_info->parser_metrics->num_lines;
        p_file_info->parser_metrics->num_lines = 0;
        uv_mutex_unlock(p_file_info->parser_mut);
		rrdset_next(st);
		rrddim_set_by_pointer(st, rd_min, num_lines);
		rrdset_done(st);
		sleep_usec(1000000);
	}

    netdata_thread_cleanup_pop(1);
    return NULL;
}