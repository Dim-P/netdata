// SPDX-License-Identifier: GPL-3.0-or-later

#include "plugin_logsmanagement.h"
#include "../../logsmanagement/file_info.h"

static struct Chart_data{
    char * rrd_type;

    RRDSET *st_lines;
    RRDDIM *dim_lines;
    collected_number num_lines;

    RRDSET *st_req_methods;
    RRDDIM *dim_req_method_get, *dim_req_method_post;
    collected_number num_req_method_get, num_req_method_post;

    RRDSET *st_resp_code_family;
    RRDDIM *dim_resp_code_1xx, *dim_resp_code_2xx;
    collected_number num_resp_code_1xx, num_resp_code_2xx;
};

struct Chart_data **chart_data_arr;

static void logsmanagement_plugin_main_cleanup(void *ptr) {
    struct netdata_static_thread *static_thread = (struct netdata_static_thread *)ptr;
    static_thread->enabled = NETDATA_MAIN_THREAD_EXITING;

    info("cleaning up...");

    static_thread->enabled = NETDATA_MAIN_THREAD_EXITED;
}


void *logsmanagement_plugin_main(void *ptr){
	netdata_thread_cleanup_push(logsmanagement_plugin_main_cleanup, ptr);

    while(!p_file_infos_arr) sleep_usec(100000); // wait for p_file_infos_arr initialisation

    chart_data_arr = callocz(1, p_file_infos_arr->count * sizeof(struct Chart_data *));

    for(int i = 0; i < p_file_infos_arr->count; i++){
        struct File_info *p_file_info = p_file_infos_arr->data[i];
        if(!p_file_info->parser_config){ // Check if there is parser configuration to be used for chart generation
            chart_data_arr[i] = NULL;
            continue; 
        } 

        chart_data_arr[i] = callocz(1, sizeof(struct Chart_data));
        chart_data_arr[i]->rrd_type = p_file_info->filename;



        /* Number of lines - initialise */
    	chart_data_arr[i]->st_lines = rrdset_create_localhost(
                chart_data_arr[i]->rrd_type
                , "total lines parsed"
                , NULL
                , "total lines parsed"
                , NULL
                , "Lines collected (total)"
                , "lines/s"
                , "logsmanagement.plugin"
                , NULL
                , 132200
                , localhost->rrd_update_every
                , RRDSET_TYPE_AREA
        );
        chart_data_arr[i]->dim_lines = rrddim_add(chart_data_arr[i]->st_lines, "lines/s", NULL, 1, 1, RRD_ALGORITHM_ABSOLUTE);

        /* Request methods - initialise */
        chart_data_arr[i]->st_req_methods = rrdset_create_localhost(
                chart_data_arr[i]->rrd_type
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
        chart_data_arr[i]->dim_req_method_get = rrddim_add(chart_data_arr[i]->st_req_methods, "GET", NULL, 1, 1, RRD_ALGORITHM_ABSOLUTE);
        chart_data_arr[i]->dim_req_method_post = rrddim_add(chart_data_arr[i]->st_req_methods, "POST", NULL, 1, 1, RRD_ALGORITHM_ABSOLUTE);

        /* Response code family - initialise */
        chart_data_arr[i]->st_resp_code_family = rrdset_create_localhost(
                chart_data_arr[i]->rrd_type
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
        chart_data_arr[i]->dim_resp_code_1xx = rrddim_add(chart_data_arr[i]->st_resp_code_family, "1xx", NULL, 1, 1, RRD_ALGORITHM_ABSOLUTE);
        chart_data_arr[i]->dim_resp_code_2xx = rrddim_add(chart_data_arr[i]->st_resp_code_family, "2xx", NULL, 1, 1, RRD_ALGORITHM_ABSOLUTE);
        


        uv_mutex_lock(p_file_info->parser_mut);

        /* Number of lines - collect first time */
        chart_data_arr[i]->num_lines = p_file_info->parser_metrics->num_lines;
        p_file_info->parser_metrics->num_lines = 0;

        /* Request methods - collect first time */
        chart_data_arr[i]->num_req_method_get = p_file_info->parser_metrics->req_method.get;
        p_file_info->parser_metrics->req_method.get = 0;
        chart_data_arr[i]->num_req_method_post = p_file_info->parser_metrics->req_method.post;
        p_file_info->parser_metrics->req_method.post = 0;

        /* Response code family - collect first time */
        chart_data_arr[i]->num_resp_code_1xx = p_file_info->parser_metrics->resp_code_family.resp_1xx;
        p_file_info->parser_metrics->resp_code_family.resp_1xx = 0;
        chart_data_arr[i]->num_resp_code_2xx = p_file_info->parser_metrics->resp_code_family.resp_2xx;
        p_file_info->parser_metrics->resp_code_family.resp_2xx = 0;
        
        uv_mutex_unlock(p_file_info->parser_mut);



        /* Number of lines - update chart first time */
        rrddim_set_by_pointer(chart_data_arr[i]->st_lines, chart_data_arr[i]->dim_lines, chart_data_arr[i]->num_lines);
    	rrdset_done(chart_data_arr[i]->st_lines);

        /* Request methods - update chart first time */
        rrddim_set_by_pointer(chart_data_arr[i]->st_req_methods, chart_data_arr[i]->dim_req_method_get, chart_data_arr[i]->num_req_method_get);
        rrddim_set_by_pointer(chart_data_arr[i]->st_req_methods, chart_data_arr[i]->dim_req_method_post, chart_data_arr[i]->num_req_method_post);
        rrdset_done(chart_data_arr[i]->st_req_methods);

        /* Response code family - update chart first time */
        rrddim_set_by_pointer(chart_data_arr[i]->st_resp_code_family, chart_data_arr[i]->dim_resp_code_1xx, chart_data_arr[i]->num_resp_code_1xx);
        rrddim_set_by_pointer(chart_data_arr[i]->st_resp_code_family, chart_data_arr[i]->dim_resp_code_2xx, chart_data_arr[i]->num_resp_code_2xx);
        rrdset_done(chart_data_arr[i]->st_resp_code_family);
        
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

            /* Number of lines - collect */
            chart_data_arr[i]->num_lines += p_file_info->parser_metrics->num_lines;
            p_file_info->parser_metrics->num_lines = 0;

            /* Request methods - collect */
            chart_data_arr[i]->num_req_method_get = p_file_info->parser_metrics->req_method.get;
            p_file_info->parser_metrics->req_method.get = 0;
            chart_data_arr[i]->num_req_method_post = p_file_info->parser_metrics->req_method.post;
            p_file_info->parser_metrics->req_method.post = 0;

            /* Response code family - collect */
            chart_data_arr[i]->num_resp_code_1xx = p_file_info->parser_metrics->resp_code_family.resp_1xx;
            p_file_info->parser_metrics->resp_code_family.resp_1xx = 0;
            chart_data_arr[i]->num_resp_code_2xx = p_file_info->parser_metrics->resp_code_family.resp_2xx;
            p_file_info->parser_metrics->resp_code_family.resp_2xx = 0;
            
            uv_mutex_unlock(p_file_info->parser_mut);



            /* Number of lines - update chart */
    		rrdset_next(chart_data_arr[i]->st_lines);
    		rrddim_set_by_pointer(chart_data_arr[i]->st_lines, chart_data_arr[i]->dim_lines, chart_data_arr[i]->num_lines);
            rrdset_done(chart_data_arr[i]->st_lines);

            /* Request methods - update chart */
            rrdset_next(chart_data_arr[i]->st_req_methods);
            rrddim_set_by_pointer(chart_data_arr[i]->st_req_methods, chart_data_arr[i]->dim_req_method_get, chart_data_arr[i]->num_req_method_get);
            rrddim_set_by_pointer(chart_data_arr[i]->st_req_methods, chart_data_arr[i]->dim_req_method_post, chart_data_arr[i]->num_req_method_post);
            rrdset_done(chart_data_arr[i]->st_req_methods);

            /* Response code family - update chart */
            rrdset_next(chart_data_arr[i]->st_resp_code_family);
            rrddim_set_by_pointer(chart_data_arr[i]->st_resp_code_family, chart_data_arr[i]->dim_resp_code_1xx, chart_data_arr[i]->num_resp_code_1xx);
            rrddim_set_by_pointer(chart_data_arr[i]->st_resp_code_family, chart_data_arr[i]->dim_resp_code_2xx, chart_data_arr[i]->num_resp_code_2xx);
            rrdset_done(chart_data_arr[i]->st_resp_code_family);
            
        }
	}

    netdata_thread_cleanup_pop(1);
    return NULL;
}