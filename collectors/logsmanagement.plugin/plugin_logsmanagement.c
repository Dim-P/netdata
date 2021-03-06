// SPDX-License-Identifier: GPL-3.0-or-later

#include "plugin_logsmanagement.h"
#include "../../logsmanagement/file_info.h"

#define NETDATA_CHART_PRIO_LOGS_BASE            132200
#define NETDATA_CHART_PRIO_LINES                NETDATA_CHART_PRIO_LOGS_BASE + 0
#define NETDATA_CHART_PRIO_VHOST                NETDATA_CHART_PRIO_LOGS_BASE + 1
#define NETDATA_CHART_PRIO_PORT                 NETDATA_CHART_PRIO_LOGS_BASE + 2
#define NETDATA_CHART_PRIO_REQ_METHODS          NETDATA_CHART_PRIO_LOGS_BASE + 3
#define NETDATA_CHART_PRIO_REQ_PROTO            NETDATA_CHART_PRIO_LOGS_BASE + 4
#define NETDATA_CHART_PRIO_BANDWIDTH            NETDATA_CHART_PRIO_LOGS_BASE + 5
#define NETDATA_CHART_PRIO_REQ_PROC_TIME        NETDATA_CHART_PRIO_LOGS_BASE + 6
#define NETDATA_CHART_PRIO_RESP_CODE_FAMILY     NETDATA_CHART_PRIO_LOGS_BASE + 7
#define NETDATA_CHART_PRIO_RESP_CODE            NETDATA_CHART_PRIO_LOGS_BASE + 8
#define NETDATA_CHART_PRIO_RESP_CODE_TYPE       NETDATA_CHART_PRIO_LOGS_BASE + 9
#define NETDATA_CHART_PRIO_SSL_PROTO            NETDATA_CHART_PRIO_LOGS_BASE + 10
#define NETDATA_CHART_PRIO_SSL_CIPHER_SUITE     NETDATA_CHART_PRIO_LOGS_BASE + 11
#define NETDATA_CHART_PRIO_IP_VER               NETDATA_CHART_PRIO_LOGS_BASE + 12
#define NETDATA_CHART_PRIO_REQ_CLIENT_CURRENT   NETDATA_CHART_PRIO_LOGS_BASE + 13
#define NETDATA_CHART_PRIO_REQ_CLIENT_ALL_TIME  NETDATA_CHART_PRIO_LOGS_BASE + 14

struct Chart_data{
    char *rrd_type;

    /* Number of lines */
    RRDSET *st_lines;
    RRDDIM *dim_lines_total;
    RRDDIM *dim_lines_rate;
    collected_number num_lines_total, num_lines_rate;

    /* Vhosts */
    RRDSET *st_vhost;
    RRDDIM **dim_vhosts;
    collected_number *num_vhosts;
    int vhost_size, vhost_size_max; /**< Actual size and maximum allocated size of dim_vhosts, num_vhosts arrays **/ 

    /* Ports */
    RRDSET *st_port;
    RRDDIM **dim_ports;
    collected_number *num_ports;
    int *ports;                      /**< Array of port numbers  **/
    int port_size, port_size_max;    /**< Actual size and maximum allocated size of dim_ports, num_ports and ports arrays **/ 

    /* IP Version */
    RRDSET *st_ip_ver;
    RRDDIM *dim_ip_ver_4, *dim_ip_ver_6, *dim_ip_ver_invalid;
    collected_number num_ip_ver_4, num_ip_ver_6, num_ip_ver_invalid;

    /* Request client current poll */
    RRDSET *st_req_client_current;
    RRDDIM *dim_req_client_current_ipv4, *dim_req_client_current_ipv6;
    collected_number num_req_client_current_ipv4, num_req_client_current_ipv6;

    /* Request client all-time */
    RRDSET *st_req_client_all_time;
    RRDDIM *dim_req_client_all_time_ipv4, *dim_req_client_all_time_ipv6;
    collected_number num_req_client_all_time_ipv4, num_req_client_all_time_ipv6;

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

    /* Request protocol */
    RRDSET *st_req_proto;
    RRDDIM *dim_req_proto_http_1, *dim_req_proto_http_1_1, *dim_req_proto_http_2, *dim_req_proto_other;
    collected_number num_req_proto_http_1, num_req_proto_http_1_1, num_req_proto_http_2, num_req_proto_other;

    /* Request bandwidth */
    RRDSET *st_bandwidth;
    RRDDIM *dim_bandwidth_req_size, *dim_bandwidth_resp_size;
    collected_number num_bandwidth_req_size, num_bandwidth_resp_size;

    /* Request processing time */
    RRDSET *st_req_proc_time;
    RRDDIM *dim_req_proc_time_min, *dim_req_proc_time_max, *dim_req_proc_time_avg;
    collected_number num_req_proc_time_min, num_req_proc_time_max, num_req_proc_time_avg;

    /* Response code family */
    RRDSET *st_resp_code_family;
    RRDDIM *dim_resp_code_family_1xx, *dim_resp_code_family_2xx, *dim_resp_code_family_3xx, *dim_resp_code_family_4xx, *dim_resp_code_family_5xx, *dim_resp_code_family_other;
    collected_number num_resp_code_family_1xx, num_resp_code_family_2xx, num_resp_code_family_3xx, num_resp_code_family_4xx, num_resp_code_family_5xx, num_resp_code_family_other;

    /* Response code */
    RRDSET *st_resp_code;
    RRDDIM *dim_resp_code[501];
    collected_number num_resp_code[501];

    /* Response code type */
    RRDSET *st_resp_code_type;
    RRDDIM *dim_resp_code_type_success, *dim_resp_code_type_redirect, *dim_resp_code_type_bad, *dim_resp_code_type_error, *dim_resp_code_type_other;
    collected_number num_resp_code_type_success, num_resp_code_type_redirect, num_resp_code_type_bad, num_resp_code_type_error, num_resp_code_type_other;

    /* SSL protocol */
    RRDSET *st_ssl_proto;
    RRDDIM *dim_ssl_proto_tlsv1, *dim_ssl_proto_tlsv1_1, *dim_ssl_proto_tlsv1_2, *dim_ssl_proto_tlsv1_3, *dim_ssl_proto_sslv2, *dim_ssl_proto_sslv3, *dim_ssl_proto_other;
    collected_number num_ssl_proto_tlsv1, num_ssl_proto_tlsv1_1, num_ssl_proto_tlsv1_2, num_ssl_proto_tlsv1_3, num_ssl_proto_sslv2, num_ssl_proto_sslv3, num_ssl_proto_other;

    /* SSL cipher suite */
    RRDSET *st_ssl_cipher;
    RRDDIM **dim_ssl_ciphers;
    collected_number *num_ssl_ciphers;
    int ssl_cipher_size, ssl_cipher_size_max; /**< Actual size and maximum allocated size of dim_ssl_ciphers, num_ssl_ciphers arrays **/ 
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

    /* wait for p_file_infos_arr initialisation
     * TODO: Not production ready - needs refactoring as this can lead to race condition! */
    while(!p_file_infos_arr) sleep_usec(100000); 

    chart_data_arr = callocz(1, p_file_infos_arr->count * sizeof(struct Chart_data *));

    for(int i = 0; i < p_file_infos_arr->count; i++){
        struct File_info *p_file_info = p_file_infos_arr->data[i];
        if(!p_file_info->parser_config){ // Check if there is parser configuration to be used for chart generation
            chart_data_arr[i] = NULL;
            continue; 
        } 

        chart_data_arr[i] = callocz(1, sizeof(struct Chart_data));
        chart_data_arr[i]->rrd_type = (char *) p_file_info->chart_name;



        /* Number of lines - initialise */
    	chart_data_arr[i]->st_lines = rrdset_create_localhost(
                chart_data_arr[i]->rrd_type
                , "lines parsed"
                , NULL
                , "lines parsed"
                , NULL
                , "Log lines parsed"
                , "lines/s"
                , "logsmanagement.plugin"
                , NULL
                , NETDATA_CHART_PRIO_LINES
                , localhost->rrd_update_every
                , RRDSET_TYPE_AREA
        );
        // TODO: Change dim_lines_total to RRD_ALGORITHM_INCREMENTAL
        chart_data_arr[i]->dim_lines_total = rrddim_add(chart_data_arr[i]->st_lines, "Total lines", NULL, 1, 1, RRD_ALGORITHM_ABSOLUTE);
        chart_data_arr[i]->dim_lines_rate = rrddim_add(chart_data_arr[i]->st_lines, "New lines", NULL, 1, 1, RRD_ALGORITHM_INCREMENTAL);

        /* Vhost - initialise */
        if(p_file_info->parser_config->chart_config & CHART_VHOST){
            chart_data_arr[i]->st_vhost = rrdset_create_localhost(
                    chart_data_arr[i]->rrd_type
                    , "vhost"
                    , NULL
                    , "vhost"
                    , NULL
                    , "Requests by Vhost"
                    , "requests/s"
                    , "logsmanagement.plugin"
                    , NULL
                    , NETDATA_CHART_PRIO_VHOST
                    , localhost->rrd_update_every
                    , RRDSET_TYPE_AREA
            );
        }

        /* Port - initialise */
        if(p_file_info->parser_config->chart_config & CHART_PORT){
            chart_data_arr[i]->st_port = rrdset_create_localhost(
                    chart_data_arr[i]->rrd_type
                    , "port"
                    , NULL
                    , "port"
                    , NULL
                    , "Requests by Port"
                    , "requests/s"
                    , "logsmanagement.plugin"
                    , NULL
                    , NETDATA_CHART_PRIO_PORT
                    , localhost->rrd_update_every
                    , RRDSET_TYPE_AREA
            );
        }

        /* IP Version - initialise */
        if(p_file_info->parser_config->chart_config & CHART_IP_VERSION){
            chart_data_arr[i]->st_ip_ver = rrdset_create_localhost(
                    chart_data_arr[i]->rrd_type
                    , "ip version"
                    , NULL
                    , "ip version"
                    , NULL
                    , "Requests by IP version"
                    , "requests/s"
                    , "logsmanagement.plugin"
                    , NULL
                    , NETDATA_CHART_PRIO_IP_VER
                    , localhost->rrd_update_every
                    , RRDSET_TYPE_AREA
            );
            chart_data_arr[i]->dim_ip_ver_4 = rrddim_add(chart_data_arr[i]->st_ip_ver, "ipv4", NULL, 1, 1, RRD_ALGORITHM_INCREMENTAL);
            chart_data_arr[i]->dim_ip_ver_6 = rrddim_add(chart_data_arr[i]->st_ip_ver, "ipv6", NULL, 1, 1, RRD_ALGORITHM_INCREMENTAL);
            chart_data_arr[i]->dim_ip_ver_invalid = rrddim_add(chart_data_arr[i]->st_ip_ver, "invalid", NULL, 1, 1, RRD_ALGORITHM_INCREMENTAL);
        }

        /* Request client current poll - initialise */
        if(p_file_info->parser_config->chart_config & CHART_REQ_CLIENT_CURRENT){
            chart_data_arr[i]->st_req_client_current = rrdset_create_localhost(
                    chart_data_arr[i]->rrd_type
                    , "clients"
                    , NULL
                    , "clients"
                    , NULL
                    , "Current Poll Unique Client IPs"
                    , "unique ips"
                    , "logsmanagement.plugin"
                    , NULL
                    , NETDATA_CHART_PRIO_REQ_CLIENT_CURRENT
                    , localhost->rrd_update_every
                    , RRDSET_TYPE_AREA
            );
            chart_data_arr[i]->dim_req_client_current_ipv4 = rrddim_add(chart_data_arr[i]->st_req_client_current, "ipv4", NULL, 1, 1, RRD_ALGORITHM_INCREMENTAL);
            chart_data_arr[i]->dim_req_client_current_ipv6 = rrddim_add(chart_data_arr[i]->st_req_client_current, "ipv6", NULL, 1, 1, RRD_ALGORITHM_INCREMENTAL);
        }

        /* Request client all-time - initialise */
        if(p_file_info->parser_config->chart_config & CHART_REQ_CLIENT_ALL_TIME){
            chart_data_arr[i]->st_req_client_all_time = rrdset_create_localhost(
                    chart_data_arr[i]->rrd_type
                    , "clients all"
                    , NULL
                    , "clients"
                    , NULL
                    , "All Time Unique Client IPs"
                    , "unique ips"
                    , "logsmanagement.plugin"
                    , NULL
                    , NETDATA_CHART_PRIO_REQ_CLIENT_ALL_TIME
                    , localhost->rrd_update_every
                    , RRDSET_TYPE_AREA
            );
            chart_data_arr[i]->dim_req_client_all_time_ipv4 = rrddim_add(chart_data_arr[i]->st_req_client_all_time, "ipv4", NULL, 1, 1, RRD_ALGORITHM_ABSOLUTE);
            chart_data_arr[i]->dim_req_client_all_time_ipv6 = rrddim_add(chart_data_arr[i]->st_req_client_all_time, "ipv6", NULL, 1, 1, RRD_ALGORITHM_ABSOLUTE);
        }

        /* Request methods - initialise */
        if(p_file_info->parser_config->chart_config & CHART_REQ_METHODS){
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
                    , NETDATA_CHART_PRIO_REQ_METHODS
                    , localhost->rrd_update_every
                    , RRDSET_TYPE_AREA
            );
            chart_data_arr[i]->dim_req_method_acl = rrddim_add(chart_data_arr[i]->st_req_methods, "ACL", NULL, 1, 1, RRD_ALGORITHM_INCREMENTAL);
            chart_data_arr[i]->dim_req_method_baseline_control = rrddim_add(chart_data_arr[i]->st_req_methods, "BASELINE-CONTROL", NULL, 1, 1, RRD_ALGORITHM_INCREMENTAL);
            chart_data_arr[i]->dim_req_method_bind = rrddim_add(chart_data_arr[i]->st_req_methods, "BIND", NULL, 1, 1, RRD_ALGORITHM_INCREMENTAL);
            chart_data_arr[i]->dim_req_method_checkin = rrddim_add(chart_data_arr[i]->st_req_methods, "CHECKIN", NULL, 1, 1, RRD_ALGORITHM_INCREMENTAL);
            chart_data_arr[i]->dim_req_method_checkout = rrddim_add(chart_data_arr[i]->st_req_methods, "CHECKOUT", NULL, 1, 1, RRD_ALGORITHM_INCREMENTAL);
            chart_data_arr[i]->dim_req_method_connect = rrddim_add(chart_data_arr[i]->st_req_methods, "CONNECT", NULL, 1, 1, RRD_ALGORITHM_INCREMENTAL);
            chart_data_arr[i]->dim_req_method_copy = rrddim_add(chart_data_arr[i]->st_req_methods, "COPY", NULL, 1, 1, RRD_ALGORITHM_INCREMENTAL);
            chart_data_arr[i]->dim_req_method_delet = rrddim_add(chart_data_arr[i]->st_req_methods, "DELETE", NULL, 1, 1, RRD_ALGORITHM_INCREMENTAL);
            chart_data_arr[i]->dim_req_method_get = rrddim_add(chart_data_arr[i]->st_req_methods, "GET", NULL, 1, 1, RRD_ALGORITHM_INCREMENTAL);
            chart_data_arr[i]->dim_req_method_head = rrddim_add(chart_data_arr[i]->st_req_methods, "HEAD", NULL, 1, 1, RRD_ALGORITHM_INCREMENTAL);
            chart_data_arr[i]->dim_req_method_label = rrddim_add(chart_data_arr[i]->st_req_methods, "LABEL", NULL, 1, 1, RRD_ALGORITHM_INCREMENTAL);
            chart_data_arr[i]->dim_req_method_link = rrddim_add(chart_data_arr[i]->st_req_methods, "LINK", NULL, 1, 1, RRD_ALGORITHM_INCREMENTAL);
            chart_data_arr[i]->dim_req_method_lock = rrddim_add(chart_data_arr[i]->st_req_methods, "LOCK", NULL, 1, 1, RRD_ALGORITHM_INCREMENTAL);
            chart_data_arr[i]->dim_req_method_merge = rrddim_add(chart_data_arr[i]->st_req_methods, "MERGE", NULL, 1, 1, RRD_ALGORITHM_INCREMENTAL);
            chart_data_arr[i]->dim_req_method_mkactivity = rrddim_add(chart_data_arr[i]->st_req_methods, "MKACTIVITY", NULL, 1, 1, RRD_ALGORITHM_INCREMENTAL);
            chart_data_arr[i]->dim_req_method_mkcalendar = rrddim_add(chart_data_arr[i]->st_req_methods, "MKCALENDAR", NULL, 1, 1, RRD_ALGORITHM_INCREMENTAL);
            chart_data_arr[i]->dim_req_method_mkcol = rrddim_add(chart_data_arr[i]->st_req_methods, "MKCOL", NULL, 1, 1, RRD_ALGORITHM_INCREMENTAL);
            chart_data_arr[i]->dim_req_method_mkredirectref = rrddim_add(chart_data_arr[i]->st_req_methods, "MKREDIRECTREF", NULL, 1, 1, RRD_ALGORITHM_INCREMENTAL);
            chart_data_arr[i]->dim_req_method_mkworkspace = rrddim_add(chart_data_arr[i]->st_req_methods, "MKWORKSPACE", NULL, 1, 1, RRD_ALGORITHM_INCREMENTAL);
            chart_data_arr[i]->dim_req_method_move = rrddim_add(chart_data_arr[i]->st_req_methods, "MOVE", NULL, 1, 1, RRD_ALGORITHM_INCREMENTAL);
            chart_data_arr[i]->dim_req_method_options = rrddim_add(chart_data_arr[i]->st_req_methods, "OPTIONS", NULL, 1, 1, RRD_ALGORITHM_INCREMENTAL);
            chart_data_arr[i]->dim_req_method_orderpatch = rrddim_add(chart_data_arr[i]->st_req_methods, "ORDERPATCH", NULL, 1, 1, RRD_ALGORITHM_INCREMENTAL);
            chart_data_arr[i]->dim_req_method_patch = rrddim_add(chart_data_arr[i]->st_req_methods, "PATCH", NULL, 1, 1, RRD_ALGORITHM_INCREMENTAL);
            chart_data_arr[i]->dim_req_method_post = rrddim_add(chart_data_arr[i]->st_req_methods, "POST", NULL, 1, 1, RRD_ALGORITHM_INCREMENTAL);
            chart_data_arr[i]->dim_req_method_pri = rrddim_add(chart_data_arr[i]->st_req_methods, "PRI", NULL, 1, 1, RRD_ALGORITHM_INCREMENTAL);
            chart_data_arr[i]->dim_req_method_propfind = rrddim_add(chart_data_arr[i]->st_req_methods, "PROPFIND", NULL, 1, 1, RRD_ALGORITHM_INCREMENTAL);
            chart_data_arr[i]->dim_req_method_proppatch = rrddim_add(chart_data_arr[i]->st_req_methods, "PROPPATCH", NULL, 1, 1, RRD_ALGORITHM_INCREMENTAL);
            chart_data_arr[i]->dim_req_method_put = rrddim_add(chart_data_arr[i]->st_req_methods, "PUT", NULL, 1, 1, RRD_ALGORITHM_INCREMENTAL);
            chart_data_arr[i]->dim_req_method_rebind = rrddim_add(chart_data_arr[i]->st_req_methods, "REBIND", NULL, 1, 1, RRD_ALGORITHM_INCREMENTAL);
            chart_data_arr[i]->dim_req_method_report = rrddim_add(chart_data_arr[i]->st_req_methods, "REPORT", NULL, 1, 1, RRD_ALGORITHM_INCREMENTAL);
            chart_data_arr[i]->dim_req_method_search = rrddim_add(chart_data_arr[i]->st_req_methods, "SEARCH", NULL, 1, 1, RRD_ALGORITHM_INCREMENTAL);
            chart_data_arr[i]->dim_req_method_trace = rrddim_add(chart_data_arr[i]->st_req_methods, "TRACE", NULL, 1, 1, RRD_ALGORITHM_INCREMENTAL);
            chart_data_arr[i]->dim_req_method_unbind = rrddim_add(chart_data_arr[i]->st_req_methods, "UNBIND", NULL, 1, 1, RRD_ALGORITHM_INCREMENTAL);
            chart_data_arr[i]->dim_req_method_uncheckout = rrddim_add(chart_data_arr[i]->st_req_methods, "UNCHECKOUT", NULL, 1, 1, RRD_ALGORITHM_INCREMENTAL);
            chart_data_arr[i]->dim_req_method_unlink = rrddim_add(chart_data_arr[i]->st_req_methods, "UNLINK", NULL, 1, 1, RRD_ALGORITHM_INCREMENTAL);
            chart_data_arr[i]->dim_req_method_unlock = rrddim_add(chart_data_arr[i]->st_req_methods, "UNLOCK", NULL, 1, 1, RRD_ALGORITHM_INCREMENTAL);
            chart_data_arr[i]->dim_req_method_update = rrddim_add(chart_data_arr[i]->st_req_methods, "UPDATE", NULL, 1, 1, RRD_ALGORITHM_INCREMENTAL);
            chart_data_arr[i]->dim_req_method_updateredirectref = rrddim_add(chart_data_arr[i]->st_req_methods, "UPDATEREDIRECTREF", NULL, 1, 1, RRD_ALGORITHM_INCREMENTAL);
        }
        
        /* Request protocol - initialise */
        if(p_file_info->parser_config->chart_config & CHART_REQ_PROTO){
            chart_data_arr[i]->st_req_proto = rrdset_create_localhost(
                    chart_data_arr[i]->rrd_type
                    , "http versions"
                    , NULL
                    , "http versions"
                    , NULL
                    , "Requests Per HTTP Version"
                    , "requests/s"
                    , "logsmanagement.plugin"
                    , NULL
                    , NETDATA_CHART_PRIO_REQ_PROTO
                    , localhost->rrd_update_every
                    , RRDSET_TYPE_AREA
            );
            chart_data_arr[i]->dim_req_proto_http_1 = rrddim_add(chart_data_arr[i]->st_req_proto, "1.0", NULL, 1, 1, RRD_ALGORITHM_INCREMENTAL);
            chart_data_arr[i]->dim_req_proto_http_1_1 = rrddim_add(chart_data_arr[i]->st_req_proto, "1.1", NULL, 1, 1, RRD_ALGORITHM_INCREMENTAL);
            chart_data_arr[i]->dim_req_proto_http_2 = rrddim_add(chart_data_arr[i]->st_req_proto, "2.0", NULL, 1, 1, RRD_ALGORITHM_INCREMENTAL);
            chart_data_arr[i]->dim_req_proto_other = rrddim_add(chart_data_arr[i]->st_req_proto, "other", NULL, 1, 1, RRD_ALGORITHM_INCREMENTAL);
        }

        /* Request bandwidth - initialise */
        if(p_file_info->parser_config->chart_config & CHART_BANDWIDTH){
            chart_data_arr[i]->st_bandwidth = rrdset_create_localhost(
                    chart_data_arr[i]->rrd_type
                    , "bandwidth"
                    , NULL
                    , "bandwidth"
                    , NULL
                    , "Bandwidth"
                    , "kilobits/s"
                    , "logsmanagement.plugin"
                    , NULL
                    , NETDATA_CHART_PRIO_BANDWIDTH
                    , localhost->rrd_update_every
                    , RRDSET_TYPE_AREA
            );
            chart_data_arr[i]->dim_bandwidth_req_size = rrddim_add(chart_data_arr[i]->st_bandwidth, "received", NULL, 8, 1000, RRD_ALGORITHM_INCREMENTAL);
            chart_data_arr[i]->dim_bandwidth_resp_size = rrddim_add(chart_data_arr[i]->st_bandwidth, "sent", NULL, -8, 1000, RRD_ALGORITHM_INCREMENTAL);
        }

        /* Request processing time - initialise */
        if(p_file_info->parser_config->chart_config & CHART_REQ_PROC_TIME){
            chart_data_arr[i]->st_req_proc_time = rrdset_create_localhost(
                    chart_data_arr[i]->rrd_type
                    , "timings"
                    , NULL
                    , "timings"
                    , NULL
                    , "Request Processing Time"
                    , "milliseconds"
                    , "logsmanagement.plugin"
                    , NULL
                    , NETDATA_CHART_PRIO_REQ_PROC_TIME
                    , localhost->rrd_update_every
                    , RRDSET_TYPE_LINE
            );
            chart_data_arr[i]->dim_req_proc_time_min = rrddim_add(chart_data_arr[i]->st_req_proc_time, "min", NULL, 1, 1000, RRD_ALGORITHM_ABSOLUTE);
            chart_data_arr[i]->dim_req_proc_time_max = rrddim_add(chart_data_arr[i]->st_req_proc_time, "max", NULL, 1, 1000, RRD_ALGORITHM_ABSOLUTE);
            chart_data_arr[i]->dim_req_proc_time_avg = rrddim_add(chart_data_arr[i]->st_req_proc_time, "avg", NULL, 1, 1000, RRD_ALGORITHM_ABSOLUTE);
        }

        /* Response code family - initialise */
        if(p_file_info->parser_config->chart_config & CHART_RESP_CODE_FAMILY){
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
                    , NETDATA_CHART_PRIO_RESP_CODE_FAMILY
                    , localhost->rrd_update_every
                    , RRDSET_TYPE_AREA
            );
            chart_data_arr[i]->dim_resp_code_family_1xx = rrddim_add(chart_data_arr[i]->st_resp_code_family, "1xx", NULL, 1, 1, RRD_ALGORITHM_INCREMENTAL);
            chart_data_arr[i]->dim_resp_code_family_2xx = rrddim_add(chart_data_arr[i]->st_resp_code_family, "2xx", NULL, 1, 1, RRD_ALGORITHM_INCREMENTAL);
            chart_data_arr[i]->dim_resp_code_family_3xx = rrddim_add(chart_data_arr[i]->st_resp_code_family, "3xx", NULL, 1, 1, RRD_ALGORITHM_INCREMENTAL); 
            chart_data_arr[i]->dim_resp_code_family_4xx = rrddim_add(chart_data_arr[i]->st_resp_code_family, "4xx", NULL, 1, 1, RRD_ALGORITHM_INCREMENTAL); 
            chart_data_arr[i]->dim_resp_code_family_5xx = rrddim_add(chart_data_arr[i]->st_resp_code_family, "5xx", NULL, 1, 1, RRD_ALGORITHM_INCREMENTAL); 
            chart_data_arr[i]->dim_resp_code_family_other = rrddim_add(chart_data_arr[i]->st_resp_code_family, "other", NULL, 1, 1, RRD_ALGORITHM_INCREMENTAL);     
        }   

        /* Response code - initialise */
        if(p_file_info->parser_config->chart_config & CHART_RESP_CODE){
            chart_data_arr[i]->st_resp_code = rrdset_create_localhost(
                    chart_data_arr[i]->rrd_type
                    , "detailed responses"
                    , NULL
                    , "responses"
                    , NULL
                    , "Detailed Response Codes"
                    , "requests/s"
                    , "logsmanagement.plugin"
                    , NULL
                    , NETDATA_CHART_PRIO_RESP_CODE
                    , localhost->rrd_update_every
                    , RRDSET_TYPE_AREA
            );
            for(int j = 0; j < 500; j++){
                char dim_resp_code_name[4];
                sprintf(dim_resp_code_name, "%d", j + 100);
                chart_data_arr[i]->dim_resp_code[j] = rrddim_add(chart_data_arr[i]->st_resp_code, dim_resp_code_name, NULL, 1, 1, RRD_ALGORITHM_INCREMENTAL);
            }
            chart_data_arr[i]->dim_resp_code[500] = rrddim_add(chart_data_arr[i]->st_resp_code, "other", NULL, 1, 1, RRD_ALGORITHM_INCREMENTAL);
        }

        /* Response code type - initialise */
        if(p_file_info->parser_config->chart_config & CHART_RESP_CODE_TYPE){
            chart_data_arr[i]->st_resp_code_type = rrdset_create_localhost(
                    chart_data_arr[i]->rrd_type
                    , "response types"
                    , NULL
                    , "responses"
                    , NULL
                    , "Response Statuses"
                    , "requests/s"
                    , "logsmanagement.plugin"
                    , NULL
                    , NETDATA_CHART_PRIO_RESP_CODE_TYPE
                    , localhost->rrd_update_every
                    , RRDSET_TYPE_AREA
            );
            chart_data_arr[i]->dim_resp_code_type_success = rrddim_add(chart_data_arr[i]->st_resp_code_type, "success", NULL, 1, 1, RRD_ALGORITHM_INCREMENTAL);
            chart_data_arr[i]->dim_resp_code_type_redirect = rrddim_add(chart_data_arr[i]->st_resp_code_type, "redirect", NULL, 1, 1, RRD_ALGORITHM_INCREMENTAL);
            chart_data_arr[i]->dim_resp_code_type_bad = rrddim_add(chart_data_arr[i]->st_resp_code_type, "bad", NULL, 1, 1, RRD_ALGORITHM_INCREMENTAL); 
            chart_data_arr[i]->dim_resp_code_type_error = rrddim_add(chart_data_arr[i]->st_resp_code_type, "error", NULL, 1, 1, RRD_ALGORITHM_INCREMENTAL); 
            chart_data_arr[i]->dim_resp_code_type_other = rrddim_add(chart_data_arr[i]->st_resp_code_type, "other", NULL, 1, 1, RRD_ALGORITHM_INCREMENTAL); 
        }

        /* SSL protocol - initialise */
        if(p_file_info->parser_config->chart_config & CHART_SSL_PROTO){
            chart_data_arr[i]->st_ssl_proto = rrdset_create_localhost(
                    chart_data_arr[i]->rrd_type
                    , "ssl protocol"
                    , NULL
                    , "ssl protocol"
                    , NULL
                    , "Requests Per SSL Protocol"
                    , "requests/s"
                    , "logsmanagement.plugin"
                    , NULL
                    , NETDATA_CHART_PRIO_SSL_PROTO
                    , localhost->rrd_update_every
                    , RRDSET_TYPE_AREA
            );
            chart_data_arr[i]->dim_ssl_proto_tlsv1 = rrddim_add(chart_data_arr[i]->st_ssl_proto, "TLSV1", NULL, 1, 1, RRD_ALGORITHM_INCREMENTAL);
            chart_data_arr[i]->dim_ssl_proto_tlsv1_1 = rrddim_add(chart_data_arr[i]->st_ssl_proto, "TLSV1.1", NULL, 1, 1, RRD_ALGORITHM_INCREMENTAL);
            chart_data_arr[i]->dim_ssl_proto_tlsv1_2 = rrddim_add(chart_data_arr[i]->st_ssl_proto, "TLSV1.2", NULL, 1, 1, RRD_ALGORITHM_INCREMENTAL);
            chart_data_arr[i]->dim_ssl_proto_tlsv1_3 = rrddim_add(chart_data_arr[i]->st_ssl_proto, "TLSV1.3", NULL, 1, 1, RRD_ALGORITHM_INCREMENTAL);
            chart_data_arr[i]->dim_ssl_proto_sslv2 = rrddim_add(chart_data_arr[i]->st_ssl_proto, "SSLV2", NULL, 1, 1, RRD_ALGORITHM_INCREMENTAL);
            chart_data_arr[i]->dim_ssl_proto_sslv3 = rrddim_add(chart_data_arr[i]->st_ssl_proto, "SSLV3", NULL, 1, 1, RRD_ALGORITHM_INCREMENTAL);
            chart_data_arr[i]->dim_ssl_proto_other = rrddim_add(chart_data_arr[i]->st_ssl_proto, "other", NULL, 1, 1, RRD_ALGORITHM_INCREMENTAL);
        }

        /* SSL cipher suite - initialise */
        if(p_file_info->parser_config->chart_config & CHART_SSL_CIPHER){
            chart_data_arr[i]->st_ssl_cipher = rrdset_create_localhost(
                    chart_data_arr[i]->rrd_type
                    , "ssl cipher suite"
                    , NULL
                    , "ssl cipher suite"
                    , NULL
                    , "Requests by SSL cipher suite"
                    , "requests/s"
                    , "logsmanagement.plugin"
                    , NULL
                    , NETDATA_CHART_PRIO_SSL_CIPHER_SUITE
                    , localhost->rrd_update_every
                    , RRDSET_TYPE_AREA
            );
        }


        uv_mutex_lock(p_file_info->parser_mut);

        /* Number of lines - collect first time */
        chart_data_arr[i]->num_lines_total = p_file_info->parser_metrics->num_lines_total;
        chart_data_arr[i]->num_lines_rate = p_file_info->parser_metrics->num_lines_rate;
        p_file_info->parser_metrics->num_lines_rate = 0;

        /* Vhost - collect first time */
        if(p_file_info->parser_config->chart_config & CHART_VHOST){
            for(int j = 0; j < p_file_info->parser_metrics->vhost_arr.size; j++){
                int k;
                for(k = 0; k < chart_data_arr[i]->vhost_size; k++){
                    if(!strcmp(p_file_info->parser_metrics->vhost_arr.vhosts[j].name, chart_data_arr[i]->dim_vhosts[k]->name)){
                        chart_data_arr[i]->num_vhosts[k] = p_file_info->parser_metrics->vhost_arr.vhosts[j].count;
                        p_file_info->parser_metrics->vhost_arr.vhosts[j].count = 0;
                        break;
                    }
                }
                if(chart_data_arr[i]->vhost_size == k){ // New vhost not in existing dimensions
                    chart_data_arr[i]->vhost_size++;

                    if(chart_data_arr[i]->vhost_size >= chart_data_arr[i]->vhost_size_max){
                        chart_data_arr[i]->vhost_size_max = chart_data_arr[i]->vhost_size * LOG_PARSER_METRICS_VHOST_BUFFS_SCALE_FACTOR + 1;

                        chart_data_arr[i]->dim_vhosts = reallocz(chart_data_arr[i]->dim_vhosts, chart_data_arr[i]->vhost_size_max * sizeof(RRDDIM));
                        chart_data_arr[i]->num_vhosts = reallocz(chart_data_arr[i]->num_vhosts, chart_data_arr[i]->vhost_size_max * sizeof(collected_number));
                    }
                    
                    chart_data_arr[i]->dim_vhosts[chart_data_arr[i]->vhost_size - 1] = rrddim_add(chart_data_arr[i]->st_vhost, 
                        p_file_info->parser_metrics->vhost_arr.vhosts[j].name, NULL, 1, 1, RRD_ALGORITHM_INCREMENTAL);
                    
                    chart_data_arr[i]->num_vhosts[chart_data_arr[i]->vhost_size - 1] = p_file_info->parser_metrics->vhost_arr.vhosts[j].count;
                    p_file_info->parser_metrics->vhost_arr.vhosts[j].count = 0;
                }
            }
        }

        /* Port - collect first time */
        if(p_file_info->parser_config->chart_config & CHART_PORT){
            for(int j = 0; j < p_file_info->parser_metrics->port_arr.size; j++){
                int k;
                for(k = 0; k < chart_data_arr[i]->port_size; k++){
                    if(p_file_info->parser_metrics->port_arr.ports[j].port == chart_data_arr[i]->ports[k]){
                        chart_data_arr[i]->num_ports[k] = p_file_info->parser_metrics->port_arr.ports[j].count;
                        p_file_info->parser_metrics->port_arr.ports[j].count = 0;
                        break;
                    }
                }
                if(chart_data_arr[i]->port_size == k){ // New port not in existing dimensions
                    chart_data_arr[i]->port_size++;

                    if(chart_data_arr[i]->port_size >= chart_data_arr[i]->port_size_max){
                        chart_data_arr[i]->port_size_max = chart_data_arr[i]->port_size * LOG_PARSER_METRICS_PORT_BUFFS_SCALE_FACTOR + 1;

                        chart_data_arr[i]->ports = reallocz(chart_data_arr[i]->ports, chart_data_arr[i]->port_size_max * sizeof(int));
                        chart_data_arr[i]->dim_ports = reallocz(chart_data_arr[i]->dim_ports, chart_data_arr[i]->port_size_max * sizeof(RRDDIM));
                        chart_data_arr[i]->num_ports = reallocz(chart_data_arr[i]->num_ports, chart_data_arr[i]->port_size_max * sizeof(collected_number));
                    }

                    chart_data_arr[i]->ports[chart_data_arr[i]->port_size - 1] = p_file_info->parser_metrics->port_arr.ports[j].port;

                    if(unlikely(chart_data_arr[i]->ports[chart_data_arr[i]->port_size - 1] == INVALID_PORT)){
                        chart_data_arr[i]->dim_ports[chart_data_arr[i]->port_size - 1] = rrddim_add(chart_data_arr[i]->st_port, 
                            "invalid", NULL, 1, 1, RRD_ALGORITHM_INCREMENTAL);
                    } else {
                        char port_name[6] = "";
                        snprintf(port_name, 6, "%d", chart_data_arr[i]->ports[chart_data_arr[i]->port_size - 1]);
                        chart_data_arr[i]->dim_ports[chart_data_arr[i]->port_size - 1] = rrddim_add(chart_data_arr[i]->st_port, 
                            port_name, NULL, 1, 1, RRD_ALGORITHM_INCREMENTAL);
                    }
                    
                    chart_data_arr[i]->num_ports[chart_data_arr[i]->port_size - 1] = p_file_info->parser_metrics->port_arr.ports[j].count;
                    p_file_info->parser_metrics->port_arr.ports[j].count = 0;
                }
            }
        }

        /* IP Version - collect first time */
        if(p_file_info->parser_config->chart_config & CHART_IP_VERSION){
            chart_data_arr[i]->num_ip_ver_4 = p_file_info->parser_metrics->ip_ver.v4;
            p_file_info->parser_metrics->ip_ver.v4 = 0;
            chart_data_arr[i]->num_ip_ver_6 = p_file_info->parser_metrics->ip_ver.v6;
            p_file_info->parser_metrics->ip_ver.v6 = 0;
            chart_data_arr[i]->num_ip_ver_invalid = p_file_info->parser_metrics->ip_ver.invalid;
            p_file_info->parser_metrics->ip_ver.invalid = 0;
        }

        /* Request client current poll - collect first time */
        if(p_file_info->parser_config->chart_config & CHART_REQ_CLIENT_CURRENT){
            chart_data_arr[i]->num_req_client_current_ipv4 = p_file_info->parser_metrics->req_clients_current_arr.ipv4_size;
            p_file_info->parser_metrics->req_clients_current_arr.ipv4_size = 0;  
            chart_data_arr[i]->num_req_client_current_ipv6 = p_file_info->parser_metrics->req_clients_current_arr.ipv6_size;
            p_file_info->parser_metrics->req_clients_current_arr.ipv6_size = 0;
        }

        /* Request client all-time - collect first time */
        if(p_file_info->parser_config->chart_config & CHART_REQ_CLIENT_ALL_TIME){
            chart_data_arr[i]->num_req_client_all_time_ipv4 = p_file_info->parser_metrics->req_clients_alltime_arr.ipv4_size;  
            chart_data_arr[i]->num_req_client_all_time_ipv6 = p_file_info->parser_metrics->req_clients_alltime_arr.ipv6_size;
        }

        /* Request methods - collect first time */
        if(p_file_info->parser_config->chart_config & CHART_REQ_METHODS){
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
        }

        /* Request protocol - collect first time */
        if(p_file_info->parser_config->chart_config & CHART_REQ_PROTO){
            chart_data_arr[i]->num_req_proto_http_1 = p_file_info->parser_metrics->req_proto.http_1;
            p_file_info->parser_metrics->req_proto.http_1 = 0;
            chart_data_arr[i]->num_req_proto_http_1_1 = p_file_info->parser_metrics->req_proto.http_1_1;
            p_file_info->parser_metrics->req_proto.http_1_1 = 0;
            chart_data_arr[i]->num_req_proto_http_2 = p_file_info->parser_metrics->req_proto.http_2;
            p_file_info->parser_metrics->req_proto.http_2 = 0;
            chart_data_arr[i]->num_req_proto_other = p_file_info->parser_metrics->req_proto.other;
            p_file_info->parser_metrics->req_proto.other = 0;
        }

        /* Request bandwidth - collect first time */
        if(p_file_info->parser_config->chart_config & CHART_BANDWIDTH){
            chart_data_arr[i]->num_bandwidth_req_size = p_file_info->parser_metrics->bandwidth.req_size;
            p_file_info->parser_metrics->bandwidth.req_size = 0;
            chart_data_arr[i]->num_bandwidth_resp_size = p_file_info->parser_metrics->bandwidth.resp_size;
            p_file_info->parser_metrics->bandwidth.resp_size = 0;
        }

        /* Request proc time - collect first time */
        if(p_file_info->parser_config->chart_config & CHART_REQ_PROC_TIME){
            chart_data_arr[i]->num_req_proc_time_min = p_file_info->parser_metrics->req_proc_time.min;
            p_file_info->parser_metrics->req_proc_time.min = 0;
            chart_data_arr[i]->num_req_proc_time_max = p_file_info->parser_metrics->req_proc_time.max;
            p_file_info->parser_metrics->req_proc_time.max = 0;
            chart_data_arr[i]->num_req_proc_time_avg = p_file_info->parser_metrics->req_proc_time.count ? 
                    p_file_info->parser_metrics->req_proc_time.sum / p_file_info->parser_metrics->req_proc_time.count : 0;
            p_file_info->parser_metrics->req_proc_time.sum = 0;
            p_file_info->parser_metrics->req_proc_time.count = 0;
        }

        /* Response code family - collect first time */
        if(p_file_info->parser_config->chart_config & CHART_RESP_CODE_FAMILY){
            chart_data_arr[i]->num_resp_code_family_1xx = p_file_info->parser_metrics->resp_code_family.resp_1xx;
            p_file_info->parser_metrics->resp_code_family.resp_1xx = 0;
            chart_data_arr[i]->num_resp_code_family_2xx = p_file_info->parser_metrics->resp_code_family.resp_2xx;
            p_file_info->parser_metrics->resp_code_family.resp_2xx = 0;
            chart_data_arr[i]->num_resp_code_family_3xx = p_file_info->parser_metrics->resp_code_family.resp_3xx;
            p_file_info->parser_metrics->resp_code_family.resp_3xx = 0;
            chart_data_arr[i]->num_resp_code_family_4xx = p_file_info->parser_metrics->resp_code_family.resp_4xx;
            p_file_info->parser_metrics->resp_code_family.resp_4xx = 0;
            chart_data_arr[i]->num_resp_code_family_5xx = p_file_info->parser_metrics->resp_code_family.resp_5xx;
            p_file_info->parser_metrics->resp_code_family.resp_5xx = 0;
            chart_data_arr[i]->num_resp_code_family_other = p_file_info->parser_metrics->resp_code_family.other;
            p_file_info->parser_metrics->resp_code_family.other = 0;
        }

        /* Response code - collect first time */
        if(p_file_info->parser_config->chart_config & CHART_RESP_CODE){
            for(int j = 0; j < 501; j++){
                chart_data_arr[i]->num_resp_code[j] = p_file_info->parser_metrics->resp_code[j];
                p_file_info->parser_metrics->resp_code[j] = 0;
            }
        }

        /* Response code type - collect first time */
        if(p_file_info->parser_config->chart_config & CHART_RESP_CODE_TYPE){
            chart_data_arr[i]->num_resp_code_type_success = p_file_info->parser_metrics->resp_code_type.resp_success;
            p_file_info->parser_metrics->resp_code_type.resp_success = 0;
            chart_data_arr[i]->num_resp_code_type_redirect = p_file_info->parser_metrics->resp_code_type.resp_redirect;
            p_file_info->parser_metrics->resp_code_type.resp_redirect = 0;
            chart_data_arr[i]->num_resp_code_type_bad = p_file_info->parser_metrics->resp_code_type.resp_bad;
            p_file_info->parser_metrics->resp_code_type.resp_bad = 0;
            chart_data_arr[i]->num_resp_code_type_error = p_file_info->parser_metrics->resp_code_type.resp_error;
            p_file_info->parser_metrics->resp_code_type.resp_error = 0;
            chart_data_arr[i]->num_resp_code_type_other = p_file_info->parser_metrics->resp_code_type.other;
            p_file_info->parser_metrics->resp_code_type.other = 0;
        }

        /* SSL protocol - collect first time */
        if(p_file_info->parser_config->chart_config & CHART_SSL_PROTO){
            chart_data_arr[i]->num_ssl_proto_tlsv1 = p_file_info->parser_metrics->ssl_proto.tlsv1;
            p_file_info->parser_metrics->ssl_proto.tlsv1 = 0;
            chart_data_arr[i]->num_ssl_proto_tlsv1_1 = p_file_info->parser_metrics->ssl_proto.tlsv1_1;
            p_file_info->parser_metrics->ssl_proto.tlsv1_1 = 0;
            chart_data_arr[i]->num_ssl_proto_tlsv1_2 = p_file_info->parser_metrics->ssl_proto.tlsv1_2;
            p_file_info->parser_metrics->ssl_proto.tlsv1_2 = 0;
            chart_data_arr[i]->num_ssl_proto_tlsv1_3 = p_file_info->parser_metrics->ssl_proto.tlsv1_3;
            p_file_info->parser_metrics->ssl_proto.tlsv1_3 = 0;
            chart_data_arr[i]->num_ssl_proto_sslv2 = p_file_info->parser_metrics->ssl_proto.sslv2;
            p_file_info->parser_metrics->ssl_proto.sslv2 = 0;
            chart_data_arr[i]->num_ssl_proto_sslv3 = p_file_info->parser_metrics->ssl_proto.sslv3;
            p_file_info->parser_metrics->ssl_proto.sslv3 = 0;
            chart_data_arr[i]->num_ssl_proto_other = p_file_info->parser_metrics->ssl_proto.other;
            p_file_info->parser_metrics->ssl_proto.other = 0;
        }

        /* SSL cipher suite - collect first time */
        if(p_file_info->parser_config->chart_config & CHART_SSL_CIPHER){
            for(int j = 0; j < p_file_info->parser_metrics->ssl_cipher_arr.size; j++){
                int k;
                for(k = 0; k < chart_data_arr[i]->ssl_cipher_size; k++){
                    if(!strcmp(p_file_info->parser_metrics->ssl_cipher_arr.ssl_ciphers[j].string, chart_data_arr[i]->dim_ssl_ciphers[k]->name)){
                        chart_data_arr[i]->num_ssl_ciphers[k] = p_file_info->parser_metrics->ssl_cipher_arr.ssl_ciphers[j].count;
                        p_file_info->parser_metrics->ssl_cipher_arr.ssl_ciphers[j].count = 0;
                        break;
                    }
                }
                if(chart_data_arr[i]->ssl_cipher_size == k){ // New SSL cipher suite not in existing dimensions
                    chart_data_arr[i]->ssl_cipher_size++;

                    if(chart_data_arr[i]->ssl_cipher_size >= chart_data_arr[i]->ssl_cipher_size_max){
                        chart_data_arr[i]->ssl_cipher_size_max = chart_data_arr[i]->ssl_cipher_size * LOG_PARSER_METRICS_SLL_CIPHER_BUFFS_SCALE_FACTOR + 1;

                        chart_data_arr[i]->dim_ssl_ciphers = reallocz(chart_data_arr[i]->dim_ssl_ciphers, chart_data_arr[i]->ssl_cipher_size_max * sizeof(RRDDIM));
                        chart_data_arr[i]->num_ssl_ciphers = reallocz(chart_data_arr[i]->num_ssl_ciphers, chart_data_arr[i]->ssl_cipher_size_max * sizeof(collected_number));
                    }
                    
                    chart_data_arr[i]->dim_ssl_ciphers[chart_data_arr[i]->ssl_cipher_size - 1] = rrddim_add(chart_data_arr[i]->st_ssl_cipher, 
                        p_file_info->parser_metrics->ssl_cipher_arr.ssl_ciphers[j].string, NULL, 1, 1, RRD_ALGORITHM_INCREMENTAL);
                    
                    chart_data_arr[i]->num_ssl_ciphers[chart_data_arr[i]->ssl_cipher_size - 1] = p_file_info->parser_metrics->ssl_cipher_arr.ssl_ciphers[j].count;
                    p_file_info->parser_metrics->ssl_cipher_arr.ssl_ciphers[j].count = 0;
                }
            }
        }

        
        uv_mutex_unlock(p_file_info->parser_mut);



        /* Number of lines - update chart first time */
        rrddim_set_by_pointer(chart_data_arr[i]->st_lines, chart_data_arr[i]->dim_lines_total, chart_data_arr[i]->num_lines_total);
        rrddim_set_by_pointer(chart_data_arr[i]->st_lines, chart_data_arr[i]->dim_lines_rate, chart_data_arr[i]->num_lines_rate);
    	rrdset_done(chart_data_arr[i]->st_lines);

        /* Vhost - update chart first time */
        if(p_file_info->parser_config->chart_config & CHART_VHOST){
            for(int j = 0; j < chart_data_arr[i]->vhost_size; j++){
                rrddim_set_by_pointer(chart_data_arr[i]->st_vhost, chart_data_arr[i]->dim_vhosts[j], chart_data_arr[i]->num_vhosts[j]);
            }
            rrdset_done(chart_data_arr[i]->st_vhost);
        }

        /* Port - update chart first time */
        if(p_file_info->parser_config->chart_config & CHART_PORT){
            for(int j = 0; j < chart_data_arr[i]->port_size; j++){
                rrddim_set_by_pointer(chart_data_arr[i]->st_port, chart_data_arr[i]->dim_ports[j], chart_data_arr[i]->num_ports[j]);
            }
            rrdset_done(chart_data_arr[i]->st_port);
        }

        /* IP Version - update chart first time */
        if(p_file_info->parser_config->chart_config & CHART_IP_VERSION){
            rrddim_set_by_pointer(chart_data_arr[i]->st_ip_ver, chart_data_arr[i]->dim_ip_ver_4, chart_data_arr[i]->num_ip_ver_4);
            rrddim_set_by_pointer(chart_data_arr[i]->st_ip_ver, chart_data_arr[i]->dim_ip_ver_6, chart_data_arr[i]->num_ip_ver_6);
            rrddim_set_by_pointer(chart_data_arr[i]->st_ip_ver, chart_data_arr[i]->dim_ip_ver_invalid, chart_data_arr[i]->num_ip_ver_invalid);
            rrdset_done(chart_data_arr[i]->st_ip_ver);
        }

        /* Request client current poll - update chart first time */
        if(p_file_info->parser_config->chart_config & CHART_REQ_CLIENT_CURRENT){
            rrddim_set_by_pointer(chart_data_arr[i]->st_req_client_current, chart_data_arr[i]->dim_req_client_current_ipv4, chart_data_arr[i]->num_req_client_current_ipv4);
            rrddim_set_by_pointer(chart_data_arr[i]->st_req_client_current, chart_data_arr[i]->dim_req_client_current_ipv6, chart_data_arr[i]->num_req_client_current_ipv6);
            rrdset_done(chart_data_arr[i]->st_req_client_current);
        }

        /* Request client all-time - update chart first time */
        if(p_file_info->parser_config->chart_config & CHART_REQ_CLIENT_ALL_TIME){
            rrddim_set_by_pointer(chart_data_arr[i]->st_req_client_all_time, chart_data_arr[i]->dim_req_client_all_time_ipv4, chart_data_arr[i]->num_req_client_all_time_ipv4);
            rrddim_set_by_pointer(chart_data_arr[i]->st_req_client_all_time, chart_data_arr[i]->dim_req_client_all_time_ipv6, chart_data_arr[i]->num_req_client_all_time_ipv6);
            rrdset_done(chart_data_arr[i]->st_req_client_all_time);
        }

        /* Request methods - update chart first time */
        if(p_file_info->parser_config->chart_config & CHART_REQ_METHODS){
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
        }

        /* Request protocol - update chart first time */
        if(p_file_info->parser_config->chart_config & CHART_REQ_PROTO){
            rrddim_set_by_pointer(chart_data_arr[i]->st_req_proto, chart_data_arr[i]->dim_req_proto_http_1, chart_data_arr[i]->num_req_proto_http_1);
            rrddim_set_by_pointer(chart_data_arr[i]->st_req_proto, chart_data_arr[i]->dim_req_proto_http_1_1, chart_data_arr[i]->num_req_proto_http_1_1);
            rrddim_set_by_pointer(chart_data_arr[i]->st_req_proto, chart_data_arr[i]->dim_req_proto_http_2, chart_data_arr[i]->num_req_proto_http_2);
            rrddim_set_by_pointer(chart_data_arr[i]->st_req_proto, chart_data_arr[i]->dim_req_proto_other, chart_data_arr[i]->num_req_proto_other);
            rrdset_done(chart_data_arr[i]->st_req_proto);
        }

        /* Request bandwidth - update chart first time */
        if(p_file_info->parser_config->chart_config & CHART_BANDWIDTH){
            rrddim_set_by_pointer(chart_data_arr[i]->st_bandwidth, chart_data_arr[i]->dim_bandwidth_req_size, chart_data_arr[i]->num_bandwidth_req_size);
            rrddim_set_by_pointer(chart_data_arr[i]->st_bandwidth, chart_data_arr[i]->dim_bandwidth_resp_size, chart_data_arr[i]->num_bandwidth_resp_size);
            rrdset_done(chart_data_arr[i]->st_bandwidth);
        }

        /* Request proc time - update chart first time */
        if(p_file_info->parser_config->chart_config & CHART_REQ_PROC_TIME){
            rrddim_set_by_pointer(chart_data_arr[i]->st_req_proc_time, chart_data_arr[i]->dim_req_proc_time_min, chart_data_arr[i]->num_req_proc_time_min);
            rrddim_set_by_pointer(chart_data_arr[i]->st_req_proc_time, chart_data_arr[i]->dim_req_proc_time_max, chart_data_arr[i]->num_req_proc_time_max);
            rrddim_set_by_pointer(chart_data_arr[i]->st_req_proc_time, chart_data_arr[i]->dim_req_proc_time_avg, chart_data_arr[i]->num_req_proc_time_avg);
            rrdset_done(chart_data_arr[i]->st_req_proc_time);
        }

        /* Response code family - update chart first time */
        if(p_file_info->parser_config->chart_config & CHART_RESP_CODE_FAMILY){
            rrddim_set_by_pointer(chart_data_arr[i]->st_resp_code_family, chart_data_arr[i]->dim_resp_code_family_1xx, chart_data_arr[i]->num_resp_code_family_1xx);
            rrddim_set_by_pointer(chart_data_arr[i]->st_resp_code_family, chart_data_arr[i]->dim_resp_code_family_2xx, chart_data_arr[i]->num_resp_code_family_2xx);
            rrddim_set_by_pointer(chart_data_arr[i]->st_resp_code_family, chart_data_arr[i]->dim_resp_code_family_3xx, chart_data_arr[i]->num_resp_code_family_3xx);
            rrddim_set_by_pointer(chart_data_arr[i]->st_resp_code_family, chart_data_arr[i]->dim_resp_code_family_4xx, chart_data_arr[i]->num_resp_code_family_4xx);
            rrddim_set_by_pointer(chart_data_arr[i]->st_resp_code_family, chart_data_arr[i]->dim_resp_code_family_5xx, chart_data_arr[i]->num_resp_code_family_5xx);
            rrddim_set_by_pointer(chart_data_arr[i]->st_resp_code_family, chart_data_arr[i]->dim_resp_code_family_other, chart_data_arr[i]->num_resp_code_family_other);
            rrdset_done(chart_data_arr[i]->st_resp_code_family);
        }

        /* Response code - update chart first time */
        if(p_file_info->parser_config->chart_config & CHART_RESP_CODE){
            for(int j = 0; j < 501; j++) rrddim_set_by_pointer(chart_data_arr[i]->st_resp_code, chart_data_arr[i]->dim_resp_code[j], chart_data_arr[i]->num_resp_code[j]);
            rrdset_done(chart_data_arr[i]->st_resp_code);
        }

        /* Response code type - update chart first time */
        if(p_file_info->parser_config->chart_config & CHART_RESP_CODE_TYPE){
            rrddim_set_by_pointer(chart_data_arr[i]->st_resp_code_type, chart_data_arr[i]->dim_resp_code_type_success, chart_data_arr[i]->num_resp_code_type_success);
            rrddim_set_by_pointer(chart_data_arr[i]->st_resp_code_type, chart_data_arr[i]->dim_resp_code_type_redirect, chart_data_arr[i]->num_resp_code_type_redirect);
            rrddim_set_by_pointer(chart_data_arr[i]->st_resp_code_type, chart_data_arr[i]->dim_resp_code_type_bad, chart_data_arr[i]->num_resp_code_type_bad);
            rrddim_set_by_pointer(chart_data_arr[i]->st_resp_code_type, chart_data_arr[i]->dim_resp_code_type_error, chart_data_arr[i]->num_resp_code_type_error);
            rrddim_set_by_pointer(chart_data_arr[i]->st_resp_code_type, chart_data_arr[i]->dim_resp_code_type_other, chart_data_arr[i]->num_resp_code_type_other);
            rrdset_done(chart_data_arr[i]->st_resp_code_type);
        }

        /* SSL protocol - update chart first time */
        if(p_file_info->parser_config->chart_config & CHART_SSL_PROTO){
            rrddim_set_by_pointer(chart_data_arr[i]->st_ssl_proto, chart_data_arr[i]->dim_ssl_proto_tlsv1, chart_data_arr[i]->num_ssl_proto_tlsv1);
            rrddim_set_by_pointer(chart_data_arr[i]->st_ssl_proto, chart_data_arr[i]->dim_ssl_proto_tlsv1_1, chart_data_arr[i]->num_ssl_proto_tlsv1_1);
            rrddim_set_by_pointer(chart_data_arr[i]->st_ssl_proto, chart_data_arr[i]->dim_ssl_proto_tlsv1_2, chart_data_arr[i]->num_ssl_proto_tlsv1_2);
            rrddim_set_by_pointer(chart_data_arr[i]->st_ssl_proto, chart_data_arr[i]->dim_ssl_proto_tlsv1_3, chart_data_arr[i]->num_ssl_proto_tlsv1_3);
            rrddim_set_by_pointer(chart_data_arr[i]->st_ssl_proto, chart_data_arr[i]->dim_ssl_proto_sslv2, chart_data_arr[i]->num_ssl_proto_sslv2);
            rrddim_set_by_pointer(chart_data_arr[i]->st_ssl_proto, chart_data_arr[i]->dim_ssl_proto_sslv3, chart_data_arr[i]->num_ssl_proto_sslv3);
            rrddim_set_by_pointer(chart_data_arr[i]->st_ssl_proto, chart_data_arr[i]->dim_ssl_proto_other, chart_data_arr[i]->num_ssl_proto_other);
            rrdset_done(chart_data_arr[i]->st_ssl_proto);
        }

        /* SSL cipher suite - update chart first time */
        if(p_file_info->parser_config->chart_config & CHART_SSL_CIPHER){
            for(int j = 0; j < chart_data_arr[i]->ssl_cipher_size; j++){
                rrddim_set_by_pointer(chart_data_arr[i]->st_ssl_cipher, chart_data_arr[i]->dim_ssl_ciphers[j], chart_data_arr[i]->num_ssl_ciphers[j]);
            }
            rrdset_done(chart_data_arr[i]->st_ssl_cipher);
        }

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
            chart_data_arr[i]->num_lines_total = p_file_info->parser_metrics->num_lines_total;
            // p_file_info->parser_metrics->num_lines_total = 0;
            chart_data_arr[i]->num_lines_rate += p_file_info->parser_metrics->num_lines_rate;
            p_file_info->parser_metrics->num_lines_rate = 0;

            /* Vhost - collect */
            if(p_file_info->parser_config->chart_config & CHART_VHOST){
                for(int j = 0; j < p_file_info->parser_metrics->vhost_arr.size; j++){
                    int k;
                    for(k = 0; k < chart_data_arr[i]->vhost_size; k++){
                        if(!strcmp(p_file_info->parser_metrics->vhost_arr.vhosts[j].name, chart_data_arr[i]->dim_vhosts[k]->name)){
                            chart_data_arr[i]->num_vhosts[k] += p_file_info->parser_metrics->vhost_arr.vhosts[j].count;
                            p_file_info->parser_metrics->vhost_arr.vhosts[j].count = 0;
                            break;
                        }
                    }
                    if(chart_data_arr[i]->vhost_size == k){ // New vhost not in existing dimensions
                        chart_data_arr[i]->vhost_size++;

                        if(chart_data_arr[i]->vhost_size >= chart_data_arr[i]->vhost_size_max){
                            chart_data_arr[i]->vhost_size_max = chart_data_arr[i]->vhost_size * LOG_PARSER_METRICS_VHOST_BUFFS_SCALE_FACTOR + 1;

                            chart_data_arr[i]->dim_vhosts = reallocz(chart_data_arr[i]->dim_vhosts, chart_data_arr[i]->vhost_size_max * sizeof(RRDDIM));
                            chart_data_arr[i]->num_vhosts = reallocz(chart_data_arr[i]->num_vhosts, chart_data_arr[i]->vhost_size_max * sizeof(collected_number));
                        }
                        
                        chart_data_arr[i]->dim_vhosts[chart_data_arr[i]->vhost_size - 1] = rrddim_add(chart_data_arr[i]->st_vhost, 
                            p_file_info->parser_metrics->vhost_arr.vhosts[j].name, NULL, 1, 1, RRD_ALGORITHM_INCREMENTAL);
                        
                        chart_data_arr[i]->num_vhosts[chart_data_arr[i]->vhost_size - 1] = p_file_info->parser_metrics->vhost_arr.vhosts[j].count;
                    }
                }
            }

            /* Port - collect */
            if(p_file_info->parser_config->chart_config & CHART_PORT){
                for(int j = 0; j < p_file_info->parser_metrics->port_arr.size; j++){
                    int k;
                    for(k = 0; k < chart_data_arr[i]->port_size; k++){
                        if(p_file_info->parser_metrics->port_arr.ports[j].port == chart_data_arr[i]->ports[k]){
                            chart_data_arr[i]->num_ports[k] += p_file_info->parser_metrics->port_arr.ports[j].count;
                            p_file_info->parser_metrics->port_arr.ports[j].count = 0;
                            break;
                        }
                    }
                    if(chart_data_arr[i]->port_size == k){ // New port not in existing dimensions
                        chart_data_arr[i]->port_size++;

                        if(chart_data_arr[i]->port_size >= chart_data_arr[i]->port_size_max){
                            chart_data_arr[i]->port_size_max = chart_data_arr[i]->port_size * LOG_PARSER_METRICS_PORT_BUFFS_SCALE_FACTOR + 1;

                            chart_data_arr[i]->ports = reallocz(chart_data_arr[i]->ports, chart_data_arr[i]->port_size_max * sizeof(int));
                            chart_data_arr[i]->dim_ports = reallocz(chart_data_arr[i]->dim_ports, chart_data_arr[i]->port_size_max * sizeof(RRDDIM));
                            chart_data_arr[i]->num_ports = reallocz(chart_data_arr[i]->num_ports, chart_data_arr[i]->port_size_max * sizeof(collected_number));
                        }

                        chart_data_arr[i]->ports[chart_data_arr[i]->port_size - 1] = p_file_info->parser_metrics->port_arr.ports[j].port;

                        if(unlikely(chart_data_arr[i]->ports[chart_data_arr[i]->port_size - 1] == INVALID_PORT)){
                        chart_data_arr[i]->dim_ports[chart_data_arr[i]->port_size - 1] = rrddim_add(chart_data_arr[i]->st_port, 
                            "invalid", NULL, 1, 1, RRD_ALGORITHM_INCREMENTAL);
                        } else {
                            char port_name[6] = "";
                            snprintf(port_name, 6, "%d", chart_data_arr[i]->ports[chart_data_arr[i]->port_size - 1]);
                            chart_data_arr[i]->dim_ports[chart_data_arr[i]->port_size - 1] = rrddim_add(chart_data_arr[i]->st_port, 
                                port_name, NULL, 1, 1, RRD_ALGORITHM_INCREMENTAL);
                        }

                        chart_data_arr[i]->num_ports[chart_data_arr[i]->port_size - 1] = p_file_info->parser_metrics->port_arr.ports[j].count;
                    }
                }
            }

            /* IP Version - collect */
            if(p_file_info->parser_config->chart_config & CHART_IP_VERSION){
                chart_data_arr[i]->num_ip_ver_4 += p_file_info->parser_metrics->ip_ver.v4;
                p_file_info->parser_metrics->ip_ver.v4 = 0;
                chart_data_arr[i]->num_ip_ver_6 += p_file_info->parser_metrics->ip_ver.v6;
                p_file_info->parser_metrics->ip_ver.v6 = 0;
                chart_data_arr[i]->num_ip_ver_invalid += p_file_info->parser_metrics->ip_ver.invalid;
                p_file_info->parser_metrics->ip_ver.invalid = 0;
            }

            /* Request client current poll - collect */
            if(p_file_info->parser_config->chart_config & CHART_REQ_CLIENT_CURRENT){
                chart_data_arr[i]->num_req_client_current_ipv4 += p_file_info->parser_metrics->req_clients_current_arr.ipv4_size;
                p_file_info->parser_metrics->req_clients_current_arr.ipv4_size = 0;  
                chart_data_arr[i]->num_req_client_current_ipv6 += p_file_info->parser_metrics->req_clients_current_arr.ipv6_size;
                p_file_info->parser_metrics->req_clients_current_arr.ipv6_size = 0;
            }

            /* Request client all-time - collect */
            if(p_file_info->parser_config->chart_config & CHART_REQ_CLIENT_ALL_TIME){
                chart_data_arr[i]->num_req_client_all_time_ipv4 = p_file_info->parser_metrics->req_clients_alltime_arr.ipv4_size;  
                chart_data_arr[i]->num_req_client_all_time_ipv6 = p_file_info->parser_metrics->req_clients_alltime_arr.ipv6_size;
            }

            /* Request methods - collect */
            if(p_file_info->parser_config->chart_config & CHART_REQ_METHODS){
                chart_data_arr[i]->num_req_method_acl += p_file_info->parser_metrics->req_method.acl;
                p_file_info->parser_metrics->req_method.acl = 0;
                chart_data_arr[i]->num_req_method_baseline_control += p_file_info->parser_metrics->req_method.baseline_control;
                p_file_info->parser_metrics->req_method.baseline_control = 0;
                chart_data_arr[i]->num_req_method_bind += p_file_info->parser_metrics->req_method.bind;
                p_file_info->parser_metrics->req_method.bind = 0;
                chart_data_arr[i]->num_req_method_checkin += p_file_info->parser_metrics->req_method.checkin;
                p_file_info->parser_metrics->req_method.checkin = 0;
                chart_data_arr[i]->num_req_method_checkout += p_file_info->parser_metrics->req_method.checkout;
                p_file_info->parser_metrics->req_method.checkout = 0;
                chart_data_arr[i]->num_req_method_connect += p_file_info->parser_metrics->req_method.connect;
                p_file_info->parser_metrics->req_method.connect = 0;
                chart_data_arr[i]->num_req_method_copy += p_file_info->parser_metrics->req_method.copy;
                p_file_info->parser_metrics->req_method.copy = 0;
                chart_data_arr[i]->num_req_method_delet += p_file_info->parser_metrics->req_method.delet;
                p_file_info->parser_metrics->req_method.delet = 0;
                chart_data_arr[i]->num_req_method_get += p_file_info->parser_metrics->req_method.get;
                p_file_info->parser_metrics->req_method.get = 0;
                chart_data_arr[i]->num_req_method_head += p_file_info->parser_metrics->req_method.head;
                p_file_info->parser_metrics->req_method.head = 0;
                chart_data_arr[i]->num_req_method_label += p_file_info->parser_metrics->req_method.label;
                p_file_info->parser_metrics->req_method.label = 0;
                chart_data_arr[i]->num_req_method_link += p_file_info->parser_metrics->req_method.link;
                p_file_info->parser_metrics->req_method.link = 0;
                chart_data_arr[i]->num_req_method_lock += p_file_info->parser_metrics->req_method.lock;
                p_file_info->parser_metrics->req_method.lock = 0;
                chart_data_arr[i]->num_req_method_merge += p_file_info->parser_metrics->req_method.merge;
                p_file_info->parser_metrics->req_method.merge = 0;
                chart_data_arr[i]->num_req_method_mkactivity += p_file_info->parser_metrics->req_method.mkactivity;
                p_file_info->parser_metrics->req_method.mkactivity = 0;
                chart_data_arr[i]->num_req_method_mkcalendar += p_file_info->parser_metrics->req_method.mkcalendar;
                p_file_info->parser_metrics->req_method.mkcalendar = 0;
                chart_data_arr[i]->num_req_method_mkcol += p_file_info->parser_metrics->req_method.mkcol;
                p_file_info->parser_metrics->req_method.mkcol = 0;
                chart_data_arr[i]->num_req_method_mkredirectref += p_file_info->parser_metrics->req_method.mkredirectref;
                p_file_info->parser_metrics->req_method.mkredirectref = 0;
                chart_data_arr[i]->num_req_method_mkworkspace += p_file_info->parser_metrics->req_method.mkworkspace;
                p_file_info->parser_metrics->req_method.mkworkspace = 0;
                chart_data_arr[i]->num_req_method_move += p_file_info->parser_metrics->req_method.move;
                p_file_info->parser_metrics->req_method.move = 0;
                chart_data_arr[i]->num_req_method_options += p_file_info->parser_metrics->req_method.options;
                p_file_info->parser_metrics->req_method.options = 0;
                chart_data_arr[i]->num_req_method_orderpatch += p_file_info->parser_metrics->req_method.orderpatch;
                p_file_info->parser_metrics->req_method.orderpatch = 0;
                chart_data_arr[i]->num_req_method_patch += p_file_info->parser_metrics->req_method.patch;
                p_file_info->parser_metrics->req_method.patch = 0;
                chart_data_arr[i]->num_req_method_post += p_file_info->parser_metrics->req_method.post;
                p_file_info->parser_metrics->req_method.post = 0;
                chart_data_arr[i]->num_req_method_pri += p_file_info->parser_metrics->req_method.pri;
                p_file_info->parser_metrics->req_method.pri = 0;
                chart_data_arr[i]->num_req_method_propfind += p_file_info->parser_metrics->req_method.propfind;
                p_file_info->parser_metrics->req_method.propfind = 0;
                chart_data_arr[i]->num_req_method_proppatch += p_file_info->parser_metrics->req_method.proppatch;
                p_file_info->parser_metrics->req_method.proppatch = 0;
                chart_data_arr[i]->num_req_method_put += p_file_info->parser_metrics->req_method.put;
                p_file_info->parser_metrics->req_method.put = 0;
                chart_data_arr[i]->num_req_method_rebind += p_file_info->parser_metrics->req_method.rebind;
                p_file_info->parser_metrics->req_method.rebind = 0;
                chart_data_arr[i]->num_req_method_report += p_file_info->parser_metrics->req_method.report;
                p_file_info->parser_metrics->req_method.report = 0;
                chart_data_arr[i]->num_req_method_search += p_file_info->parser_metrics->req_method.search;
                p_file_info->parser_metrics->req_method.search = 0;
                chart_data_arr[i]->num_req_method_trace += p_file_info->parser_metrics->req_method.trace;
                p_file_info->parser_metrics->req_method.trace = 0;
                chart_data_arr[i]->num_req_method_unbind += p_file_info->parser_metrics->req_method.unbind;
                p_file_info->parser_metrics->req_method.unbind = 0;
                chart_data_arr[i]->num_req_method_uncheckout += p_file_info->parser_metrics->req_method.uncheckout;
                p_file_info->parser_metrics->req_method.uncheckout = 0;
                chart_data_arr[i]->num_req_method_unlink += p_file_info->parser_metrics->req_method.unlink;
                p_file_info->parser_metrics->req_method.unlink = 0;
                chart_data_arr[i]->num_req_method_unlock += p_file_info->parser_metrics->req_method.unlock;
                p_file_info->parser_metrics->req_method.unlock = 0;
                chart_data_arr[i]->num_req_method_update += p_file_info->parser_metrics->req_method.update;
                p_file_info->parser_metrics->req_method.update = 0;
                chart_data_arr[i]->num_req_method_updateredirectref += p_file_info->parser_metrics->req_method.updateredirectref;
                p_file_info->parser_metrics->req_method.updateredirectref = 0;
            }

            /* Request protocol - collect */
            if(p_file_info->parser_config->chart_config & CHART_REQ_PROTO){
                chart_data_arr[i]->num_req_proto_http_1 += p_file_info->parser_metrics->req_proto.http_1;
                p_file_info->parser_metrics->req_proto.http_1 = 0;
                chart_data_arr[i]->num_req_proto_http_1_1 += p_file_info->parser_metrics->req_proto.http_1_1;
                p_file_info->parser_metrics->req_proto.http_1_1 = 0;
                chart_data_arr[i]->num_req_proto_http_2 += p_file_info->parser_metrics->req_proto.http_2;
                p_file_info->parser_metrics->req_proto.http_2 = 0;
                chart_data_arr[i]->num_req_proto_other += p_file_info->parser_metrics->req_proto.other;
                p_file_info->parser_metrics->req_proto.other = 0;
            }

            /* Request bandwidth - collect */
            if(p_file_info->parser_config->chart_config & CHART_BANDWIDTH){
                chart_data_arr[i]->num_bandwidth_req_size += p_file_info->parser_metrics->bandwidth.req_size;
                p_file_info->parser_metrics->bandwidth.req_size = 0;
                chart_data_arr[i]->num_bandwidth_resp_size += p_file_info->parser_metrics->bandwidth.resp_size;
                p_file_info->parser_metrics->bandwidth.resp_size = 0;
            }

            /* Request proc time - collect */
            if(p_file_info->parser_config->chart_config & CHART_REQ_PROC_TIME){
                chart_data_arr[i]->num_req_proc_time_min = p_file_info->parser_metrics->req_proc_time.min;
                p_file_info->parser_metrics->req_proc_time.min = 0;
                chart_data_arr[i]->num_req_proc_time_max = p_file_info->parser_metrics->req_proc_time.max;
                p_file_info->parser_metrics->req_proc_time.max = 0;
                chart_data_arr[i]->num_req_proc_time_avg = p_file_info->parser_metrics->req_proc_time.count ? 
                    p_file_info->parser_metrics->req_proc_time.sum / p_file_info->parser_metrics->req_proc_time.count : 0;
                p_file_info->parser_metrics->req_proc_time.sum = 0;
                p_file_info->parser_metrics->req_proc_time.count = 0;
            }

            /* Response code family - collect */
            if(p_file_info->parser_config->chart_config & CHART_RESP_CODE_FAMILY){
                chart_data_arr[i]->num_resp_code_family_1xx += p_file_info->parser_metrics->resp_code_family.resp_1xx;
                p_file_info->parser_metrics->resp_code_family.resp_1xx = 0;
                chart_data_arr[i]->num_resp_code_family_2xx += p_file_info->parser_metrics->resp_code_family.resp_2xx;
                p_file_info->parser_metrics->resp_code_family.resp_2xx = 0;
                chart_data_arr[i]->num_resp_code_family_3xx += p_file_info->parser_metrics->resp_code_family.resp_3xx;
                p_file_info->parser_metrics->resp_code_family.resp_3xx = 0;
                chart_data_arr[i]->num_resp_code_family_4xx += p_file_info->parser_metrics->resp_code_family.resp_4xx;
                p_file_info->parser_metrics->resp_code_family.resp_4xx = 0;
                chart_data_arr[i]->num_resp_code_family_5xx += p_file_info->parser_metrics->resp_code_family.resp_5xx;
                p_file_info->parser_metrics->resp_code_family.resp_5xx = 0;
                chart_data_arr[i]->num_resp_code_family_other += p_file_info->parser_metrics->resp_code_family.other;
                p_file_info->parser_metrics->resp_code_family.other = 0;
            }

            /* Response code - collect */
            if(p_file_info->parser_config->chart_config & CHART_RESP_CODE){
                for(int j = 0; j < 501; j++){
                    chart_data_arr[i]->num_resp_code[j] += p_file_info->parser_metrics->resp_code[j];
                    p_file_info->parser_metrics->resp_code[j] = 0;
                }
            }

            /* Response code type - collect */
            if(p_file_info->parser_config->chart_config & CHART_RESP_CODE_TYPE){
                chart_data_arr[i]->num_resp_code_type_success += p_file_info->parser_metrics->resp_code_type.resp_success;
                p_file_info->parser_metrics->resp_code_type.resp_success = 0;
                chart_data_arr[i]->num_resp_code_type_redirect += p_file_info->parser_metrics->resp_code_type.resp_redirect;
                p_file_info->parser_metrics->resp_code_type.resp_redirect = 0;
                chart_data_arr[i]->num_resp_code_type_bad += p_file_info->parser_metrics->resp_code_type.resp_bad;
                p_file_info->parser_metrics->resp_code_type.resp_bad = 0;
                chart_data_arr[i]->num_resp_code_type_error += p_file_info->parser_metrics->resp_code_type.resp_error;
                p_file_info->parser_metrics->resp_code_type.resp_error = 0;
                chart_data_arr[i]->num_resp_code_type_other += p_file_info->parser_metrics->resp_code_type.other;
                p_file_info->parser_metrics->resp_code_type.other = 0;
            }

            /* SSL protocol - collect */
            if(p_file_info->parser_config->chart_config & CHART_SSL_PROTO){
                chart_data_arr[i]->num_ssl_proto_tlsv1 += p_file_info->parser_metrics->ssl_proto.tlsv1;
                p_file_info->parser_metrics->ssl_proto.tlsv1 = 0;
                chart_data_arr[i]->num_ssl_proto_tlsv1_1 += p_file_info->parser_metrics->ssl_proto.tlsv1_1;
                p_file_info->parser_metrics->ssl_proto.tlsv1_1 = 0;
                chart_data_arr[i]->num_ssl_proto_tlsv1_2 += p_file_info->parser_metrics->ssl_proto.tlsv1_2;
                p_file_info->parser_metrics->ssl_proto.tlsv1_2 = 0;
                chart_data_arr[i]->num_ssl_proto_tlsv1_3 += p_file_info->parser_metrics->ssl_proto.tlsv1_3;
                p_file_info->parser_metrics->ssl_proto.tlsv1_3 = 0;
                chart_data_arr[i]->num_ssl_proto_sslv2 += p_file_info->parser_metrics->ssl_proto.sslv2;
                p_file_info->parser_metrics->ssl_proto.sslv2 = 0;
                chart_data_arr[i]->num_ssl_proto_sslv3 += p_file_info->parser_metrics->ssl_proto.sslv3;
                p_file_info->parser_metrics->ssl_proto.sslv3 = 0;
                chart_data_arr[i]->num_ssl_proto_other += p_file_info->parser_metrics->ssl_proto.other;
                p_file_info->parser_metrics->ssl_proto.other = 0;
            }

            /* SSL cipher suite - collect */
            if(p_file_info->parser_config->chart_config & CHART_SSL_CIPHER){
                for(int j = 0; j < p_file_info->parser_metrics->ssl_cipher_arr.size; j++){
                    int k;
                    for(k = 0; k < chart_data_arr[i]->ssl_cipher_size; k++){
                        if(!strcmp(p_file_info->parser_metrics->ssl_cipher_arr.ssl_ciphers[j].string, chart_data_arr[i]->dim_ssl_ciphers[k]->name)){
                            chart_data_arr[i]->num_ssl_ciphers[k] += p_file_info->parser_metrics->ssl_cipher_arr.ssl_ciphers[j].count;
                            p_file_info->parser_metrics->ssl_cipher_arr.ssl_ciphers[j].count = 0;
                            break;
                        }
                    }
                    if(chart_data_arr[i]->ssl_cipher_size == k){ // New SSL cipher suite not in existing dimensions
                        chart_data_arr[i]->ssl_cipher_size++;

                        if(chart_data_arr[i]->ssl_cipher_size >= chart_data_arr[i]->ssl_cipher_size_max){
                            chart_data_arr[i]->ssl_cipher_size_max = chart_data_arr[i]->ssl_cipher_size * LOG_PARSER_METRICS_SLL_CIPHER_BUFFS_SCALE_FACTOR + 1;

                            chart_data_arr[i]->dim_ssl_ciphers = reallocz(chart_data_arr[i]->dim_ssl_ciphers, chart_data_arr[i]->ssl_cipher_size_max * sizeof(RRDDIM));
                            chart_data_arr[i]->num_ssl_ciphers = reallocz(chart_data_arr[i]->num_ssl_ciphers, chart_data_arr[i]->ssl_cipher_size_max * sizeof(collected_number));
                        }
                        
                        chart_data_arr[i]->dim_ssl_ciphers[chart_data_arr[i]->ssl_cipher_size - 1] = rrddim_add(chart_data_arr[i]->st_ssl_cipher, 
                            p_file_info->parser_metrics->ssl_cipher_arr.ssl_ciphers[j].string, NULL, 1, 1, RRD_ALGORITHM_INCREMENTAL);
                        
                        chart_data_arr[i]->num_ssl_ciphers[chart_data_arr[i]->ssl_cipher_size - 1] = p_file_info->parser_metrics->ssl_cipher_arr.ssl_ciphers[j].count;
                        p_file_info->parser_metrics->ssl_cipher_arr.ssl_ciphers[j].count = 0;
                    }
                }
            }
            
            uv_mutex_unlock(p_file_info->parser_mut);

            if(unlikely(netdata_exit)) break;

            /* Number of lines - update chart */
    		rrdset_next(chart_data_arr[i]->st_lines);
    		rrddim_set_by_pointer(chart_data_arr[i]->st_lines, chart_data_arr[i]->dim_lines_total, chart_data_arr[i]->num_lines_total);
            rrddim_set_by_pointer(chart_data_arr[i]->st_lines, chart_data_arr[i]->dim_lines_rate, chart_data_arr[i]->num_lines_rate);
            rrdset_done(chart_data_arr[i]->st_lines);

            /* Vhost - update chart */
            if(p_file_info->parser_config->chart_config & CHART_VHOST){
                rrdset_next(chart_data_arr[i]->st_vhost);
                for(int j = 0; j < chart_data_arr[i]->vhost_size; j++){
                    rrddim_set_by_pointer(chart_data_arr[i]->st_vhost, chart_data_arr[i]->dim_vhosts[j], chart_data_arr[i]->num_vhosts[j]);
                }
                rrdset_done(chart_data_arr[i]->st_vhost);
            }

            /* Port - update chart */
            if(p_file_info->parser_config->chart_config & CHART_PORT){
                rrdset_next(chart_data_arr[i]->st_port);
                for(int j = 0; j < chart_data_arr[i]->port_size; j++){
                    rrddim_set_by_pointer(chart_data_arr[i]->st_port, chart_data_arr[i]->dim_ports[j], chart_data_arr[i]->num_ports[j]);
                }
                rrdset_done(chart_data_arr[i]->st_port);
            }

            /* IP Version - update chart */
            if(p_file_info->parser_config->chart_config & CHART_IP_VERSION){
                rrdset_next(chart_data_arr[i]->st_ip_ver);
                rrddim_set_by_pointer(chart_data_arr[i]->st_ip_ver, chart_data_arr[i]->dim_ip_ver_4, chart_data_arr[i]->num_ip_ver_4);
                rrddim_set_by_pointer(chart_data_arr[i]->st_ip_ver, chart_data_arr[i]->dim_ip_ver_6, chart_data_arr[i]->num_ip_ver_6);
                rrddim_set_by_pointer(chart_data_arr[i]->st_ip_ver, chart_data_arr[i]->dim_ip_ver_invalid, chart_data_arr[i]->num_ip_ver_invalid);
                rrdset_done(chart_data_arr[i]->st_ip_ver);
            }

            /* Request client current poll - update chart */
            if(p_file_info->parser_config->chart_config & CHART_REQ_CLIENT_CURRENT){
                rrdset_next(chart_data_arr[i]->st_req_client_current);
                rrddim_set_by_pointer(chart_data_arr[i]->st_req_client_current, chart_data_arr[i]->dim_req_client_current_ipv4, chart_data_arr[i]->num_req_client_current_ipv4);
                rrddim_set_by_pointer(chart_data_arr[i]->st_req_client_current, chart_data_arr[i]->dim_req_client_current_ipv6, chart_data_arr[i]->num_req_client_current_ipv6);
                rrdset_done(chart_data_arr[i]->st_req_client_current);
            }

            /* Request client all-time - update chart */
            if(p_file_info->parser_config->chart_config & CHART_REQ_CLIENT_ALL_TIME){
                rrdset_next(chart_data_arr[i]->st_req_client_all_time);
                rrddim_set_by_pointer(chart_data_arr[i]->st_req_client_all_time, chart_data_arr[i]->dim_req_client_all_time_ipv4, chart_data_arr[i]->num_req_client_all_time_ipv4);
                rrddim_set_by_pointer(chart_data_arr[i]->st_req_client_all_time, chart_data_arr[i]->dim_req_client_all_time_ipv6, chart_data_arr[i]->num_req_client_all_time_ipv6);
                rrdset_done(chart_data_arr[i]->st_req_client_all_time);
            }

            /* Request methods - update chart */
            if(p_file_info->parser_config->chart_config & CHART_REQ_METHODS){
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
            }

            /* Request protocol - update chart */
            if(p_file_info->parser_config->chart_config & CHART_REQ_PROTO){
                rrdset_next(chart_data_arr[i]->st_req_proto);
                rrddim_set_by_pointer(chart_data_arr[i]->st_req_proto, chart_data_arr[i]->dim_req_proto_http_1, chart_data_arr[i]->num_req_proto_http_1);
                rrddim_set_by_pointer(chart_data_arr[i]->st_req_proto, chart_data_arr[i]->dim_req_proto_http_1_1, chart_data_arr[i]->num_req_proto_http_1_1);
                rrddim_set_by_pointer(chart_data_arr[i]->st_req_proto, chart_data_arr[i]->dim_req_proto_http_2, chart_data_arr[i]->num_req_proto_http_2);
                rrddim_set_by_pointer(chart_data_arr[i]->st_req_proto, chart_data_arr[i]->dim_req_proto_other, chart_data_arr[i]->num_req_proto_other);
                rrdset_done(chart_data_arr[i]->st_req_proto);
            }

            /* Request bandwidth - update chart */
            if(p_file_info->parser_config->chart_config & CHART_BANDWIDTH){
                rrdset_next(chart_data_arr[i]->st_bandwidth);
                rrddim_set_by_pointer(chart_data_arr[i]->st_bandwidth, chart_data_arr[i]->dim_bandwidth_req_size, chart_data_arr[i]->num_bandwidth_req_size);
                rrddim_set_by_pointer(chart_data_arr[i]->st_bandwidth, chart_data_arr[i]->dim_bandwidth_resp_size, chart_data_arr[i]->num_bandwidth_resp_size);
                rrdset_done(chart_data_arr[i]->st_bandwidth);
            }

            /* Request proc time - update chart */
            if(p_file_info->parser_config->chart_config & CHART_REQ_PROC_TIME){
                rrdset_next(chart_data_arr[i]->st_req_proc_time);
                rrddim_set_by_pointer(chart_data_arr[i]->st_req_proc_time, chart_data_arr[i]->dim_req_proc_time_min, chart_data_arr[i]->num_req_proc_time_min);
                rrddim_set_by_pointer(chart_data_arr[i]->st_req_proc_time, chart_data_arr[i]->dim_req_proc_time_max, chart_data_arr[i]->num_req_proc_time_max);
                rrddim_set_by_pointer(chart_data_arr[i]->st_req_proc_time, chart_data_arr[i]->dim_req_proc_time_avg, chart_data_arr[i]->num_req_proc_time_avg);
                rrdset_done(chart_data_arr[i]->st_req_proc_time);
            }

            /* Response code family - update chart */
            if(p_file_info->parser_config->chart_config & CHART_RESP_CODE_FAMILY){
                rrdset_next(chart_data_arr[i]->st_resp_code_family);
                rrddim_set_by_pointer(chart_data_arr[i]->st_resp_code_family, chart_data_arr[i]->dim_resp_code_family_1xx, chart_data_arr[i]->num_resp_code_family_1xx);
                rrddim_set_by_pointer(chart_data_arr[i]->st_resp_code_family, chart_data_arr[i]->dim_resp_code_family_2xx, chart_data_arr[i]->num_resp_code_family_2xx);
                rrddim_set_by_pointer(chart_data_arr[i]->st_resp_code_family, chart_data_arr[i]->dim_resp_code_family_3xx, chart_data_arr[i]->num_resp_code_family_3xx);
                rrddim_set_by_pointer(chart_data_arr[i]->st_resp_code_family, chart_data_arr[i]->dim_resp_code_family_4xx, chart_data_arr[i]->num_resp_code_family_4xx);
                rrddim_set_by_pointer(chart_data_arr[i]->st_resp_code_family, chart_data_arr[i]->dim_resp_code_family_5xx, chart_data_arr[i]->num_resp_code_family_5xx);
                rrddim_set_by_pointer(chart_data_arr[i]->st_resp_code_family, chart_data_arr[i]->dim_resp_code_family_other, chart_data_arr[i]->num_resp_code_family_other);
                rrdset_done(chart_data_arr[i]->st_resp_code_family);
            }

            /* Response code - update chart */
            if(p_file_info->parser_config->chart_config & CHART_RESP_CODE){
                rrdset_next(chart_data_arr[i]->st_resp_code);
                for(int j = 0; j < 501; j++) rrddim_set_by_pointer(chart_data_arr[i]->st_resp_code, chart_data_arr[i]->dim_resp_code[j], chart_data_arr[i]->num_resp_code[j]);
                rrdset_done(chart_data_arr[i]->st_resp_code);
            }
            
            /* Response code family - update chart */
            if(p_file_info->parser_config->chart_config & CHART_RESP_CODE_TYPE){
                rrdset_next(chart_data_arr[i]->st_resp_code_type);
                rrddim_set_by_pointer(chart_data_arr[i]->st_resp_code_type, chart_data_arr[i]->dim_resp_code_type_success, chart_data_arr[i]->num_resp_code_type_success);
                rrddim_set_by_pointer(chart_data_arr[i]->st_resp_code_type, chart_data_arr[i]->dim_resp_code_type_redirect, chart_data_arr[i]->num_resp_code_type_redirect);
                rrddim_set_by_pointer(chart_data_arr[i]->st_resp_code_type, chart_data_arr[i]->dim_resp_code_type_bad, chart_data_arr[i]->num_resp_code_type_bad);
                rrddim_set_by_pointer(chart_data_arr[i]->st_resp_code_type, chart_data_arr[i]->dim_resp_code_type_error, chart_data_arr[i]->num_resp_code_type_error);
                rrddim_set_by_pointer(chart_data_arr[i]->st_resp_code_type, chart_data_arr[i]->dim_resp_code_type_other, chart_data_arr[i]->num_resp_code_type_other);
                rrdset_done(chart_data_arr[i]->st_resp_code_type);
            }

            /* SSL protocol - update chart */
            if(p_file_info->parser_config->chart_config & CHART_SSL_PROTO){
                rrdset_next(chart_data_arr[i]->st_ssl_proto);
                rrddim_set_by_pointer(chart_data_arr[i]->st_ssl_proto, chart_data_arr[i]->dim_ssl_proto_tlsv1, chart_data_arr[i]->num_ssl_proto_tlsv1);
                rrddim_set_by_pointer(chart_data_arr[i]->st_ssl_proto, chart_data_arr[i]->dim_ssl_proto_tlsv1_1, chart_data_arr[i]->num_ssl_proto_tlsv1_1);
                rrddim_set_by_pointer(chart_data_arr[i]->st_ssl_proto, chart_data_arr[i]->dim_ssl_proto_tlsv1_2, chart_data_arr[i]->num_ssl_proto_tlsv1_2);
                rrddim_set_by_pointer(chart_data_arr[i]->st_ssl_proto, chart_data_arr[i]->dim_ssl_proto_tlsv1_3, chart_data_arr[i]->num_ssl_proto_tlsv1_3);
                rrddim_set_by_pointer(chart_data_arr[i]->st_ssl_proto, chart_data_arr[i]->dim_ssl_proto_sslv2, chart_data_arr[i]->num_ssl_proto_sslv2);
                rrddim_set_by_pointer(chart_data_arr[i]->st_ssl_proto, chart_data_arr[i]->dim_ssl_proto_sslv3, chart_data_arr[i]->num_ssl_proto_sslv3);
                rrddim_set_by_pointer(chart_data_arr[i]->st_ssl_proto, chart_data_arr[i]->dim_ssl_proto_other, chart_data_arr[i]->num_ssl_proto_other);
                rrdset_done(chart_data_arr[i]->st_ssl_proto);
            }

            /* SSL cipher suite - update chart */
            if(p_file_info->parser_config->chart_config & CHART_SSL_CIPHER){
                rrdset_next(chart_data_arr[i]->st_ssl_cipher);
                for(int j = 0; j < chart_data_arr[i]->ssl_cipher_size; j++){
                    rrddim_set_by_pointer(chart_data_arr[i]->st_ssl_cipher, chart_data_arr[i]->dim_ssl_ciphers[j], chart_data_arr[i]->num_ssl_ciphers[j]);
                }
                rrdset_done(chart_data_arr[i]->st_ssl_cipher);
            }
        }
	}

    netdata_thread_cleanup_pop(1);
    return NULL;
}