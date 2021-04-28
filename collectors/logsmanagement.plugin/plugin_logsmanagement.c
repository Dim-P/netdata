// SPDX-License-Identifier: GPL-3.0-or-later

#include "plugin_logsmanagement.h"
#include "../../logsmanagement/file_info.h"

#define RRD_TYPE_LOGS_MANAGEMENT "Logs Management"

struct

static void logsmanagement_plugin_main_cleanup(void *ptr) {
    struct netdata_static_thread *static_thread = (struct netdata_static_thread *)ptr;
    static_thread->enabled = NETDATA_MAIN_THREAD_EXITING;

    info("cleaning up...");

    static_thread->enabled = NETDATA_MAIN_THREAD_EXITED;
}


void *logsmanagement_plugin_main(void *ptr){
	netdata_thread_cleanup_push(logsmanagement_plugin_main_cleanup, ptr);

    while(!p_file_infos_arr) sleep_usec(100000); // wait for p_file_infos_arr initialisation

    //struct File_info *p_file_info = p_file_infos_arr->data[0];

    for(int i = 0; i < p_file_infos_arr->count; i++){
        struct File_info *p_file_info = p_file_infos_arr->data[i];
        if(!p_file_info->parser_config) continue; // Check if there is parser configuration to be used for chart generation

    	RRDSET *st_lines = rrdset_create_localhost(
                RRD_TYPE_LOGS_MANAGEMENT
                , "logsmanagement"
                , NULL
                , "total lines parsed"
                , NULL
                , "Logs Management main"
                , "lines/s"
                , "logsmanagement.plugin"
                , NULL
                , 132200
                , localhost->rrd_update_every
                , RRDSET_TYPE_AREA
        );
        RRDDIM *lines_per_sec = rrddim_add(st_lines, "lines/s", NULL, 1, 1, RRD_ALGORITHM_ABSOLUTE);

        RRDSET *st_req_methods = rrdset_create_localhost(
                RRD_TYPE_LOGS_MANAGEMENT
                , "http methods"
                , NULL
                , "http methods"
                , NULL
                , "Requests Per HTTP Method"
                , "requests/s"
                , "logsmanagement.plugin"
                , NULL
                , 132200
                , localhost->rrd_update_every
                , RRDSET_TYPE_AREA
        );
        RRDDIM *req_method_get_per_sec = rrddim_add(st_req_methods, "GET", NULL, 1, 1, RRD_ALGORITHM_ABSOLUTE);
        RRDDIM *req_method_post_per_sec = rrddim_add(st_req_methods, "POST", NULL, 1, 1, RRD_ALGORITHM_ABSOLUTE);

        RRDSET *st_resp_code_family = rrdset_create_localhost(
                RRD_TYPE_LOGS_MANAGEMENT
                , "responses"
                , NULL
                , "responses"
                , NULL
                , "Response Codes"
                , "requests/s"
                , "logsmanagement.plugin"
                , NULL
                , 132200
                , localhost->rrd_update_every
                , RRDSET_TYPE_AREA
        );
        RRDDIM *resp_code_1xx_per_sec = rrddim_add(st_resp_code_family, "1xx", NULL, 1, 1, RRD_ALGORITHM_ABSOLUTE);
        RRDDIM *resp_code_2xx_per_sec = rrddim_add(st_resp_code_family, "2xx", NULL, 1, 1, RRD_ALGORITHM_ABSOLUTE);
        
        

        uv_mutex_lock(p_file_info->parser_mut);

        collected_number num_lines = p_file_info->parser_metrics->num_lines;
        p_file_info->parser_metrics->num_lines = 0;

        collected_number req_method_get = p_file_info->parser_metrics->req_method.get;
        p_file_info->parser_metrics->req_method.get = 0;
        collected_number req_method_post = p_file_info->parser_metrics->req_method.post;
        p_file_info->parser_metrics->req_method.post = 0;

        collected_number resp_code_1xx = p_file_info->parser_metrics->resp_code_family.resp_1xx;
        p_file_info->parser_metrics->resp_code_family.resp_1xx = 0;
        collected_number resp_code_2xx = p_file_info->parser_metrics->resp_code_family.resp_2xx;
        p_file_info->parser_metrics->resp_code_family.resp_2xx = 0;

        uv_mutex_unlock(p_file_info->parser_mut);



        rrddim_set_by_pointer(st_lines, lines_per_sec, num_lines);
    	rrdset_done(st_lines);

        rrddim_set_by_pointer(st_req_methods, req_method_get_per_sec, req_method_get);
        rrddim_set_by_pointer(st_req_methods, req_method_post_per_sec, req_method_post);
        rrdset_done(st_req_methods);

        rrddim_set_by_pointer(st_resp_code_family, resp_code_1xx_per_sec, resp_code_1xx);
        rrddim_set_by_pointer(st_resp_code_family, resp_code_2xx_per_sec, resp_code_2xx);
        rrdset_done(st_resp_code_family);
    }



    usec_t step = localhost->rrd_update_every * USEC_PER_SEC;
    heartbeat_t hb;
    heartbeat_init(&hb);

	while(!netdata_exit){

        usec_t hb_dt = heartbeat_next(&hb, step);

        if(unlikely(netdata_exit)) break;

        for(int i = 0; i < p_file_infos_arr->count; i++){
            struct File_info *p_file_info = p_file_infos_arr->data[i];
            if(!p_file_info->parser_config) continue; // Check if there is parser configuration to be used for chart generation

    		uv_mutex_lock(p_file_info->parser_mut);

            num_lines += p_file_info->parser_metrics->num_lines;
            p_file_info->parser_metrics->num_lines = 0;

            req_method_get = p_file_info->parser_metrics->req_method.get;
            p_file_info->parser_metrics->req_method.get = 0;
            req_method_post = p_file_info->parser_metrics->req_method.post;
            p_file_info->parser_metrics->req_method.post = 0;

            resp_code_1xx = p_file_info->parser_metrics->resp_code_family.resp_1xx;
            p_file_info->parser_metrics->resp_code_family.resp_1xx = 0;
            resp_code_2xx = p_file_info->parser_metrics->resp_code_family.resp_2xx;
            p_file_info->parser_metrics->resp_code_family.resp_2xx = 0;

            uv_mutex_unlock(p_file_info->parser_mut);




    		rrdset_next(st_lines);
    		rrddim_set_by_pointer(st_lines, lines_per_sec, num_lines);
    		rrdset_done(st_lines);

            rrdset_next(st_req_methods);
            rrddim_set_by_pointer(st_req_methods, req_method_get_per_sec, req_method_get);
            rrddim_set_by_pointer(st_req_methods, req_method_post_per_sec, req_method_post);
            rrdset_done(st_req_methods);

            rrdset_next(st_resp_code_family);
            rrddim_set_by_pointer(st_resp_code_family, resp_code_1xx_per_sec, resp_code_1xx);
            rrddim_set_by_pointer(st_resp_code_family, resp_code_2xx_per_sec, resp_code_2xx);
            rrdset_done(st_resp_code_family);
        }
	}

    netdata_thread_cleanup_pop(1);
    return NULL;
}