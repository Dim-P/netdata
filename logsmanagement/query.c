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

char *execute_query(uint64_t start_timestamp, uint64_t end_timestamp, const char *filename, const char *keyword, size_t *p_size) {
    static thread_local uint64_t end_timestamp_prev, last_timestamp_prev = 0;
    static thread_local char *filename_prev, *keyword_prev = NULL;
    DB_query_params_t query_params = {0};
    struct File_info *p_file_info;

    assert(*p_size <= MAX_LOG_MSG_SIZE);  // Requested max size of results must be smaller than maximum size of each DB row entry.

    if (!last_timestamp_prev) {
        /* Case of new query (new thread) */
        end_timestamp_prev = end_timestamp;
        filename_prev = (char *)filename;
        keyword_prev = (char *)keyword;
        query_params.start_timestamp = start_timestamp;
    } else {
        /* Case of existing query continuing */
        query_params.start_timestamp = last_timestamp_prev;
    }
    query_params.end_timestamp = end_timestamp_prev;
    query_params.filename = filename_prev;
    query_params.keyword = keyword_prev;
    query_params.results_size = *p_size;

    /* Find p_file_info for this query */
    for (int file_info_offset = 0; file_info_offset < p_file_infos_arr->count; file_info_offset++) {
        if (!strcmp(p_file_infos_arr->data[file_info_offset]->filename, query_params.filename)) {
            p_file_info = p_file_infos_arr->data[file_info_offset];
            break;
        }
    }

    /* Secure DB lock to ensure no data will be transferred from the buffers to the DB 
    * during the query execution and also no other execute_query will try to access the DB
    * at the same time. The operations happen atomically and the DB searches in series. */
    db_set_lock(p_file_info->db_mut);

    const uint64_t start_time = get_unix_time_ms();

    // Search DB first
    db_search(&query_params, p_file_info);

    const uint64_t db_search_time = get_unix_time_ms();

    m_assert((query_params.results && query_params.results_size) || !(query_params.results || query_params.results_size),
             "Either both results and results_size true or both false");

    // Search circular buffer ONLY IF DB returns no more results!
    if (!query_params.results_size) {
        fprintf_log(LOGS_MANAG_DEBUG, stderr, "\nSearching circular buffer!\n");
        circ_buff_search(p_file_info->msg_buff, &query_params);
    }

    db_release_lock(p_file_info->db_mut);

    last_timestamp_prev = query_params.start_timestamp;

    *p_size = query_params.results_size;

    const uint64_t end_time = get_unix_time_ms();
    fprintf_log(LOGS_MANAG_INFO, stderr,
                "It took %" PRId64
                "ms to execute query "
                "(%" PRId64 "ms DB search, %" PRId64
                "ms circ buffer search), "
                "retrieving %zuKB.\n",
                (int64_t)end_time - start_time,
                (int64_t)db_search_time - start_time, (int64_t)end_time - db_search_time,
                query_params.results_size / 1000);

    return query_params.results;
}
