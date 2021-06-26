/** @file query.c
 *  @brief This is the file containing the implementation of the querying API.
 *
 *  @author Dimitris Pantazis
 */

#include "query.h"
#include <uv.h>
#include "circular_buffer.h"
#include "db_api.h"
#include "file_info.h"
#include "helper.h"

int execute_query(logs_query_params_t *p_query_params) {
    struct File_info *p_file_info = NULL;
    size_t max_query_page_size = p_query_params->results_buff->size;
    assert(max_query_page_size <= MAX_LOG_MSG_SIZE);  // Requested max size of results must be smaller than maximum size of each DB row entry.

    fprintf(stderr, "Query params:%lu\n%lu\n%s\n%s\n%s\n", p_query_params->start_timestamp, p_query_params->end_timestamp, 
         p_query_params->chart_name, p_query_params->filename, p_query_params->keyword);

    /* Find p_file_info for this query according to chart_name or filename if the former is not valid. */
    if(p_query_params->chart_name && p_query_params->chart_name[0] != '\0'){
        for (int file_info_offset = 0; file_info_offset < p_file_infos_arr->count; file_info_offset++) {
            if (!strcmp(p_file_infos_arr->data[file_info_offset]->chart_name, p_query_params->chart_name)) {
                p_file_info = p_file_infos_arr->data[file_info_offset];
                break;
            }
        }
    } 
    else if(p_query_params->filename && p_query_params->filename[0] != '\0'){
        for (int file_info_offset = 0; file_info_offset < p_file_infos_arr->count; file_info_offset++) {
            if (!strcmp(p_file_infos_arr->data[file_info_offset]->filename, p_query_params->filename)) {
                p_file_info = p_file_infos_arr->data[file_info_offset];
                break;
            }
        }
    }
    else return -1;

    if(!p_file_info) return -1;

    /* Secure DB lock to ensure no data will be transferred from the buffers to the DB 
    * during the query execution and also no other execute_query will try to access the DB
    * at the same time. The operations happen atomically and the DB searches in series. */
    db_set_lock(p_file_info->db_mut);

    const uint64_t start_time = get_unix_time_ms();

    // Search DB first
    db_search(p_query_params, p_file_info, max_query_page_size);

    const uint64_t db_search_time = get_unix_time_ms();

    /* m_assert((p_query_params->results_buff->buffer && p_query_params->results_buff->len) 
             || !(p_query_params->results_buff->buffer || p_query_params->results_buff->len),
              "Either both buff and len true or both false"); */

    /* Search circular buffer ONLY IF the results len is less than the originally requested max size!
     * p_query_params->end_timestamp will be the originally requested here, as it won't have been
     * updated in db_search() due to (p_query_params->results_buff->len >= max_query_page_size) condition */
    if (p_query_params->results_buff->len < max_query_page_size) {
        fprintf_log(LOGS_MANAG_INFO, stderr, "\nSearching circular buffer!\n");
        if(!p_query_params->keyword) circ_buff_search(p_file_info->msg_buff, p_query_params, max_query_page_size); // TODO: Implement keyword search for circ buffer.
    }

    db_release_lock(p_file_info->db_mut);

    // p_query_params->results_buff->len++;
    // p_query_params->results_buff->buffer[p_query_params->results_buff->len] = '\0';

    const uint64_t end_time = get_unix_time_ms();
    fprintf_log(LOGS_MANAG_INFO, stderr,
                "It took %" PRId64
                "ms to execute query "
                "(%" PRId64 "ms DB search, %" PRId64
                "ms circ buffer search), "
                "retrieving %zuKB.\n",
                (int64_t)end_time - start_time,
                (int64_t)db_search_time - start_time, (int64_t)end_time - db_search_time,
                p_query_params->results_buff->len / 1000);

    if(!p_query_params->results_buff->len) return -2;

    return 0;
}
