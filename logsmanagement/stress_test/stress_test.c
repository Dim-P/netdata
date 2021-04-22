/** @file stress_test.c
 *  @brief Black-box stress testing of Netdata Logs management
 *
 *  @author Dimitrios Pantazis
 */

#include <assert.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>
#include <uv.h>
#include "../config_.h"

#include "stress_test.h"

#define SIMULATED_LOGS_DIR "/tmp/netdata_log_management_stress_test_data"
#define MSGS_TO_PRODUCE 5000000U /**< Messages to be produced per log source **/
#define QUERIES_DELAY 1 /**< Delay before executing queries once log producer threads have finished. Must be > LOG_FILE_READ_INTERVAL to ensure netdata-logs had chance to read in all produced logs. **/
#define DELAY_OPEN_TO_WRITE_SEC 6U /**< Give Netdata some time to startup and register a listener to the log source **/

#ifdef _WIN32
# define PIPENAME "\\\\?\\pipe\\netdata-logs-stress-test"
#else
# define PIPENAME "/tmp/netdata-logs-stress-test"
#endif // _WIN32

uv_process_t child_req;
uv_process_options_t options;
size_t max_msg_len;
static int log_msgs_arr_size;
static int log_files_no;

static const char *log_msgs_arr[] = {
    "[error] [client 127.0.0.1] File does not exist: /Users/user1/Documents/workspace/MAMP_webroot/json",
    "[error] [client ::1] File does not exist: /Users/user1/Documents/, referer: http://localhost:8888/app/",
    "[notice] caught SIGTERM, shutting down",
    "[error] [client ::1] client denied by server configuration: /Users/user1/Documents/workspace/MAMP_webroot/web_app/.DS_Store, referer: http://localhost:8888/web_app/bower_components/",
    "[error] [client ::1] client denied by server configuration: /Users/user1/Documents/workspace/MAMP_webroot/.DS_Store",
    "FORCE BAD WRITE --- [error1] [error2] [error3] [error4] [error5] [error6] [error7] [error8] [error9] [error10] [error11] [error12] [error13] [error14] [error15] --- FORCE BAD WRITE",
    "::1 - - \"GET /server-status?auto HTTP/1.1\" 200 691 \"-\" \"Go-http-client/1.1\" ",
    "PHP Warning:  mysqli::__construct(): (HY000/2002): Operation timed out in /Users/user1/Documents/workspace/MAMP_webroot/web_app/resources/real_time_data.php on line 14",
    "PHP Notice:  Undefined variable: rows in /Users/user1/Documents/workspace/MAMP_webroot/web_app/resources/real_time_data.php on line 50",
    "PHP Warning:  mysqli::__construct(): (08004/1040): Too many connections in /Users/user1/Documents/workspace/MAMP_webroot/web_app/resources/real_time_data.php on line 14",
    "PHP Fatal error:  Uncaught Exception: String could not be parsed as XML in /Users/user1/Documents/workspace/MAMP_webroot/web_app/resources/grid_demand.php:9",
    "PHP Warning:  mysqli::__construct(): (HY000/2002): Network is unreachable in /Users/user1/Documents/workspace/MAMP_webroot/web_app/resources/real_time_data.php on line 14",
    "130302  1:51:12 [Note] Plugin 'FEDERATED' is disabled.",
    "130302  1:51:12 InnoDB: The InnoDB memory heap is disabled",
    "130302  1:51:12 InnoDB: Mutexes and rw_locks use Windows interlocked functions",
    "130302  1:51:12 InnoDB: Compressed tables use zlib 1.2.3",
    "130302  1:51:12 InnoDB: Initializing buffer pool, size = 16.0M",
    "130302  1:51:12 InnoDB: Completed initialization of buffer pool",
    "130302  1:51:12 InnoDB: highest supported file format is Barracuda.",
    "InnoDB: The log sequence number in ibdata files does not match",
    "InnoDB: the log sequence number in the ib_logfiles!",
    "130302  1:51:12  InnoDB: Database was not shut down normally!",
    "InnoDB: Starting crash recovery.",
    "InnoDB: Reading tablespace information from the .ibd files...",
    "InnoDB: Restoring possible half-written data pages from the doublewrite",
    "InnoDB: buffer...",
    "130302  1:51:13  InnoDB: Waiting for the background threads to start",
    "130302  1:51:14 InnoDB: 1.1.8 started; log sequence number 1600324627",
    "130302  1:51:14 [Note] Server hostname (bind-address): '0.0.0.0'; port: 3306",
    "130302  1:51:14 [Note]   - '0.0.0.0' resolves to '0.0.0.0';",
    "130302  1:51:14 [Note] Server socket created on IP: '0.0.0.0'.",
    "130302  1:56:01 [Note] Plugin 'FEDERATED' is disabled.",
    "130302  1:56:01 InnoDB: The InnoDB memory heap is disabled",
    "130302  1:56:01 InnoDB: Mutexes and rw_locks use Windows interlocked functions",
    "130302  1:56:01 InnoDB: Compressed tables use zlib 1.2.3",
    "130302  1:56:01 InnoDB: Initializing buffer pool, size = 16.0M",
    "130302  1:56:01 InnoDB: Completed initialization of buffer pool",
    "130302  1:56:01 InnoDB: highest supported file format is Barracuda.",
    "InnoDB: The log sequence number in ibdata files does not match",
    "InnoDB: the log sequence number in the ib_logfiles!",
    "130302  1:56:01  InnoDB: Database was not shut down normally!",
    "InnoDB: Starting crash recovery.",
    "InnoDB: Reading tablespace information from the .ibd files...",
    "InnoDB: Restoring possible half-written data pages from the doublewrite",
    "InnoDB: buffer...",
    "130302  1:56:02  InnoDB: Waiting for the background threads to start",
    "130302  1:56:03 InnoDB: 1.1.8 started; log sequence number 1600324627",
    "130302  1:56:03 [Note] Server hostname (bind-address): '0.0.0.0'; port: 3306",
    "130302  1:56:03 [Note]   - '0.0.0.0' resolves to '0.0.0.0';",
    "130302  1:56:03 [Note] Server socket created on IP: '0.0.0.0'.",
    "python.d ERROR: elasticsearch[local] : _get_data() returned no data or type is not <dict>",
    "energid[bitcoin] : _get_data() returned no data or type is not <dict>",
    "gearman[localhost] : Failed to connect to '127.0.0.1', port 4730, error: [Errno 111] Connection refused",
    "hddtemp[localhost] : Failed to connect to '127.0.0.1', port 7634, error: [Errno 111] Connection refused",
    "plugin[main] : memcached[localhost] : check failed",
    "",
};

// TODO: Include query.h instead of copy-pasting
typedef struct db_query_params {
    uint64_t start_timestamp;
    uint64_t end_timestamp;
    char *filename;
    char *keyword;
    char *results;
    size_t results_size;
} logs_query_params_t;

size_t get_local_time(char *buf, size_t max_buf_size){
    time_t rawtime;
    struct tm *info;
    time( &rawtime );
    return strftime (buf, max_buf_size, "[%d/%b/%Y:%H:%M:%S %z] ",localtime( &rawtime ));
}

/**
 * @brief Get unix time in milliseconds
 * @return Unix time in milliseconds
 * @todo Include helper.h instead of redefinition of function here 
 */
static inline uint64_t get_unix_time_ms() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    uint64_t s1 = (uint64_t)(tv.tv_sec) * 1000;
    uint64_t s2 = (uint64_t)(tv.tv_usec) / 1000;
    return s1 + s2;
}

static void produce_logs(void *arg) {
    uint64_t runtime;
    uint64_t start_time = get_unix_time_ms();
    int log_no = *((int *)arg);
    int rc = 0;
    long int msgs_written = 0;
    uv_file file_handle;
    uv_buf_t uv_buf;
    char *buf = malloc(max_msg_len + 100);

    size_t buf_size;
    uv_fs_t write_req;

    uv_loop_t loop;
    uv_loop_init(&loop);

    fprintf(stderr, "Creating thread No %d\n", log_no);
    char log_filename[100];
    sprintf(log_filename, "%s/%d.log", SIMULATED_LOGS_DIR, log_no);

    /*
       fprintf(stderr, "Attempting to delete %s\n", log_filename);
       uv_fs_t unlink_req;
       rc = uv_fs_unlink(uv_default_loop(), &unlink_req, log_filename, NULL);
       if(rc)
       fprintf(stderr, "Delete %s error: %s\n", log_filename, uv_strerror(rc)); 
       else
       fprintf(stderr, "%s deleted\n", log_filename);
       uv_fs_req_cleanup(&unlink_req);
       */

    uv_fs_t open_req;
    rc = uv_fs_open(&loop, &open_req, log_filename, O_WRONLY | O_CREAT | O_TRUNC, 0777, NULL);
    if (rc < 0) {
        fprintf(stderr, "file_open() error: %s (%d) %s\n", log_filename, rc, uv_strerror(rc));
    } else {
        fprintf(stderr, "Opened file: %s\n", log_filename);
        file_handle = open_req.result;  // open_req->result of a uv_fs_t is the file descriptor in case of the uv_fs_open
    }
    uv_fs_req_cleanup(&open_req);

    // Write some initial data so that the log file does not start from completely empty state
    const char *initial_data = "First line dummy data!!\n";
    sprintf(buf, "%s", initial_data);
    uv_buf = uv_buf_init(buf, strlen(buf)); // Skip trailing null
    uv_fs_write(&loop, &write_req, file_handle, &uv_buf, 1, -1, NULL);


    sleep(DELAY_OPEN_TO_WRITE_SEC);

    while (msgs_written < MSGS_TO_PRODUCE) {
        //sprintf(buf, "T: %" PRIu64 " ", get_unix_time_ms());
        //size_t msg_timestamp_len = strlen(buf);

        size_t msg_timestamp_len = 50;
        msg_timestamp_len = get_local_time(buf, msg_timestamp_len);
        
        int msg_offset = rand() % log_msgs_arr_size;
        size_t msg_len = strlen(log_msgs_arr[msg_offset]);
        memcpy(&buf[msg_timestamp_len], log_msgs_arr[msg_offset], msg_len);
        buf_size = msg_timestamp_len + msg_len;
        buf[buf_size] = '\n';
        // buf[buf_size + 1] = '\0';
        // fprintf(stderr, "%*.*s", (int) buf_size + 1, (int) buf_size + 1, buf);
        uv_buf = uv_buf_init(buf, buf_size + 1);
        uv_fs_write(&loop, &write_req, file_handle, &uv_buf, 1, -1, NULL);
        msgs_written++;
        if(!(msgs_written % 1000000))
        fprintf(stderr, "Wrote %" PRId64 " messages to %s\n", msgs_written, log_filename);
        // usleep(1000);
    }

    runtime = get_unix_time_ms() - start_time - DELAY_OPEN_TO_WRITE_SEC * 1000;
    fprintf(stderr, "It took %" PRIu64 "ms to write %" PRId64 " msgs in %s (%" PRId64 "k msgs/s))\n. ",
            runtime, msgs_written, log_filename, msgs_written / runtime);
}

//void on_netdata_logs_exit(uv_process_t *req, int64_t exit_status, int term_signal) {
//    fprintf(stderr, "Process exited with status %" PRId64 ", signal %d\n", exit_status, term_signal);
//    uv_close((uv_handle_t *)req, NULL);
//}

static void connect_cb(uv_connect_t* req, int status){
    int rc = 0;
    if(status < 0){
        fprintf(stderr, "Failed to connect to pipe!\n");
        exit(-1);
    }
    else
        fprintf(stderr, "Connection to pipe successful!\n");

    uv_write_t write_req; 
    write_req.data = req->handle;
    
    // Serialise logs_query_params_t
    char *buf = calloc(100 * log_files_no, sizeof(char));
    sprintf(buf, "%d", log_files_no);
    for(int i = 0; i < log_files_no ; i++){
        sprintf(&buf[strlen(buf)], ",0,10000000000000000," SIMULATED_LOGS_DIR "/%d.log,%s,%zu", i, " ", (size_t) MAX_LOG_MSG_SIZE);
    }
    fprintf(stderr, "Serialised DB query params: %s\n", buf);

    // Write to pipe
    uv_buf_t uv_buf = uv_buf_init(buf, strlen(buf));
    rc = uv_write(&write_req, (uv_stream_t *) req->handle, &uv_buf, 1, NULL);
    if (rc) {
        fprintf(stderr, "uv_write() error: %s\n", uv_strerror(rc));
        uv_close((uv_handle_t *) req->handle, NULL);
        exit(-1);
    }

#if 1
    uv_shutdown_t shutdown_req;
    rc = uv_shutdown(&shutdown_req, (uv_stream_t *) req->handle, NULL);
    if (rc) {
        fprintf(stderr, "uv_shutdown() error: %s\n", uv_strerror(rc));
        uv_close((uv_handle_t *) req->handle, NULL);
        exit(-1);
    }
#endif 
    
}

int main(int argc, const char *argv[]) {
    fprintf(stderr, "Starting stress test program ...\n");

    srand(time(NULL));

    // Calculate log_msgs_arr_size and max_msg_len
    for (log_msgs_arr_size = 0; log_msgs_arr[log_msgs_arr_size][0] != '\0'; log_msgs_arr_size++) {
        size_t msg_len = strlen(log_msgs_arr[log_msgs_arr_size]);
        //fprintf(stderr, "Size of %d: %zu\n", log_msgs_arr_size, msg_len);
        if (msg_len > max_msg_len)
            max_msg_len = msg_len;
    }
    fprintf(stderr,
            "Number of different strings to be used as random log messages: %d\n"
            "Max log message size: %zuB\n",
            log_msgs_arr_size, max_msg_len);

    // Start threads that produce log messages
    char *ptr;
    log_files_no = (int) strtol(argv[1], &ptr, 10);
    fprintf(stdout, "Number of log files to simulate: %d", log_files_no);
    //scanf("%d", &log_files_no);
    uv_thread_t *log_producer_threads = malloc(log_files_no * sizeof(uv_thread_t));
    int *log_producer_thread_no = malloc(log_files_no * sizeof(int));
    for (int i = 0; i < log_files_no; i++) {
        fprintf(stderr, "Starting up log producer for %d.log...\n", i);
        log_producer_thread_no[i] = i;
        assert(!uv_thread_create(&log_producer_threads[i], produce_logs, &log_producer_thread_no[i]));
    }

#if 0
    /* Arguments to pass on to netdata-logs. First must be process name and last one 
     * must be NULL: http://docs.libuv.org/en/v1.x/guide/processes.html#spawning-child-processes */
    char **args = malloc((log_files_no + 2) * sizeof(char *));
    args[0] = malloc(strlen("netdata-logs") * sizeof(char));
    args[0] = "netdata-logs";
    args[log_files_no + 1] = NULL;
    for (int i = 1; i <= log_files_no; i++) {
        int length = (strlen("test_data/") + snprintf(NULL, 0, "%d", i - 1)  // size of log_files_no as string
                      + strlen(".log") + 1) *
                     sizeof(char);
        args[i] = malloc(length);
        snprintf(args[i], length, "test_data/%d.log", i - 1);
    }

    // Set netdata-logs I/O - ignore stdin, stdout, redirect stderr to same as parent
    options.stdio_count = 3;
    uv_stdio_container_t child_stdio[3];
    child_stdio[0].flags = UV_IGNORE;
    child_stdio[1].flags = UV_IGNORE;
    child_stdio[2].flags = UV_INHERIT_FD;
    child_stdio[2].data.fd = 2;
    options.stdio = child_stdio;

    options.exit_cb = on_netdata_logs_exit;
    options.cwd = "../";
    options.file = "./netdata-logs";
    options.args = args;

    sleep(LOG_PRODUCER_THREADS_SETUP_DELAY); // Give some time to the log producer threads to setup

#if 1
    int r;
    if ((r = uv_spawn(uv_default_loop(), &child_req, &options))) {
        fprintf(stderr, "Error executing netdata-logs: %s\n", uv_strerror(r));
        return 1;
    } else {
        fprintf(stderr, "Launched netdata-logs with ID %d\n", child_req.pid);
    }
#endif
#endif

    for (int j = 0; j < log_files_no; j++) {
        uv_thread_join(&log_producer_threads[j]);
    }

    sleep(QUERIES_DELAY); // Give netdata-logs more than LOG_FILE_READ_INTERVAL to ensure the entire log file has been read.

    uv_pipe_t query_data_pipe;
    uv_pipe_init(uv_default_loop(), &query_data_pipe, 1);
    uv_connect_t connect_req;
    uv_pipe_connect(&connect_req, &query_data_pipe, PIPENAME, connect_cb);

    uv_run(uv_default_loop(), UV_RUN_DEFAULT);

    uv_close((uv_handle_t *) &query_data_pipe, NULL);
}
