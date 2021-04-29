// SPDX-License-Identifier: GPL-3.0-or-later

#include "plugin_logsmanagement.h"
#include "../../logsmanagement/file_info.h"

struct Chart_data{
    char * rrd_type;

    /* Number of lines */
    RRDSET *st_lines;
    RRDDIM *dim_lines;
    collected_number num_lines;

    /* Request methods */
    RRDSET *st_req_methods;
    RRDDIM *dim_req_method_acl, *dim_req_method_baseline_control, *dim_req_method_bind, *dim_req_method_checkin, *dim_req_method_checkout,
    *dim_req_method_connect, *dim_req_method_copy, *dim_req_method_delet, *dim_req_method_get, *dim_req_method_head, *dim_req_method_label,
    *dim_req_method_link, *dim_req_method_lock, *dim_req_method_merge, *dim_req_method_mkactivity, *dim_req_method_mkcalendar, *dim_req_method_mkcol,
    *dim_req_method_mkredirectref, *dim_req_method_mkworkspace, *dim_req_method_move, *dim_req_method_options, *dim_req_method_orderpatch, 
    *dim_req_method_patch, *dim_req_method_post, *dim_req_method_pri, *dim_req_method_propfind, *dim_req_method_proppatch, *dim_req_method_put,
    *dim_req_method_rebind, *dim_req_method_report, *dim_req_method_search, *dim_req_method_trace, *dim_req_method_unbind, *dim_req_method_uncheckout,
    *dim_req_method_unlink, *dim_req_method_unlock, *dim_req_method_update, *dim_req_method_updateredirectref;
    
    collected_number num_req_method_acl, num_req_method_baseline_control, num_req_method_bind, num_req_method_checkin, num_req_method_checkout,
    num_req_method_connect, num_req_method_copy, num_req_method_delet, num_req_method_get, num_req_method_head, num_req_method_label, 
    num_req_method_link, num_req_method_lock, num_req_method_merge, num_req_method_mkactivity, num_req_method_mkcalendar, num_req_method_mkcol,
    num_req_method_mkredirectref, num_req_method_mkworkspace, num_req_method_move, num_req_method_options, num_req_method_orderpatch, 
    num_req_method_patch, num_req_method_post, num_req_method_pri, num_req_method_propfind, num_req_method_proppatch, num_req_method_put,
    num_req_method_rebind, num_req_method_report, num_req_method_search, num_req_method_trace, num_req_method_unbind, num_req_method_uncheckout,
    num_req_method_unlink, num_req_method_unlock, num_req_method_update, num_req_method_updateredirectref;

    /* Response code family */
    RRDSET *st_resp_code_family;
    RRDDIM *dim_resp_code_1xx, *dim_resp_code_2xx, *dim_resp_code_3xx, *dim_resp_code_4xx, *dim_resp_code_5xx, *dim_resp_code_other;
    collected_number num_resp_code_1xx, num_resp_code_2xx, num_resp_code_3xx, num_resp_code_4xx, num_resp_code_5xx, num_resp_code_other;

    /* Response code */
    RRDSET *st_resp_code;
    RRDDIM *dim_resp_code[501];
    collected_number num_resp_code[501];
};

static struct Chart_data **chart_data_arr;

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
        chart_data_arr[i]->dim_req_method_acl = rrddim_add(chart_data_arr[i]->st_req_methods, "ACL", NULL, 1, 1, RRD_ALGORITHM_ABSOLUTE);
        chart_data_arr[i]->dim_req_method_baseline_control = rrddim_add(chart_data_arr[i]->st_req_methods, "BASELINE-CONTROL", NULL, 1, 1, RRD_ALGORITHM_ABSOLUTE);
        chart_data_arr[i]->dim_req_method_bind = rrddim_add(chart_data_arr[i]->st_req_methods, "BIND", NULL, 1, 1, RRD_ALGORITHM_ABSOLUTE);
        chart_data_arr[i]->dim_req_method_checkin = rrddim_add(chart_data_arr[i]->st_req_methods, "CHECKIN", NULL, 1, 1, RRD_ALGORITHM_ABSOLUTE);
        chart_data_arr[i]->dim_req_method_checkout = rrddim_add(chart_data_arr[i]->st_req_methods, "CHECKOUT", NULL, 1, 1, RRD_ALGORITHM_ABSOLUTE);
        chart_data_arr[i]->dim_req_method_connect = rrddim_add(chart_data_arr[i]->st_req_methods, "CONNECT", NULL, 1, 1, RRD_ALGORITHM_ABSOLUTE);
        chart_data_arr[i]->dim_req_method_copy = rrddim_add(chart_data_arr[i]->st_req_methods, "COPY", NULL, 1, 1, RRD_ALGORITHM_ABSOLUTE);
        chart_data_arr[i]->dim_req_method_delet = rrddim_add(chart_data_arr[i]->st_req_methods, "DELETE", NULL, 1, 1, RRD_ALGORITHM_ABSOLUTE);
        chart_data_arr[i]->dim_req_method_get = rrddim_add(chart_data_arr[i]->st_req_methods, "GET", NULL, 1, 1, RRD_ALGORITHM_ABSOLUTE);
        chart_data_arr[i]->dim_req_method_head = rrddim_add(chart_data_arr[i]->st_req_methods, "HEAD", NULL, 1, 1, RRD_ALGORITHM_ABSOLUTE);
        chart_data_arr[i]->dim_req_method_label = rrddim_add(chart_data_arr[i]->st_req_methods, "LABEL", NULL, 1, 1, RRD_ALGORITHM_ABSOLUTE);
        chart_data_arr[i]->dim_req_method_link = rrddim_add(chart_data_arr[i]->st_req_methods, "LINK", NULL, 1, 1, RRD_ALGORITHM_ABSOLUTE);
        chart_data_arr[i]->dim_req_method_lock = rrddim_add(chart_data_arr[i]->st_req_methods, "LOCK", NULL, 1, 1, RRD_ALGORITHM_ABSOLUTE);
        chart_data_arr[i]->dim_req_method_merge = rrddim_add(chart_data_arr[i]->st_req_methods, "MERGE", NULL, 1, 1, RRD_ALGORITHM_ABSOLUTE);
        chart_data_arr[i]->dim_req_method_mkactivity = rrddim_add(chart_data_arr[i]->st_req_methods, "MKACTIVITY", NULL, 1, 1, RRD_ALGORITHM_ABSOLUTE);
        chart_data_arr[i]->dim_req_method_mkcalendar = rrddim_add(chart_data_arr[i]->st_req_methods, "MKCALENDAR", NULL, 1, 1, RRD_ALGORITHM_ABSOLUTE);
        chart_data_arr[i]->dim_req_method_mkcol = rrddim_add(chart_data_arr[i]->st_req_methods, "MKCOL", NULL, 1, 1, RRD_ALGORITHM_ABSOLUTE);
        chart_data_arr[i]->dim_req_method_mkredirectref = rrddim_add(chart_data_arr[i]->st_req_methods, "MKREDIRECTREF", NULL, 1, 1, RRD_ALGORITHM_ABSOLUTE);
        chart_data_arr[i]->dim_req_method_mkworkspace = rrddim_add(chart_data_arr[i]->st_req_methods, "MKWORKSPACE", NULL, 1, 1, RRD_ALGORITHM_ABSOLUTE);
        chart_data_arr[i]->dim_req_method_move = rrddim_add(chart_data_arr[i]->st_req_methods, "MOVE", NULL, 1, 1, RRD_ALGORITHM_ABSOLUTE);
        chart_data_arr[i]->dim_req_method_options = rrddim_add(chart_data_arr[i]->st_req_methods, "OPTIONS", NULL, 1, 1, RRD_ALGORITHM_ABSOLUTE);
        chart_data_arr[i]->dim_req_method_orderpatch = rrddim_add(chart_data_arr[i]->st_req_methods, "ORDERPATCH", NULL, 1, 1, RRD_ALGORITHM_ABSOLUTE);
        chart_data_arr[i]->dim_req_method_patch = rrddim_add(chart_data_arr[i]->st_req_methods, "PATCH", NULL, 1, 1, RRD_ALGORITHM_ABSOLUTE);
        chart_data_arr[i]->dim_req_method_post = rrddim_add(chart_data_arr[i]->st_req_methods, "POST", NULL, 1, 1, RRD_ALGORITHM_ABSOLUTE);
        chart_data_arr[i]->dim_req_method_pri = rrddim_add(chart_data_arr[i]->st_req_methods, "PRI", NULL, 1, 1, RRD_ALGORITHM_ABSOLUTE);
        chart_data_arr[i]->dim_req_method_propfind = rrddim_add(chart_data_arr[i]->st_req_methods, "PROPFIND", NULL, 1, 1, RRD_ALGORITHM_ABSOLUTE);
        chart_data_arr[i]->dim_req_method_proppatch = rrddim_add(chart_data_arr[i]->st_req_methods, "PROPPATCH", NULL, 1, 1, RRD_ALGORITHM_ABSOLUTE);
        chart_data_arr[i]->dim_req_method_put = rrddim_add(chart_data_arr[i]->st_req_methods, "PUT", NULL, 1, 1, RRD_ALGORITHM_ABSOLUTE);
        chart_data_arr[i]->dim_req_method_rebind = rrddim_add(chart_data_arr[i]->st_req_methods, "REBIND", NULL, 1, 1, RRD_ALGORITHM_ABSOLUTE);
        chart_data_arr[i]->dim_req_method_report = rrddim_add(chart_data_arr[i]->st_req_methods, "REPORT", NULL, 1, 1, RRD_ALGORITHM_ABSOLUTE);
        chart_data_arr[i]->dim_req_method_search = rrddim_add(chart_data_arr[i]->st_req_methods, "SEARCH", NULL, 1, 1, RRD_ALGORITHM_ABSOLUTE);
        chart_data_arr[i]->dim_req_method_trace = rrddim_add(chart_data_arr[i]->st_req_methods, "TRACE", NULL, 1, 1, RRD_ALGORITHM_ABSOLUTE);
        chart_data_arr[i]->dim_req_method_unbind = rrddim_add(chart_data_arr[i]->st_req_methods, "UNBIND", NULL, 1, 1, RRD_ALGORITHM_ABSOLUTE);
        chart_data_arr[i]->dim_req_method_uncheckout = rrddim_add(chart_data_arr[i]->st_req_methods, "UNCHECKOUT", NULL, 1, 1, RRD_ALGORITHM_ABSOLUTE);
        chart_data_arr[i]->dim_req_method_unlink = rrddim_add(chart_data_arr[i]->st_req_methods, "UNLINK", NULL, 1, 1, RRD_ALGORITHM_ABSOLUTE);
        chart_data_arr[i]->dim_req_method_unlock = rrddim_add(chart_data_arr[i]->st_req_methods, "UNLOCK", NULL, 1, 1, RRD_ALGORITHM_ABSOLUTE);
        chart_data_arr[i]->dim_req_method_update = rrddim_add(chart_data_arr[i]->st_req_methods, "UPDATE", NULL, 1, 1, RRD_ALGORITHM_ABSOLUTE);
        chart_data_arr[i]->dim_req_method_updateredirectref = rrddim_add(chart_data_arr[i]->st_req_methods, "UPDATEREDIRECTREF", NULL, 1, 1, RRD_ALGORITHM_ABSOLUTE);
        
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
        chart_data_arr[i]->dim_resp_code_3xx = rrddim_add(chart_data_arr[i]->st_resp_code_family, "3xx", NULL, 1, 1, RRD_ALGORITHM_ABSOLUTE); 
        chart_data_arr[i]->dim_resp_code_4xx = rrddim_add(chart_data_arr[i]->st_resp_code_family, "4xx", NULL, 1, 1, RRD_ALGORITHM_ABSOLUTE); 
        chart_data_arr[i]->dim_resp_code_5xx = rrddim_add(chart_data_arr[i]->st_resp_code_family, "5xx", NULL, 1, 1, RRD_ALGORITHM_ABSOLUTE); 
        chart_data_arr[i]->dim_resp_code_other = rrddim_add(chart_data_arr[i]->st_resp_code_family, "other", NULL, 1, 1, RRD_ALGORITHM_ABSOLUTE);        

        /* Response code - initialise */
        chart_data_arr[i]->st_resp_code = rrdset_create_localhost(
                chart_data_arr[i]->rrd_type
                , "detailed responses"
                , NULL
                , "detailed responses"
                , NULL
                , "Detailed Response Codes"
                , "requests/s"
                , "logsmanagement.plugin"
                , NULL
                , 132200
                , localhost->rrd_update_every
                , RRDSET_TYPE_AREA
        );
        for(int j = 0; j < 500; j++){
            char dim_resp_code_name[4];
            sprintf(dim_resp_code_name, "%d", j + 100);
            chart_data_arr[i]->dim_resp_code[j] = rrddim_add(chart_data_arr[i]->st_resp_code, dim_resp_code_name, NULL, 1, 1, RRD_ALGORITHM_ABSOLUTE);
        }
        chart_data_arr[i]->dim_resp_code[500] = rrddim_add(chart_data_arr[i]->st_resp_code, "other", NULL, 1, 1, RRD_ALGORITHM_ABSOLUTE);

        uv_mutex_lock(p_file_info->parser_mut);

        /* Number of lines - collect first time */
        chart_data_arr[i]->num_lines = p_file_info->parser_metrics->num_lines;
        p_file_info->parser_metrics->num_lines = 0;

        /* Request methods - collect first time */
        chart_data_arr[i]->num_req_method_acl = p_file_info->parser_metrics->req_method.acl;
        p_file_info->parser_metrics->req_method.acl = 0;
        chart_data_arr[i]->num_req_method_baseline_control = p_file_info->parser_metrics->req_method.baseline_control;
        p_file_info->parser_metrics->req_method.baseline_control = 0;
        chart_data_arr[i]->num_req_method_bind = p_file_info->parser_metrics->req_method.bind;
        p_file_info->parser_metrics->req_method.bind = 0;
        chart_data_arr[i]->num_req_method_checkin = p_file_info->parser_metrics->req_method.checkin;
        p_file_info->parser_metrics->req_method.checkin = 0;
        chart_data_arr[i]->num_req_method_checkout = p_file_info->parser_metrics->req_method.checkout;
        p_file_info->parser_metrics->req_method.checkout = 0;
        chart_data_arr[i]->num_req_method_connect = p_file_info->parser_metrics->req_method.connect;
        p_file_info->parser_metrics->req_method.connect = 0;
        chart_data_arr[i]->num_req_method_copy = p_file_info->parser_metrics->req_method.copy;
        p_file_info->parser_metrics->req_method.copy = 0;
        chart_data_arr[i]->num_req_method_delet = p_file_info->parser_metrics->req_method.delet;
        p_file_info->parser_metrics->req_method.delet = 0;
        chart_data_arr[i]->num_req_method_get = p_file_info->parser_metrics->req_method.get;
        p_file_info->parser_metrics->req_method.get = 0;
        chart_data_arr[i]->num_req_method_head = p_file_info->parser_metrics->req_method.head;
        p_file_info->parser_metrics->req_method.head = 0;
        chart_data_arr[i]->num_req_method_label = p_file_info->parser_metrics->req_method.label;
        p_file_info->parser_metrics->req_method.label = 0;
        chart_data_arr[i]->num_req_method_link = p_file_info->parser_metrics->req_method.link;
        p_file_info->parser_metrics->req_method.link = 0;
        chart_data_arr[i]->num_req_method_lock = p_file_info->parser_metrics->req_method.lock;
        p_file_info->parser_metrics->req_method.lock = 0;
        chart_data_arr[i]->num_req_method_merge = p_file_info->parser_metrics->req_method.merge;
        p_file_info->parser_metrics->req_method.merge = 0;
        chart_data_arr[i]->num_req_method_mkactivity = p_file_info->parser_metrics->req_method.mkactivity;
        p_file_info->parser_metrics->req_method.mkactivity = 0;
        chart_data_arr[i]->num_req_method_mkcalendar = p_file_info->parser_metrics->req_method.mkcalendar;
        p_file_info->parser_metrics->req_method.mkcalendar = 0;
        chart_data_arr[i]->num_req_method_mkcol = p_file_info->parser_metrics->req_method.mkcol;
        p_file_info->parser_metrics->req_method.mkcol = 0;
        chart_data_arr[i]->num_req_method_mkredirectref = p_file_info->parser_metrics->req_method.mkredirectref;
        p_file_info->parser_metrics->req_method.mkredirectref = 0;
        chart_data_arr[i]->num_req_method_mkworkspace = p_file_info->parser_metrics->req_method.mkworkspace;
        p_file_info->parser_metrics->req_method.mkworkspace = 0;
        chart_data_arr[i]->num_req_method_move = p_file_info->parser_metrics->req_method.move;
        p_file_info->parser_metrics->req_method.move = 0;
        chart_data_arr[i]->num_req_method_options = p_file_info->parser_metrics->req_method.options;
        p_file_info->parser_metrics->req_method.options = 0;
        chart_data_arr[i]->num_req_method_orderpatch = p_file_info->parser_metrics->req_method.orderpatch;
        p_file_info->parser_metrics->req_method.orderpatch = 0;
        chart_data_arr[i]->num_req_method_patch = p_file_info->parser_metrics->req_method.patch;
        p_file_info->parser_metrics->req_method.patch = 0;
        chart_data_arr[i]->num_req_method_post = p_file_info->parser_metrics->req_method.post;
        p_file_info->parser_metrics->req_method.post = 0;
        chart_data_arr[i]->num_req_method_pri = p_file_info->parser_metrics->req_method.pri;
        p_file_info->parser_metrics->req_method.pri = 0;
        chart_data_arr[i]->num_req_method_propfind = p_file_info->parser_metrics->req_method.propfind;
        p_file_info->parser_metrics->req_method.propfind = 0;
        chart_data_arr[i]->num_req_method_proppatch = p_file_info->parser_metrics->req_method.proppatch;
        p_file_info->parser_metrics->req_method.proppatch = 0;
        chart_data_arr[i]->num_req_method_put = p_file_info->parser_metrics->req_method.put;
        p_file_info->parser_metrics->req_method.put = 0;
        chart_data_arr[i]->num_req_method_rebind = p_file_info->parser_metrics->req_method.rebind;
        p_file_info->parser_metrics->req_method.rebind = 0;
        chart_data_arr[i]->num_req_method_report = p_file_info->parser_metrics->req_method.report;
        p_file_info->parser_metrics->req_method.report = 0;
        chart_data_arr[i]->num_req_method_search = p_file_info->parser_metrics->req_method.search;
        p_file_info->parser_metrics->req_method.search = 0;
        chart_data_arr[i]->num_req_method_trace = p_file_info->parser_metrics->req_method.trace;
        p_file_info->parser_metrics->req_method.trace = 0;
        chart_data_arr[i]->num_req_method_unbind = p_file_info->parser_metrics->req_method.unbind;
        p_file_info->parser_metrics->req_method.unbind = 0;
        chart_data_arr[i]->num_req_method_uncheckout = p_file_info->parser_metrics->req_method.uncheckout;
        p_file_info->parser_metrics->req_method.uncheckout = 0;
        chart_data_arr[i]->num_req_method_unlink = p_file_info->parser_metrics->req_method.unlink;
        p_file_info->parser_metrics->req_method.unlink = 0;
        chart_data_arr[i]->num_req_method_unlock = p_file_info->parser_metrics->req_method.unlock;
        p_file_info->parser_metrics->req_method.unlock = 0;
        chart_data_arr[i]->num_req_method_update = p_file_info->parser_metrics->req_method.update;
        p_file_info->parser_metrics->req_method.update = 0;
        chart_data_arr[i]->num_req_method_updateredirectref = p_file_info->parser_metrics->req_method.updateredirectref;
        p_file_info->parser_metrics->req_method.updateredirectref = 0;


        /* Response code family - collect first time */
        chart_data_arr[i]->num_resp_code_1xx = p_file_info->parser_metrics->resp_code_family.resp_1xx;
        p_file_info->parser_metrics->resp_code_family.resp_1xx = 0;
        chart_data_arr[i]->num_resp_code_2xx = p_file_info->parser_metrics->resp_code_family.resp_2xx;
        p_file_info->parser_metrics->resp_code_family.resp_2xx = 0;
        chart_data_arr[i]->num_resp_code_3xx = p_file_info->parser_metrics->resp_code_family.resp_3xx;
        p_file_info->parser_metrics->resp_code_family.resp_3xx = 0;
        chart_data_arr[i]->num_resp_code_4xx = p_file_info->parser_metrics->resp_code_family.resp_4xx;
        p_file_info->parser_metrics->resp_code_family.resp_4xx = 0;
        chart_data_arr[i]->num_resp_code_5xx = p_file_info->parser_metrics->resp_code_family.resp_5xx;
        p_file_info->parser_metrics->resp_code_family.resp_5xx = 0;
        chart_data_arr[i]->num_resp_code_other = p_file_info->parser_metrics->resp_code_family.other;
        p_file_info->parser_metrics->resp_code_family.other = 0;

        /* Response code - collect first time */
        for(int j = 0; j < 501; j++){
            chart_data_arr[i]->num_resp_code[j] = p_file_info->parser_metrics->resp_code[j];
            p_file_info->parser_metrics->resp_code[j] = 0;
        }
        
        uv_mutex_unlock(p_file_info->parser_mut);



        /* Number of lines - update chart first time */
        rrddim_set_by_pointer(chart_data_arr[i]->st_lines, chart_data_arr[i]->dim_lines, chart_data_arr[i]->num_lines);
    	rrdset_done(chart_data_arr[i]->st_lines);

        /* Request methods - update chart first time */
        rrddim_set_by_pointer(chart_data_arr[i]->st_req_methods, chart_data_arr[i]->dim_req_method_acl, chart_data_arr[i]->num_req_method_acl);
        rrddim_set_by_pointer(chart_data_arr[i]->st_req_methods, chart_data_arr[i]->dim_req_method_baseline_control, chart_data_arr[i]->num_req_method_baseline_control);
        rrddim_set_by_pointer(chart_data_arr[i]->st_req_methods, chart_data_arr[i]->dim_req_method_bind, chart_data_arr[i]->num_req_method_bind);
        rrddim_set_by_pointer(chart_data_arr[i]->st_req_methods, chart_data_arr[i]->dim_req_method_checkin, chart_data_arr[i]->num_req_method_checkin);
        rrddim_set_by_pointer(chart_data_arr[i]->st_req_methods, chart_data_arr[i]->dim_req_method_checkout, chart_data_arr[i]->num_req_method_checkout);
        rrddim_set_by_pointer(chart_data_arr[i]->st_req_methods, chart_data_arr[i]->dim_req_method_connect, chart_data_arr[i]->num_req_method_connect);
        rrddim_set_by_pointer(chart_data_arr[i]->st_req_methods, chart_data_arr[i]->dim_req_method_copy, chart_data_arr[i]->num_req_method_copy);
        rrddim_set_by_pointer(chart_data_arr[i]->st_req_methods, chart_data_arr[i]->dim_req_method_delet, chart_data_arr[i]->num_req_method_delet);
        rrddim_set_by_pointer(chart_data_arr[i]->st_req_methods, chart_data_arr[i]->dim_req_method_get, chart_data_arr[i]->num_req_method_get);
        rrddim_set_by_pointer(chart_data_arr[i]->st_req_methods, chart_data_arr[i]->dim_req_method_head, chart_data_arr[i]->num_req_method_head);
        rrddim_set_by_pointer(chart_data_arr[i]->st_req_methods, chart_data_arr[i]->dim_req_method_label, chart_data_arr[i]->num_req_method_label);
        rrddim_set_by_pointer(chart_data_arr[i]->st_req_methods, chart_data_arr[i]->dim_req_method_link, chart_data_arr[i]->num_req_method_link);
        rrddim_set_by_pointer(chart_data_arr[i]->st_req_methods, chart_data_arr[i]->dim_req_method_lock, chart_data_arr[i]->num_req_method_lock);
        rrddim_set_by_pointer(chart_data_arr[i]->st_req_methods, chart_data_arr[i]->dim_req_method_merge, chart_data_arr[i]->num_req_method_merge);
        rrddim_set_by_pointer(chart_data_arr[i]->st_req_methods, chart_data_arr[i]->dim_req_method_mkactivity, chart_data_arr[i]->num_req_method_mkactivity);
        rrddim_set_by_pointer(chart_data_arr[i]->st_req_methods, chart_data_arr[i]->dim_req_method_mkcalendar, chart_data_arr[i]->num_req_method_mkcalendar);
        rrddim_set_by_pointer(chart_data_arr[i]->st_req_methods, chart_data_arr[i]->dim_req_method_mkcol, chart_data_arr[i]->num_req_method_mkcol);
        rrddim_set_by_pointer(chart_data_arr[i]->st_req_methods, chart_data_arr[i]->dim_req_method_mkredirectref, chart_data_arr[i]->num_req_method_mkredirectref);
        rrddim_set_by_pointer(chart_data_arr[i]->st_req_methods, chart_data_arr[i]->dim_req_method_mkworkspace, chart_data_arr[i]->num_req_method_mkworkspace);
        rrddim_set_by_pointer(chart_data_arr[i]->st_req_methods, chart_data_arr[i]->dim_req_method_move, chart_data_arr[i]->num_req_method_move);
        rrddim_set_by_pointer(chart_data_arr[i]->st_req_methods, chart_data_arr[i]->dim_req_method_options, chart_data_arr[i]->num_req_method_options);
        rrddim_set_by_pointer(chart_data_arr[i]->st_req_methods, chart_data_arr[i]->dim_req_method_orderpatch, chart_data_arr[i]->num_req_method_orderpatch);
        rrddim_set_by_pointer(chart_data_arr[i]->st_req_methods, chart_data_arr[i]->dim_req_method_patch, chart_data_arr[i]->num_req_method_patch);
        rrddim_set_by_pointer(chart_data_arr[i]->st_req_methods, chart_data_arr[i]->dim_req_method_post, chart_data_arr[i]->num_req_method_post);
        rrddim_set_by_pointer(chart_data_arr[i]->st_req_methods, chart_data_arr[i]->dim_req_method_pri, chart_data_arr[i]->num_req_method_pri);
        rrddim_set_by_pointer(chart_data_arr[i]->st_req_methods, chart_data_arr[i]->dim_req_method_propfind, chart_data_arr[i]->num_req_method_propfind);
        rrddim_set_by_pointer(chart_data_arr[i]->st_req_methods, chart_data_arr[i]->dim_req_method_proppatch, chart_data_arr[i]->num_req_method_proppatch);
        rrddim_set_by_pointer(chart_data_arr[i]->st_req_methods, chart_data_arr[i]->dim_req_method_put, chart_data_arr[i]->num_req_method_put);
        rrddim_set_by_pointer(chart_data_arr[i]->st_req_methods, chart_data_arr[i]->dim_req_method_rebind, chart_data_arr[i]->num_req_method_rebind);
        rrddim_set_by_pointer(chart_data_arr[i]->st_req_methods, chart_data_arr[i]->dim_req_method_report, chart_data_arr[i]->num_req_method_report);
        rrddim_set_by_pointer(chart_data_arr[i]->st_req_methods, chart_data_arr[i]->dim_req_method_search, chart_data_arr[i]->num_req_method_search);
        rrddim_set_by_pointer(chart_data_arr[i]->st_req_methods, chart_data_arr[i]->dim_req_method_trace, chart_data_arr[i]->num_req_method_trace);
        rrddim_set_by_pointer(chart_data_arr[i]->st_req_methods, chart_data_arr[i]->dim_req_method_unbind, chart_data_arr[i]->num_req_method_unbind);
        rrddim_set_by_pointer(chart_data_arr[i]->st_req_methods, chart_data_arr[i]->dim_req_method_uncheckout, chart_data_arr[i]->num_req_method_uncheckout);
        rrddim_set_by_pointer(chart_data_arr[i]->st_req_methods, chart_data_arr[i]->dim_req_method_unlink, chart_data_arr[i]->num_req_method_unlink);
        rrddim_set_by_pointer(chart_data_arr[i]->st_req_methods, chart_data_arr[i]->dim_req_method_unlock, chart_data_arr[i]->num_req_method_unlock);
        rrddim_set_by_pointer(chart_data_arr[i]->st_req_methods, chart_data_arr[i]->dim_req_method_update, chart_data_arr[i]->num_req_method_update);
        rrddim_set_by_pointer(chart_data_arr[i]->st_req_methods, chart_data_arr[i]->dim_req_method_updateredirectref, chart_data_arr[i]->num_req_method_updateredirectref);
        rrdset_done(chart_data_arr[i]->st_req_methods);

        /* Response code family - update chart first time */
        rrddim_set_by_pointer(chart_data_arr[i]->st_resp_code_family, chart_data_arr[i]->dim_resp_code_1xx, chart_data_arr[i]->num_resp_code_1xx);
        rrddim_set_by_pointer(chart_data_arr[i]->st_resp_code_family, chart_data_arr[i]->dim_resp_code_2xx, chart_data_arr[i]->num_resp_code_2xx);
        rrddim_set_by_pointer(chart_data_arr[i]->st_resp_code_family, chart_data_arr[i]->dim_resp_code_3xx, chart_data_arr[i]->num_resp_code_3xx);
        rrddim_set_by_pointer(chart_data_arr[i]->st_resp_code_family, chart_data_arr[i]->dim_resp_code_4xx, chart_data_arr[i]->num_resp_code_4xx);
        rrddim_set_by_pointer(chart_data_arr[i]->st_resp_code_family, chart_data_arr[i]->dim_resp_code_5xx, chart_data_arr[i]->num_resp_code_5xx);
        rrddim_set_by_pointer(chart_data_arr[i]->st_resp_code_family, chart_data_arr[i]->dim_resp_code_other, chart_data_arr[i]->num_resp_code_other);
        rrdset_done(chart_data_arr[i]->st_resp_code_family);

        /* Response code - update chart first time */
        for(int j = 0; j < 501; j++) rrddim_set_by_pointer(chart_data_arr[i]->st_resp_code, chart_data_arr[i]->dim_resp_code[j], chart_data_arr[i]->num_resp_code[j]);
        rrdset_done(chart_data_arr[i]->st_resp_code);
        
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
            chart_data_arr[i]->num_req_method_acl = p_file_info->parser_metrics->req_method.acl;
            p_file_info->parser_metrics->req_method.acl = 0;
            chart_data_arr[i]->num_req_method_baseline_control = p_file_info->parser_metrics->req_method.baseline_control;
            p_file_info->parser_metrics->req_method.baseline_control = 0;
            chart_data_arr[i]->num_req_method_bind = p_file_info->parser_metrics->req_method.bind;
            p_file_info->parser_metrics->req_method.bind = 0;
            chart_data_arr[i]->num_req_method_checkin = p_file_info->parser_metrics->req_method.checkin;
            p_file_info->parser_metrics->req_method.checkin = 0;
            chart_data_arr[i]->num_req_method_checkout = p_file_info->parser_metrics->req_method.checkout;
            p_file_info->parser_metrics->req_method.checkout = 0;
            chart_data_arr[i]->num_req_method_connect = p_file_info->parser_metrics->req_method.connect;
            p_file_info->parser_metrics->req_method.connect = 0;
            chart_data_arr[i]->num_req_method_copy = p_file_info->parser_metrics->req_method.copy;
            p_file_info->parser_metrics->req_method.copy = 0;
            chart_data_arr[i]->num_req_method_delet = p_file_info->parser_metrics->req_method.delet;
            p_file_info->parser_metrics->req_method.delet = 0;
            chart_data_arr[i]->num_req_method_get = p_file_info->parser_metrics->req_method.get;
            p_file_info->parser_metrics->req_method.get = 0;
            chart_data_arr[i]->num_req_method_head = p_file_info->parser_metrics->req_method.head;
            p_file_info->parser_metrics->req_method.head = 0;
            chart_data_arr[i]->num_req_method_label = p_file_info->parser_metrics->req_method.label;
            p_file_info->parser_metrics->req_method.label = 0;
            chart_data_arr[i]->num_req_method_link = p_file_info->parser_metrics->req_method.link;
            p_file_info->parser_metrics->req_method.link = 0;
            chart_data_arr[i]->num_req_method_lock = p_file_info->parser_metrics->req_method.lock;
            p_file_info->parser_metrics->req_method.lock = 0;
            chart_data_arr[i]->num_req_method_merge = p_file_info->parser_metrics->req_method.merge;
            p_file_info->parser_metrics->req_method.merge = 0;
            chart_data_arr[i]->num_req_method_mkactivity = p_file_info->parser_metrics->req_method.mkactivity;
            p_file_info->parser_metrics->req_method.mkactivity = 0;
            chart_data_arr[i]->num_req_method_mkcalendar = p_file_info->parser_metrics->req_method.mkcalendar;
            p_file_info->parser_metrics->req_method.mkcalendar = 0;
            chart_data_arr[i]->num_req_method_mkcol = p_file_info->parser_metrics->req_method.mkcol;
            p_file_info->parser_metrics->req_method.mkcol = 0;
            chart_data_arr[i]->num_req_method_mkredirectref = p_file_info->parser_metrics->req_method.mkredirectref;
            p_file_info->parser_metrics->req_method.mkredirectref = 0;
            chart_data_arr[i]->num_req_method_mkworkspace = p_file_info->parser_metrics->req_method.mkworkspace;
            p_file_info->parser_metrics->req_method.mkworkspace = 0;
            chart_data_arr[i]->num_req_method_move = p_file_info->parser_metrics->req_method.move;
            p_file_info->parser_metrics->req_method.move = 0;
            chart_data_arr[i]->num_req_method_options = p_file_info->parser_metrics->req_method.options;
            p_file_info->parser_metrics->req_method.options = 0;
            chart_data_arr[i]->num_req_method_orderpatch = p_file_info->parser_metrics->req_method.orderpatch;
            p_file_info->parser_metrics->req_method.orderpatch = 0;
            chart_data_arr[i]->num_req_method_patch = p_file_info->parser_metrics->req_method.patch;
            p_file_info->parser_metrics->req_method.patch = 0;
            chart_data_arr[i]->num_req_method_post = p_file_info->parser_metrics->req_method.post;
            p_file_info->parser_metrics->req_method.post = 0;
            chart_data_arr[i]->num_req_method_pri = p_file_info->parser_metrics->req_method.pri;
            p_file_info->parser_metrics->req_method.pri = 0;
            chart_data_arr[i]->num_req_method_propfind = p_file_info->parser_metrics->req_method.propfind;
            p_file_info->parser_metrics->req_method.propfind = 0;
            chart_data_arr[i]->num_req_method_proppatch = p_file_info->parser_metrics->req_method.proppatch;
            p_file_info->parser_metrics->req_method.proppatch = 0;
            chart_data_arr[i]->num_req_method_put = p_file_info->parser_metrics->req_method.put;
            p_file_info->parser_metrics->req_method.put = 0;
            chart_data_arr[i]->num_req_method_rebind = p_file_info->parser_metrics->req_method.rebind;
            p_file_info->parser_metrics->req_method.rebind = 0;
            chart_data_arr[i]->num_req_method_report = p_file_info->parser_metrics->req_method.report;
            p_file_info->parser_metrics->req_method.report = 0;
            chart_data_arr[i]->num_req_method_search = p_file_info->parser_metrics->req_method.search;
            p_file_info->parser_metrics->req_method.search = 0;
            chart_data_arr[i]->num_req_method_trace = p_file_info->parser_metrics->req_method.trace;
            p_file_info->parser_metrics->req_method.trace = 0;
            chart_data_arr[i]->num_req_method_unbind = p_file_info->parser_metrics->req_method.unbind;
            p_file_info->parser_metrics->req_method.unbind = 0;
            chart_data_arr[i]->num_req_method_uncheckout = p_file_info->parser_metrics->req_method.uncheckout;
            p_file_info->parser_metrics->req_method.uncheckout = 0;
            chart_data_arr[i]->num_req_method_unlink = p_file_info->parser_metrics->req_method.unlink;
            p_file_info->parser_metrics->req_method.unlink = 0;
            chart_data_arr[i]->num_req_method_unlock = p_file_info->parser_metrics->req_method.unlock;
            p_file_info->parser_metrics->req_method.unlock = 0;
            chart_data_arr[i]->num_req_method_update = p_file_info->parser_metrics->req_method.update;
            p_file_info->parser_metrics->req_method.update = 0;
            chart_data_arr[i]->num_req_method_updateredirectref = p_file_info->parser_metrics->req_method.updateredirectref;
            p_file_info->parser_metrics->req_method.updateredirectref = 0;

            /* Response code family - collect */
            chart_data_arr[i]->num_resp_code_1xx = p_file_info->parser_metrics->resp_code_family.resp_1xx;
            p_file_info->parser_metrics->resp_code_family.resp_1xx = 0;
            chart_data_arr[i]->num_resp_code_2xx = p_file_info->parser_metrics->resp_code_family.resp_2xx;
            p_file_info->parser_metrics->resp_code_family.resp_2xx = 0;
            chart_data_arr[i]->num_resp_code_3xx = p_file_info->parser_metrics->resp_code_family.resp_3xx;
            p_file_info->parser_metrics->resp_code_family.resp_3xx = 0;
            chart_data_arr[i]->num_resp_code_4xx = p_file_info->parser_metrics->resp_code_family.resp_4xx;
            p_file_info->parser_metrics->resp_code_family.resp_4xx = 0;
            chart_data_arr[i]->num_resp_code_5xx = p_file_info->parser_metrics->resp_code_family.resp_5xx;
            p_file_info->parser_metrics->resp_code_family.resp_5xx = 0;
            chart_data_arr[i]->num_resp_code_other = p_file_info->parser_metrics->resp_code_family.other;
            p_file_info->parser_metrics->resp_code_family.other = 0;

            /* Response code - collect */
            for(int j = 0; j < 501; j++){
                chart_data_arr[i]->num_resp_code[j] = p_file_info->parser_metrics->resp_code[j];
                p_file_info->parser_metrics->resp_code[j] = 0;
            }
            
            uv_mutex_unlock(p_file_info->parser_mut);



            /* Number of lines - update chart */
    		rrdset_next(chart_data_arr[i]->st_lines);
    		rrddim_set_by_pointer(chart_data_arr[i]->st_lines, chart_data_arr[i]->dim_lines, chart_data_arr[i]->num_lines);
            rrdset_done(chart_data_arr[i]->st_lines);

            /* Request methods - update chart */
            rrdset_next(chart_data_arr[i]->st_req_methods);
            rrddim_set_by_pointer(chart_data_arr[i]->st_req_methods, chart_data_arr[i]->dim_req_method_acl, chart_data_arr[i]->num_req_method_acl);
            rrddim_set_by_pointer(chart_data_arr[i]->st_req_methods, chart_data_arr[i]->dim_req_method_baseline_control, chart_data_arr[i]->num_req_method_baseline_control);
            rrddim_set_by_pointer(chart_data_arr[i]->st_req_methods, chart_data_arr[i]->dim_req_method_bind, chart_data_arr[i]->num_req_method_bind);
            rrddim_set_by_pointer(chart_data_arr[i]->st_req_methods, chart_data_arr[i]->dim_req_method_checkin, chart_data_arr[i]->num_req_method_checkin);
            rrddim_set_by_pointer(chart_data_arr[i]->st_req_methods, chart_data_arr[i]->dim_req_method_checkout, chart_data_arr[i]->num_req_method_checkout);
            rrddim_set_by_pointer(chart_data_arr[i]->st_req_methods, chart_data_arr[i]->dim_req_method_connect, chart_data_arr[i]->num_req_method_connect);
            rrddim_set_by_pointer(chart_data_arr[i]->st_req_methods, chart_data_arr[i]->dim_req_method_copy, chart_data_arr[i]->num_req_method_copy);
            rrddim_set_by_pointer(chart_data_arr[i]->st_req_methods, chart_data_arr[i]->dim_req_method_delet, chart_data_arr[i]->num_req_method_delet);
            rrddim_set_by_pointer(chart_data_arr[i]->st_req_methods, chart_data_arr[i]->dim_req_method_get, chart_data_arr[i]->num_req_method_get);
            rrddim_set_by_pointer(chart_data_arr[i]->st_req_methods, chart_data_arr[i]->dim_req_method_head, chart_data_arr[i]->num_req_method_head);
            rrddim_set_by_pointer(chart_data_arr[i]->st_req_methods, chart_data_arr[i]->dim_req_method_label, chart_data_arr[i]->num_req_method_label);
            rrddim_set_by_pointer(chart_data_arr[i]->st_req_methods, chart_data_arr[i]->dim_req_method_link, chart_data_arr[i]->num_req_method_link);
            rrddim_set_by_pointer(chart_data_arr[i]->st_req_methods, chart_data_arr[i]->dim_req_method_lock, chart_data_arr[i]->num_req_method_lock);
            rrddim_set_by_pointer(chart_data_arr[i]->st_req_methods, chart_data_arr[i]->dim_req_method_merge, chart_data_arr[i]->num_req_method_merge);
            rrddim_set_by_pointer(chart_data_arr[i]->st_req_methods, chart_data_arr[i]->dim_req_method_mkactivity, chart_data_arr[i]->num_req_method_mkactivity);
            rrddim_set_by_pointer(chart_data_arr[i]->st_req_methods, chart_data_arr[i]->dim_req_method_mkcalendar, chart_data_arr[i]->num_req_method_mkcalendar);
            rrddim_set_by_pointer(chart_data_arr[i]->st_req_methods, chart_data_arr[i]->dim_req_method_mkcol, chart_data_arr[i]->num_req_method_mkcol);
            rrddim_set_by_pointer(chart_data_arr[i]->st_req_methods, chart_data_arr[i]->dim_req_method_mkredirectref, chart_data_arr[i]->num_req_method_mkredirectref);
            rrddim_set_by_pointer(chart_data_arr[i]->st_req_methods, chart_data_arr[i]->dim_req_method_mkworkspace, chart_data_arr[i]->num_req_method_mkworkspace);
            rrddim_set_by_pointer(chart_data_arr[i]->st_req_methods, chart_data_arr[i]->dim_req_method_move, chart_data_arr[i]->num_req_method_move);
            rrddim_set_by_pointer(chart_data_arr[i]->st_req_methods, chart_data_arr[i]->dim_req_method_options, chart_data_arr[i]->num_req_method_options);
            rrddim_set_by_pointer(chart_data_arr[i]->st_req_methods, chart_data_arr[i]->dim_req_method_orderpatch, chart_data_arr[i]->num_req_method_orderpatch);
            rrddim_set_by_pointer(chart_data_arr[i]->st_req_methods, chart_data_arr[i]->dim_req_method_patch, chart_data_arr[i]->num_req_method_patch);
            rrddim_set_by_pointer(chart_data_arr[i]->st_req_methods, chart_data_arr[i]->dim_req_method_post, chart_data_arr[i]->num_req_method_post);
            rrddim_set_by_pointer(chart_data_arr[i]->st_req_methods, chart_data_arr[i]->dim_req_method_pri, chart_data_arr[i]->num_req_method_pri);
            rrddim_set_by_pointer(chart_data_arr[i]->st_req_methods, chart_data_arr[i]->dim_req_method_propfind, chart_data_arr[i]->num_req_method_propfind);
            rrddim_set_by_pointer(chart_data_arr[i]->st_req_methods, chart_data_arr[i]->dim_req_method_proppatch, chart_data_arr[i]->num_req_method_proppatch);
            rrddim_set_by_pointer(chart_data_arr[i]->st_req_methods, chart_data_arr[i]->dim_req_method_put, chart_data_arr[i]->num_req_method_put);
            rrddim_set_by_pointer(chart_data_arr[i]->st_req_methods, chart_data_arr[i]->dim_req_method_rebind, chart_data_arr[i]->num_req_method_rebind);
            rrddim_set_by_pointer(chart_data_arr[i]->st_req_methods, chart_data_arr[i]->dim_req_method_report, chart_data_arr[i]->num_req_method_report);
            rrddim_set_by_pointer(chart_data_arr[i]->st_req_methods, chart_data_arr[i]->dim_req_method_search, chart_data_arr[i]->num_req_method_search);
            rrddim_set_by_pointer(chart_data_arr[i]->st_req_methods, chart_data_arr[i]->dim_req_method_trace, chart_data_arr[i]->num_req_method_trace);
            rrddim_set_by_pointer(chart_data_arr[i]->st_req_methods, chart_data_arr[i]->dim_req_method_unbind, chart_data_arr[i]->num_req_method_unbind);
            rrddim_set_by_pointer(chart_data_arr[i]->st_req_methods, chart_data_arr[i]->dim_req_method_uncheckout, chart_data_arr[i]->num_req_method_uncheckout);
            rrddim_set_by_pointer(chart_data_arr[i]->st_req_methods, chart_data_arr[i]->dim_req_method_unlink, chart_data_arr[i]->num_req_method_unlink);
            rrddim_set_by_pointer(chart_data_arr[i]->st_req_methods, chart_data_arr[i]->dim_req_method_unlock, chart_data_arr[i]->num_req_method_unlock);
            rrddim_set_by_pointer(chart_data_arr[i]->st_req_methods, chart_data_arr[i]->dim_req_method_update, chart_data_arr[i]->num_req_method_update);
            rrddim_set_by_pointer(chart_data_arr[i]->st_req_methods, chart_data_arr[i]->dim_req_method_updateredirectref, chart_data_arr[i]->num_req_method_updateredirectref);
            rrdset_done(chart_data_arr[i]->st_req_methods);

            /* Response code family - update chart */
            rrdset_next(chart_data_arr[i]->st_resp_code_family);
            rrddim_set_by_pointer(chart_data_arr[i]->st_resp_code_family, chart_data_arr[i]->dim_resp_code_1xx, chart_data_arr[i]->num_resp_code_1xx);
            rrddim_set_by_pointer(chart_data_arr[i]->st_resp_code_family, chart_data_arr[i]->dim_resp_code_2xx, chart_data_arr[i]->num_resp_code_2xx);
            rrddim_set_by_pointer(chart_data_arr[i]->st_resp_code_family, chart_data_arr[i]->dim_resp_code_3xx, chart_data_arr[i]->num_resp_code_3xx);
            rrddim_set_by_pointer(chart_data_arr[i]->st_resp_code_family, chart_data_arr[i]->dim_resp_code_4xx, chart_data_arr[i]->num_resp_code_4xx);
            rrddim_set_by_pointer(chart_data_arr[i]->st_resp_code_family, chart_data_arr[i]->dim_resp_code_5xx, chart_data_arr[i]->num_resp_code_5xx);
            rrddim_set_by_pointer(chart_data_arr[i]->st_resp_code_family, chart_data_arr[i]->dim_resp_code_other, chart_data_arr[i]->num_resp_code_other);
            rrdset_done(chart_data_arr[i]->st_resp_code_family);

            /* Response code - update chart first time */
            rrdset_next(chart_data_arr[i]->st_resp_code);
            for(int j = 0; j < 501; j++) rrddim_set_by_pointer(chart_data_arr[i]->st_resp_code, chart_data_arr[i]->dim_resp_code[j], chart_data_arr[i]->num_resp_code[j]);
            rrdset_done(chart_data_arr[i]->st_resp_code);
            
        }
	}

    netdata_thread_cleanup_pop(1);
    return NULL;
}