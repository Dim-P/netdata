/** @file query_test.c
 *  @brief This is the file containing tests for the query API.
 *
 *  @author Dimitris Pantazis
 */

#include "query.h"
#include <inttypes.h>
#include <stdlib.h>
#include <uv.h>
#include "config_.h"
#include "helper.h"
#include "query_test.h"

static uv_loop_t query_thread_uv_loop;
static uv_pipe_t query_data_pipe;

static void pipe_read_cb(uv_stream_t *client, ssize_t nread, const uv_buf_t *buf) {
    if (nread < 0) {
        uv_close((uv_handle_t *)client, NULL);
        return;
    }
    fprintf_log(LOGS_MANAG_INFO, stderr, "Read through pipe: %.*s\n", (int) nread, buf->base);

    // Deserialise streamed string
    char *pEnd;
    int log_files_no = strtol(strtok(buf->base, ","), &pEnd, 10);
    DB_query_params_t *query_params = malloc(log_files_no * sizeof(DB_query_params_t));
    uv_thread_t *test_execute_query_thread_id = malloc(log_files_no * sizeof(uv_thread_t));
    for (int i = 0; i < log_files_no; i++) {
        query_params[i].start_timestamp = strtoll(strtok(NULL, ","), &pEnd, 10);
        query_params[i].end_timestamp = strtoll(strtok(NULL, ","), &pEnd, 10);
        query_params[i].filename = malloc(100 * sizeof(char));
        query_params[i].filename = strtok(NULL, ",");
        query_params[i].keyword = strtok(NULL, ",");
        query_params[i].results_size = (size_t)strtoll(strtok(NULL, ","), &pEnd, 10);


        int rc = uv_thread_create(&test_execute_query_thread_id[i], test_execute_query_thread, &query_params[i]);
        if (unlikely(rc))
            fprintf_log(LOGS_MANAG_ERROR, stderr, "Creation of thread failed: %s\n", uv_strerror(rc));
        m_assert(!rc, "Creation of thread failed");
    }

}

static void alloc_cb(uv_handle_t *handle, size_t suggested_size, uv_buf_t *buf) {
    buf->base = malloc(suggested_size);
    buf->len = suggested_size;
}

static void connection_cb(uv_stream_t *server, int status) {
    int rc = 0;

    if (status == -1) {
        fprintf_log(LOGS_MANAG_ERROR, stderr, "uv_listen connection_cb error\n");
        m_assert(0, "uv_listen connection_cb error!");
    }

    fprintf_log(LOGS_MANAG_INFO, stderr, "Received connection on " LOGS_MANAGEMENT_STRESS_TEST_PIPENAME "\n");

    uv_pipe_t *client = (uv_pipe_t *)malloc(sizeof(uv_pipe_t));
    uv_pipe_init(&query_thread_uv_loop, client, 0);
    if (uv_accept(server, (uv_stream_t *)client) == 0) {
        if ((rc = uv_read_start((uv_stream_t *)client, alloc_cb, pipe_read_cb))) {
            fprintf_log(LOGS_MANAG_INFO, stderr, "uv_read_start(): %s\n", uv_strerror(rc));
            uv_close((uv_handle_t *)&client, NULL);
            m_assert(0, "uv_read_start() error");
        }
    } else {
        uv_close((uv_handle_t *)client, NULL);
    }
}

void remove_pipe(int sig) {
    uv_fs_t req;
    uv_fs_unlink(&query_thread_uv_loop, &req, LOGS_MANAGEMENT_STRESS_TEST_PIPENAME, NULL);
    uv_fs_req_cleanup(&req);
    // exit(0);
}

void test_execute_query_thread(void *args) {
    DB_query_params_t query_params = *((DB_query_params_t *)args);
    int rc = 0;
    uv_file file_handle;
    uv_buf_t uv_buf;
    int64_t file_offset = 0;
    size_t results_size = query_params.results_size;

    uv_loop_t thread_loop;
    uv_loop_init(&thread_loop);

    // Open log source to use for validation
    uv_fs_t open_req;
    rc = uv_fs_open(&thread_loop, &open_req, query_params.filename, O_RDONLY, 0, NULL);
    if (unlikely(rc < 0)) {
        fprintf_log(LOGS_MANAG_ERROR, stderr, "file_open() error: %s (%d) %s\n", query_params.filename, rc, uv_strerror(rc));
        m_assert(rc >= 0, "uv_fs_open() failed");
    } else {
        fprintf_log(LOGS_MANAG_INFO, stderr, "Opened file: %s\n", query_params.filename);
        file_handle = open_req.result;  // open_req->result of a uv_fs_t is the file descriptor in case of the uv_fs_open
    }
    uv_fs_req_cleanup(&open_req);

    // Skip first line that we know has "Dummy data".
    char *buf = malloc(sizeof(char));
    uv_buf = uv_buf_init(buf, sizeof(char));
    uv_fs_t read_req;
    do {
        rc = uv_fs_read(&thread_loop, &read_req, file_handle, &uv_buf, 1, file_offset, NULL);
        if (rc < 0)
            fprintf_log(LOGS_MANAG_ERROR, stderr, "uv_fs_read() error for %s\n", query_params.filename);
        m_assert(rc >= 0, "uv_fs_read() failed");
        file_offset++;
        // fprintf_log(stderr, "%c", buf[0]);
    } while (buf[0] != '\n');
    uv_fs_req_cleanup(&read_req);

    // Run queries and compare results with log file data
    const uint64_t start_time = get_unix_time_ms();
    uint64_t query_start_time, query_total_time = 0;
    while (1) {
        query_start_time = get_unix_time_ms();
        query_params.results = execute_query(
            query_params.start_timestamp, query_params.end_timestamp,
            query_params.filename, NULL, &query_params.results_size);
        query_total_time += (get_unix_time_ms() - query_start_time);
        if (!query_params.results)
            break;
        buf = realloc(buf, query_params.results_size);
        uv_buf = uv_buf_init(buf, query_params.results_size);
        rc = uv_fs_read(&thread_loop, &read_req, file_handle, &uv_buf, 1, file_offset, NULL);
        if (rc < 0)
            fprintf_log(LOGS_MANAG_ERROR, stderr, "uv_fs_read() error for %s\n", query_params.filename);
        m_assert(rc >= 0, "uv_fs_read() failed");

        // fprintf_log(LOGS_MANAG_DEBUG, stderr, "\n%.*s\n", 1000, query_params.results);
        // fprintf_log(LOGS_MANAG_DEBUG, stderr, "\n%.*s\n\n", 1000, buf);
        rc = memcmp(buf, query_params.results, query_params.results_size);
        if (rc)
            fprintf_log(LOGS_MANAG_INFO, stderr, "Mismatch between DB and log file data in %s\n", query_params.filename);
        m_assert(!rc, "Mismatch between DB and log file data!");

        file_offset += query_params.results_size;
        fprintf_log(LOGS_MANAG_INFO, stderr, "Query file offset %" PRId64 " for %s\n", file_offset, query_params.filename);
        freez(query_params.results);
        query_params.results_size = results_size;  // Set desired max size of results again
        uv_fs_req_cleanup(&read_req);
    }

#if 1
    // Log filesize should be the same as byte of data read back from the database
    uv_fs_t stat_req;
    rc = uv_fs_stat(&thread_loop, &stat_req, query_params.filename, NULL);
    if (rc) {
        fprintf_log(LOGS_MANAG_ERROR, stderr, "uv_fs_stat() error for %s: (%d) %s\n", query_params.filename, rc, uv_strerror(rc));
        m_assert(!rc, "uv_fs_stat() failed");
    } else {
        // Request succeeded; get filesize
        uv_stat_t *statbuf = uv_fs_get_statbuf(&stat_req);
        // fprintf_log(LOGS_MANAG_INFO, stderr, "Size of %s: %lldKB\n", query_params.filename, (long long)statbuf->st_size / 1000);
        if (statbuf->st_size != file_offset)
            fprintf_log(LOGS_MANAG_ERROR, stderr, "Mismatch between log filesize (%lld) and data size returned from query (%" PRId64 ") for: %s\n",
                        (long long)statbuf->st_size, file_offset, query_params.filename);
        m_assert(statbuf->st_size == file_offset, "Mismatch between log filesize and data size in DB!");
        fprintf_log(LOGS_MANAG_INFO, stderr, "Log filesize and data size from query match for %s\n", query_params.filename);
    }
    uv_fs_req_cleanup(&stat_req);
#endif

    const uint64_t end_time = get_unix_time_ms();
    fprintf_log(LOGS_MANAG_INFO, stderr,
            "==============================\nStress test queries for '%s' completed with success!\n"
            "Total duration: %" PRIu64 "ms to retrieve and compare %" PRId64 "KB.\nQuery execution total duration: %" PRIu64 "ms\n==============================\n",
            query_params.filename, end_time - start_time, file_offset / 1000, query_total_time);

    uv_run(&thread_loop, UV_RUN_DEFAULT);
}

void run_stress_test_queries_thread(void *args) {
    int rc = 0;
    rc = uv_loop_init(&query_thread_uv_loop);
    if (unlikely(rc)) fatal("Failed to initialise query_thread_uv_loop\n");

    if ((rc = uv_pipe_init(&query_thread_uv_loop, &query_data_pipe, 0))) {
        fprintf_log(LOGS_MANAG_INFO, stderr, "uv_pipe_init(): %s\n", uv_strerror(rc));
        m_assert(0, "uv_pipe_init() failed");
    }
    signal(SIGINT, remove_pipe);
    if ((rc = uv_pipe_bind(&query_data_pipe, LOGS_MANAGEMENT_STRESS_TEST_PIPENAME))) {
        fprintf_log(LOGS_MANAG_ERROR, stderr, "uv_pipe_bind() error %s. Trying again.\n", uv_err_name(rc));
        // Try removing pipe and binding again
        remove_pipe(0);  // Delete pipe if it exists
        // uv_close((uv_handle_t *)&query_data_pipe, NULL);
        rc = uv_pipe_bind(&query_data_pipe, LOGS_MANAGEMENT_STRESS_TEST_PIPENAME);
        fprintf_log(LOGS_MANAG_ERROR, stderr, "uv_pipe_bind() error %s\n", uv_err_name(rc));
        m_assert(!rc, "uv_pipe_bind() error!");
    }
    if ((rc = uv_listen((uv_stream_t *)&query_data_pipe, 1, connection_cb))) {
        fprintf_log(LOGS_MANAG_ERROR, stderr, "uv_pipe_listen() error %s\n", uv_err_name(rc));
        m_assert(!rc, "uv_pipe_listen() error!");
    }
    uv_run(&query_thread_uv_loop, UV_RUN_DEFAULT);
    uv_close((uv_handle_t *)&query_data_pipe, NULL);
}
