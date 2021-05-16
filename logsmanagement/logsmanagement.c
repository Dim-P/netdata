/** @file main.c
 *  @brief This is the main file of the netdata-logs project
 *
 *  The aim of the project is to add the capability to collect
 *  logs in the netdata agent and store them in a database for
 *  querying. main.c uses libuv and its callbacks mechanism to
 *  setup a listener for each log source. 
 *
 *  @author Dimitris Pantazis
 */

#include "../daemon/common.h"
#include "../libnetdata/libnetdata.h"
#include <assert.h>
#include <inttypes.h>
#include <lz4.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <uv.h>
#include "circular_buffer.h"
#include "config_.h"
#include "db_api.h"
#include "file_info.h"
#include "helper.h"
#include "query.h"
#if LOGS_MANAGEMENT_STRESS_TEST
#include "query_test.h"
#endif  // LOGS_MANAGEMENT_STRESS_TEST
#include "parser.h"

static struct config log_management_config = {
    .first_section = NULL,
    .last_section = NULL,
    .mutex = NETDATA_MUTEX_INITIALIZER,
    .index = {
            .avl_tree = {
                    .root = NULL,
                    .compar = appconfig_section_compare
            },
            .rwlock = AVL_LOCK_INITIALIZER
    }
};

struct File_infos_arr *p_file_infos_arr = NULL;
static uv_thread_t fs_events_reenable_thread_id;
static uv_loop_t *main_loop; 

// Forward declarations
static void file_changed_cb(uv_fs_event_t *handle, const char *file_basename, int events, int status);
static void handle_UV_ENOENT_err(struct File_info *p_file_info);

static void fs_events_reenable_thread(void *arg) {
    uint64_t fs_events_reenable_list_local = 0;
    int offset, rc;
    while (1) {
        uv_mutex_lock(&p_file_infos_arr->fs_events_reenable_lock);
        while (p_file_infos_arr->fs_events_reenable_list == 0) {
            uv_cond_wait(&p_file_infos_arr->fs_events_reenable_cond,
                         &p_file_infos_arr->fs_events_reenable_lock);
        }
        fs_events_reenable_list_local = p_file_infos_arr->fs_events_reenable_list;
        p_file_infos_arr->fs_events_reenable_list = 0;
        uv_mutex_unlock(&p_file_infos_arr->fs_events_reenable_lock);

        sleep_ms(FS_EVENTS_REENABLE_INTERVAL);  // Give it some time for the file that wasn't found to be created

        fprintf_log(LOGS_MANAG_DEBUG, stderr, "fs_events_reenable_list pending: %lld\n", fs_events_reenable_list_local);
        offset = 0;
        for (offset = 0; offset < p_file_infos_arr->count; offset++) {
            if (BIT_CHECK(fs_events_reenable_list_local, offset)) {
                fprintf_log(LOGS_MANAG_DEBUG, stderr, "Attempting to reenable fs_events for %s\n",
                            p_file_infos_arr->data[offset]->filename);
                struct File_info *p_file_info = p_file_infos_arr->data[offset];
                fprintf_log(LOGS_MANAG_DEBUG, stderr, "Scheduling uv_fs_event for %s\n", p_file_info->filename);
                fprintf_log(LOGS_MANAG_DEBUG, stderr, "Current filesize in fs_events_reenable_thread: %lld\n",
                            p_file_info->filesize);
                rc = uv_fs_event_start(p_file_info->fs_event_req, file_changed_cb, p_file_info->filename, 0);
                if (rc) {
                    fprintf_log(LOGS_MANAG_ERROR, stderr, "uv_fs_event_start() for %s failed (%d): %s\n",
                                p_file_info->filename, rc, uv_strerror(rc));
                    if (rc == UV_ENOENT) {
                        handle_UV_ENOENT_err(p_file_info);
                    } else
                        m_assert(!rc, "uv_fs_event_start() failed");
                }
            }
        }
    }
}

/**
 * @brief This function will handle libuv's "no such file or directory" error.
 *
 * @details When a file cannot be found, the function will stop the FS events 
 * handling registered on that file and inform the fs_events_reenable_thread 
 * that there is work to be done for that file to re-enable the event listening.
 *
 * @todo Once the dynamic configuration of files to be monitored has been implemented,
 * this function should only try to restart the event listener only if the file remains
 * in the configuration.
 * @todo Current limit of 64 files max, due to size of fs_events_reenable_list.
 * */
static void handle_UV_ENOENT_err(struct File_info *p_file_info) {
    // Stop fs events
    int rc = uv_fs_event_stop(p_file_info->fs_event_req);
    if (rc)
        fprintf_log(LOGS_MANAG_ERROR, stderr, "uv_fs_event_stop() for %s failed:%s \n", p_file_info->filename, uv_strerror(rc));
    m_assert(!rc, "uv_fs_event_stop() failed");

    p_file_info->force_file_changed_cb = 1;

    // Get offset of p_file_info in p_file_infos_arr that caused the UV_ENOENT error.
    int offset = 0;
    for (offset = 0; offset < p_file_infos_arr->count; offset++) {
        if (p_file_infos_arr->data[offset] == p_file_info) {
            fprintf_log(LOGS_MANAG_DEBUG, stderr, "handle_UV_ENOENT_err called for: %s\n", p_file_infos_arr->data[offset]->filename);
            break;
        }
    }

    p_file_info->filesize = 0;

    // Send signal to re-enable fs events
    uv_mutex_lock(&p_file_infos_arr->fs_events_reenable_lock);
    BIT_SET(p_file_infos_arr->fs_events_reenable_list, offset);
    uv_cond_signal(&p_file_infos_arr->fs_events_reenable_cond);
    uv_mutex_unlock(&p_file_infos_arr->fs_events_reenable_lock);
}

/**
 * @brief Closes a log file
 * @details Synchronously close a file using the libuv API. 
 * @param p_file_info #File_info struct containing the necessary data to close the file.
 */
static int file_close(struct File_info *p_file_info) {
    int rc = 0;
    uv_fs_t close_req;
    rc = uv_fs_close(main_loop, &close_req, p_file_info->file_handle, NULL);
    if (unlikely(rc)) {
        fprintf_log(LOGS_MANAG_ERROR, stderr, "error closing %s: %s\n", p_file_info->filename, uv_strerror(rc));
        m_assert(!rc, "uv_fs_close() failed");
    } else {
        fprintf_log(LOGS_MANAG_DEBUG, stderr, "Closed file: %s\n", p_file_info->filename);
    }
    uv_fs_req_cleanup(&close_req);
    return rc;
}

/**
 * @brief Opens a log file
 * @details Synchronously open a file (as read-only) using the libuv API. 
 * @param p_file_info #File_info struct containing the necessary data to open the file.
 * @todo Investigate if use of memory-mapping will speed up subsequent file read operations.
 */
static int file_open(struct File_info *p_file_info) {
    int rc = 0;
    // TODO: Need more elegant solution about file opening and monitoring - what if file becomes available later than startup?
    uv_fs_t open_req;
    rc = uv_fs_open(main_loop, &open_req, p_file_info->filename, O_RDONLY, 0, NULL);
    if (unlikely(rc < 0)) {
        fprintf_log(LOGS_MANAG_ERROR, stderr, "file_open() error: %s (%d) %s\n", p_file_info->filename, rc, uv_strerror(rc));
        // m_assert(rc >= 0, "uv_fs_open() failed");
    } else {
        fprintf_log(LOGS_MANAG_DEBUG, stderr, "Opened file: %s\n", p_file_info->filename);
        p_file_info->file_handle = open_req.result;  // open_req->result of a uv_fs_t is the file descriptor in case of the uv_fs_open
    }
    uv_fs_req_cleanup(&open_req);
    return rc;
}

/**
 * @brief Timer callback function to re-enable file changed events. 
 * @details The function is responsible for re-enabling the FS event handle callback. Because there may be
 * messages that were missed when the file events were not being processed (i.e. for the duration
 * of #LOG_FILE_READ_INTERVAL), a "forced" call of #file_changed_cb() will take place (if and only if 
 * the #force_file_changed_cb variable is set). The value of the #force_file_changed_cb is set or cleared 
 * in #enable_file_changed_events.
 * @param handle Timer handle.
 */
static void enable_file_changed_events_timer_cb(uv_timer_t *handle) {
    int rc = 0;
    struct File_info *p_file_info = handle->data;

    fprintf_log(LOGS_MANAG_DEBUG, stderr, "Scheduling uv_fs_event for %s\n", p_file_info->filename);
    rc = uv_fs_event_start(p_file_info->fs_event_req, file_changed_cb, p_file_info->filename, 0);
    if (rc) {
        fprintf_log(LOGS_MANAG_ERROR, stderr, "uv_fs_event_start() for %s failed (%d): %s\n",
                    p_file_info->filename, rc, uv_strerror(rc));
        if (rc == UV_ENOENT) {
            handle_UV_ENOENT_err(p_file_info);
        } else
            m_assert(!rc, "uv_fs_event_start() failed");
    }

    if (p_file_info->force_file_changed_cb) {
        fprintf_log(LOGS_MANAG_DEBUG, stderr, "Forcing uv_fs_event for %s\n", p_file_info->filename);
        file_changed_cb((uv_fs_event_t *)handle, p_file_info->file_basename, 0, 0);
    }
}

/**
 * @brief Starts a timer that will re-enable file_changed events when it expires
 * @details The purpose of this function is to re-enable file events but limit them to a maximum
 * of 1 per #LOG_FILE_READ_INTERVAL, to reduce CPU usage. 
 * @param p_file_info Struct containing the timer handle associated with the respective file info struct.
 * @param force_file_changed_cb Boolean variable. If set, a call to file_changed_cb() will be "force" as
 * soon as the file event monitoring has been re-enabled. 
 * @return 0 on success
 */
static int enable_file_changed_events(struct File_info *p_file_info, uint8_t force_file_changed_cb) {
    int rc = 0;
    p_file_info->enable_file_changed_events_timer->data = p_file_info;
    p_file_info->force_file_changed_cb = force_file_changed_cb;
    // TODO: Change the timer implementation to start once, there is no reason to start-stop.
    rc = uv_timer_start(p_file_info->enable_file_changed_events_timer,
                        (uv_timer_cb)enable_file_changed_events_timer_cb, LOG_FILE_READ_INTERVAL, 0);
    if (unlikely(rc)) {
        fprintf_log(LOGS_MANAG_ERROR, stderr, "uv_timer_start() error: (%d) %s\n", rc, uv_strerror(rc));
        m_assert(!rc, "uv_timer_start() error");
    }
    return rc;
}

/**
 * @brief Read text from a log file
 * @details Callback called in #check_if_filesize_changed_cb() to read text
 * from a log file into a Message_t struct.
 */
static void read_file_cb(uv_fs_t *req) {
    struct File_info *p_file_info = req->data;

    if (unlikely(req->result < 0)) {
        fprintf_log(LOGS_MANAG_ERROR, stderr, "Read error: %s\n", uv_strerror(req->result));
        m_assert(0, "Read error");
    } else if (unlikely(req->result == 0)) {
        /* Shouldn't reach here if there are always bytes to read. 
		 * read_file_cb() should only be called if the filesize is changed. */
        fprintf_log(LOGS_MANAG_ERROR, stderr, "Read error: %s\n", uv_strerror(req->result));
        m_assert(0, "Should never reach EOF");
    } else if (likely(req->result > 0)) {
        while (p_file_info->buff[p_file_info->buff_size - 1] != '\n') {  // Check if a half-line was read
            p_file_info->buff_size--;
            if (!p_file_info->buff_size)
                goto free_access_lock;
        }

        /* Null-terminate p_file_info->buff using the extra byte (in case needed)
         * that was reallocz'd in check_if_file_changed_cb(); */
        p_file_info->buff[p_file_info->buff_size++] = '\0';

        circ_buff_write(p_file_info);
        fprintf_log(LOGS_MANAG_INFO, stderr, "Circ buff size for %s: %d\n" LOG_SEPARATOR,
                    p_file_info->file_basename, circ_buff_get_size(p_file_info->msg_buff));

        // fprintf(stderr, "New filesize %" PRIu64 " for %s\n", p_file_info->filesize, p_file_info->filename);
    }

free_access_lock:
    p_file_info->access_lock = 0;
    fprintf_log(LOGS_MANAG_DEBUG, stderr, "Access_lock released for %s\n", p_file_info->file_basename);
    (void)file_close(p_file_info);
    (void)enable_file_changed_events(p_file_info, 1);
    uv_fs_req_cleanup(req);
    freez(req);
}

static int check_file_rotation(struct File_info *p_file_info, uint64_t new_filesize) {
    uint64_t end_time;
    uint64_t start_time = get_unix_time_ms();
    int rc = 0;
    int rotated = -1;
    if (new_filesize < p_file_info->signature_size) {
        // Filesize smaller than signature_size so log must have rotated
        // All we need to do is updated the signature - no need to memcmp with old signature
        m_assert(rotated == -1, "Rotated cannot be other than -1 at this point!");
        rotated = 1;

        p_file_info->signature_size = (size_t)new_filesize;
        // TODO: Realloc down p_file_info->signature here? Not sure if needed as that will grow at some point soon!
        p_file_info->uvBuf = uv_buf_init(p_file_info->signature, p_file_info->signature_size);
        uv_fs_t read_req;
        rc = uv_fs_read(main_loop, &read_req, p_file_info->file_handle, &p_file_info->uvBuf, 1, 0, NULL);
        if (rc < 0)
            fprintf_log(LOGS_MANAG_ERROR, stderr, "uv_fs_read() error for %s\n", p_file_info->filename);
        m_assert(rc >= 0, "uv_fs_read() failed");
        uv_fs_req_cleanup(&read_req);

        end_time = get_unix_time_ms();
        fprintf_log(LOGS_MANAG_INFO, stderr, "(1) It took %" PRIu64 "ms to check file rotation.\n", end_time - start_time);
        return rotated;
    }
    if ((int64_t)new_filesize < (int64_t)p_file_info->filesize) {
        // Filesize reduced (but larger than signature) so log must have rotated
        // No need to memcmp in this case, just update the signature
        m_assert(rotated == -1, "Rotated cannot be other than -1 at this point!");
        rotated = 1;

        p_file_info->signature_size = new_filesize > MAX_FILE_SIGNATURE_SIZE ? MAX_FILE_SIGNATURE_SIZE : (size_t)new_filesize;
        // TODO: Refactor next line with temp pointer, currently in risk of memory leak in case reallocz fails!
        p_file_info->signature = reallocz(p_file_info->signature, p_file_info->signature_size);
        p_file_info->uvBuf = uv_buf_init(p_file_info->signature, p_file_info->signature_size);
        uv_fs_t read_req;
        rc = uv_fs_read(main_loop, &read_req, p_file_info->file_handle, &p_file_info->uvBuf, 1, 0, NULL);
        if (rc < 0)
            fprintf_log(LOGS_MANAG_ERROR, stderr, "uv_fs_read() error for %s\n", p_file_info->filename);
        m_assert(rc >= 0, "uv_fs_read() failed");
        uv_fs_req_cleanup(&read_req);

        end_time = get_unix_time_ms();
        fprintf_log(LOGS_MANAG_INFO, stderr, "(2) It took %" PRIu64 "ms to check file rotation.\n", end_time - start_time);
        return rotated;
    }
    if (new_filesize >= p_file_info->signature_size && new_filesize >= p_file_info->filesize) {
        // Filesize larger than signature_size and also larger than old p_file_info->filesize
        // Not sure if log has rotated - need to memcmp signatures
        m_assert(rotated == -1, "Rotated cannot be other than -1 at this point!");

        size_t comp_buff_size = new_filesize > MAX_FILE_SIGNATURE_SIZE ? MAX_FILE_SIGNATURE_SIZE : (size_t)new_filesize;
        char *comp_buff = mallocz(comp_buff_size);
        p_file_info->uvBuf = uv_buf_init(comp_buff, comp_buff_size);
        uv_fs_t read_req;
        rc = uv_fs_read(main_loop, &read_req, p_file_info->file_handle, &p_file_info->uvBuf, 1, 0, NULL);
        if (rc < 0)
            fprintf_log(LOGS_MANAG_ERROR, stderr, "uv_fs_read() error for %s\n", p_file_info->filename);
        m_assert(rc >= 0, "uv_fs_read() failed");
        uv_fs_req_cleanup(&read_req);

        // TODO: Use fixed-size signature buffer to reduce number of reallocs
        p_file_info->signature = reallocz(p_file_info->signature, comp_buff_size);
        rotated = memcmp(p_file_info->signature, comp_buff, p_file_info->signature_size) ? 1 : 0;
        fprintf_log(LOGS_MANAG_DEBUG, stderr, "Signature size: %zuB Comp buff size: %zuB\n", p_file_info->signature_size, comp_buff_size);
        fprintf_log(LOGS_MANAG_DEBUG, stderr,
                    "signature:\n%.*s\n" LOG_SEPARATOR
                    "comp_buff:\n%.*s\n" LOG_SEPARATOR,
                    p_file_info->signature_size, p_file_info->signature,
                    comp_buff_size, comp_buff);

        // Update signature
        p_file_info->signature_size = comp_buff_size;
        memcpy(p_file_info->signature, comp_buff, p_file_info->signature_size);

        freez(comp_buff);

        end_time = get_unix_time_ms();
        fprintf_log(LOGS_MANAG_INFO, stderr, "(3) It took %" PRIu64 "ms to check file rotation.\n", end_time - start_time);
        return rotated;
    }

    m_assert(0, "Should never reach this point");
    return rotated;
}

static void check_if_filesize_changed_cb(uv_fs_t *req) {
    int rc = 0;
    struct File_info *p_file_info = req->data;
    int result = req->result;
    if (unlikely(result < 0)) {
        fprintf_log(LOGS_MANAG_ERROR, stderr, "Error in check_if_filesize_changed_cb (%d): %s\n",
                    result, uv_strerror(result));
        if (result == UV_ENOENT)
            handle_UV_ENOENT_err(p_file_info);
        else
            m_assert(0, "Error in check_if_filesize_changed_cb");
        goto cleanup_and_return;
    }

    if (p_file_info->access_lock) {
        // File already accessed by another check_if_filesize_changed_cb callback
        // TODO: Is access_lock still needed now that events can stop and restart? Keep for now.
        fprintf_log(LOGS_MANAG_ERROR, stderr, "Could not acquire access_lock for %s\n", p_file_info->file_basename);
        (void)enable_file_changed_events(p_file_info, 0);
        goto cleanup_and_return;
    }

    p_file_info->access_lock = 1;
    fprintf_log(LOGS_MANAG_DEBUG, stderr, "Access_lock acquired for %s\n", p_file_info->file_basename);

    // Open file
    // TODO: Keep file open rather than opening/closing each time in this function
    rc = file_open(p_file_info);
    if (unlikely(rc < 0)) {
        fprintf_log(LOGS_MANAG_ERROR, stderr, "Error in file_open() (%d): %s\n", rc, uv_strerror(rc));
        if (rc == UV_ENOENT)
            handle_UV_ENOENT_err(p_file_info);
        else
            m_assert(0, "Error in file_open()");
        p_file_info->access_lock = 0;
        goto cleanup_and_return;
    }

    // Get new filesize
    uv_stat_t *statbuf = uv_fs_get_statbuf(req);
    uint64_t new_filesize = statbuf->st_size;
    fprintf_log(LOGS_MANAG_DEBUG, stderr, "New filesize %s: %" PRIu64 "B\n", p_file_info->file_basename, new_filesize);

    // Check file rotation
    if (check_file_rotation(p_file_info, new_filesize)) {
        p_file_info->filesize = 0;  // New log file - we want to start reading from the beginning!
        fprintf_log(LOGS_MANAG_INFO, stderr, "Rotated:%s\n", p_file_info->filename);
    } else
        fprintf_log(LOGS_MANAG_INFO, stderr, "Not rotated:%s\n", p_file_info->filename);
    uint64_t old_filesize = p_file_info->filesize;

    /* CASE 1: Filesize has increased */
    if (likely((int64_t)new_filesize - (int64_t)old_filesize > 0)) {
        size_t filesize_diff = (size_t)(new_filesize - old_filesize >= MAX_LOG_MSG_SIZE ? MAX_LOG_MSG_SIZE : new_filesize - old_filesize);

#if 1
        if (filesize_diff == MAX_LOG_MSG_SIZE) {
            fprintf_log(LOGS_MANAG_WARNING, stderr, "File %s increased by %" PRIu64
                                         "KB (more than MAX_LOG_MSG_SIZE)! "
                                         "Will read only MAX_LOG_MSG_SIZE instead!\n",
                        p_file_info->file_basename,
                        (new_filesize - old_filesize) / 1000);
            fprintf_log(LOGS_MANAG_DEBUG, stderr, "filesize_diff %s:%" PRIu64 "B:\n", p_file_info->file_basename, filesize_diff);
        }
#endif

        uv_fs_t *read_req = mallocz(sizeof(uv_fs_t));
        read_req->data = p_file_info;
        p_file_info->buff_size = filesize_diff;
        if (!p_file_info->buff || filesize_diff > p_file_info->buff_size_max) {
            p_file_info->buff_size_max = filesize_diff * BUFF_SCALE_FACTOR;
            p_file_info->buff = reallocz(p_file_info->buff, p_file_info->buff_size_max);
        }
        m_assert(p_file_info->buff, "Realloc buffer must not be NULL!");
        p_file_info->uvBuf = uv_buf_init(p_file_info->buff, p_file_info->buff_size);
        rc = uv_fs_read(main_loop, read_req, p_file_info->file_handle,
                        &p_file_info->uvBuf, 1, old_filesize, read_file_cb);
        if (rc)
            fprintf_log(LOGS_MANAG_ERROR, stderr, "uv_fs_read() error for %s\n", p_file_info->filename);
        m_assert(!rc, "uv_fs_read() failed");
        goto cleanup_and_return;
    }
    /* CASE 2: Filesize remains the same */
    else if (unlikely(new_filesize == old_filesize)) {
        fprintf_log(LOGS_MANAG_WARNING, stderr, "%s changed but filesize remains the same\n", p_file_info->file_basename);
    }
    /* CASE 3: Filesize reduced */
    else {
        // TODO: Filesize reduced - error or log archived?? For now just assert
        fprintf_log(LOGS_MANAG_ERROR, stderr, "Filesize of %s reduced by %" PRId64 "B!!",
                    p_file_info->file_basename, (int64_t)new_filesize - (int64_t)old_filesize);
        m_assert(0, "Filesize reduced!");
    }

    p_file_info->access_lock = 0;
    fprintf_log(LOGS_MANAG_DEBUG, stderr, "Access_lock released for %s\n", p_file_info->file_basename);
    (void)file_close(p_file_info);
    (void)enable_file_changed_events(p_file_info, 0);

cleanup_and_return:
    uv_fs_req_cleanup(req);
    freez(req);
}

static void file_changed_cb(uv_fs_event_t *handle, const char *file_basename, int events, int status) {
    int rc = 0;
    struct File_info *p_file_info = handle->data;

    fprintf_log(LOGS_MANAG_DEBUG, stderr, "File changed! %s\n", file_basename ? file_basename : "");

    rc = uv_fs_event_stop(p_file_info->fs_event_req);
    if (rc) {
        fprintf_log(LOGS_MANAG_ERROR, stderr, "uv_fs_event_stop() error for %s: %s \n",
                    p_file_info->filename, uv_strerror(rc));
        m_assert(!rc, "uv_fs_event_stop() failed");
    }
    fprintf_log(LOGS_MANAG_DEBUG, stderr, "%s %s\n", file_basename, !p_file_info->force_file_changed_cb ? "forced file_changed_cb()" : "regular file_changed_cb()");

    // if (events & UV_RENAME)
    //    sleep_ms(LOG_ROTATION_WAIT_TIME);  // If renamed likely that log was rotated - wait a while for new log to be created.

    uv_fs_t *stat_req = mallocz(sizeof(uv_fs_t));
    stat_req->data = p_file_info;

    rc = uv_fs_stat(main_loop, stat_req, p_file_info->filename, check_if_filesize_changed_cb);
    if (unlikely(rc)) {
        fprintf_log(LOGS_MANAG_ERROR, stderr, "uv_fs_stat error: %s\n", uv_strerror(rc));
        m_assert(!rc, "uv_fs_stat error");
    }
    m_assert(!strcmp(file_basename, p_file_info->file_basename), "file_basename argument should equal struct file_basename");
}

static void register_file_changed_listener(struct File_info *p_file_info) {
    int rc = 0;
    fprintf_log(LOGS_MANAG_DEBUG, stderr, "Adding changes listener: %s\n", p_file_info->file_basename);

    uv_fs_event_t *fs_event_req = mallocz(sizeof(uv_fs_event_t));
    fs_event_req->data = p_file_info;
    p_file_info->fs_event_req = fs_event_req;
    rc = uv_fs_event_init(main_loop, fs_event_req);
    if (unlikely(rc))
        fprintf_log(LOGS_MANAG_ERROR, stderr, "uv_fs_event_init() for %s failed\n", p_file_info->filename);
    m_assert(!rc, "uv_fs_event_init() failed");

    p_file_info->enable_file_changed_events_timer = mallocz(sizeof(uv_timer_t));
    rc = uv_timer_init(main_loop, p_file_info->enable_file_changed_events_timer);
    if (unlikely(rc))
        fprintf_log(LOGS_MANAG_ERROR, stderr, "uv_timer_init() for %s failed\n", p_file_info->filename);
    m_assert(!rc, "uv_timer_init() failed");
    uv_fs_event_start(p_file_info->fs_event_req, file_changed_cb, p_file_info->filename, 0);
    if (rc)
        fprintf_log(LOGS_MANAG_ERROR, stderr, "uv_fs_event_start() for %s failed\n", p_file_info->filename);
    m_assert(!rc, "uv_fs_event_start() failed");
}

/*
 * @brief Initialise unique file signature
 * @details Synchronously read the first bytes of a log file to generate a unique signature,
 * which can later be used to identify the file.
 */
static void file_signature_init(struct File_info *p_file_info) {
    int rc = 0;

    p_file_info->signature_size = p_file_info->filesize > MAX_FILE_SIGNATURE_SIZE ? MAX_FILE_SIGNATURE_SIZE : (size_t)p_file_info->filesize;
    p_file_info->signature = mallocz(p_file_info->signature_size);
    p_file_info->uvBuf = uv_buf_init(p_file_info->signature, p_file_info->signature_size);

    uv_fs_t read_req;
    rc = uv_fs_read(main_loop, &read_req, p_file_info->file_handle, &p_file_info->uvBuf, 1, 0, NULL);
    if (unlikely(rc < 0))
        fprintf_log(LOGS_MANAG_ERROR, stderr, "uv_fs_read() for %s failed: (%d) %s\n", p_file_info->filename, rc, uv_strerror(rc));
    m_assert(rc >= 0, "uv_fs_read() failed");
    uv_fs_req_cleanup(&read_req);

    fprintf_log(LOGS_MANAG_INFO, stderr,
                "Initialising signature for file %s, signature size %zu\n",
                p_file_info->file_basename, p_file_info->signature_size);
    fprintf_log(LOGS_MANAG_DEBUG, stderr, "Signature: %s\n" LOG_SEPARATOR, p_file_info->signature);
}

static struct File_info *monitor_log_file_init(const char *filename) {
    int rc = 0;

    fprintf_log(LOGS_MANAG_INFO, stderr,
                "Initialising file monitoring: %s\n",
                filename);

    struct File_info *p_file_info = callocz(1, sizeof(struct File_info));

    p_file_info->filename = filename;            // NOTE: file_basename uses strdup which uses mallocz. freez() if necessary!
    p_file_info->file_basename = get_basename(filename);  // buff pointer must be NULL before first reallocz call

    if ((rc = file_open(p_file_info)) < 0) {
        freez(p_file_info);
        return NULL;
    }

    // Store initial filesize in file_info - synchronous
    uv_fs_t stat_req;
    stat_req.data = p_file_info;
    rc = uv_fs_stat(main_loop, &stat_req, filename, NULL);
    if (unlikely(rc)) {
        fprintf_log(LOGS_MANAG_ERROR, stderr, "uv_fs_stat() error for %s: (%d) %s\n", filename, rc, uv_strerror(rc));
        uv_fs_req_cleanup(&stat_req);
        freez(p_file_info);
        return NULL;
        // m_assert(!rc, "uv_fs_stat() failed");
    } else {
        // Request succeeded; get filesize
        uv_stat_t *statbuf = uv_fs_get_statbuf(&stat_req);
        fprintf_log(LOGS_MANAG_INFO, stderr, "Size of %s: %lldKB\n", p_file_info->filename,
                    (long long)statbuf->st_size / 1000);
        p_file_info->filesize = statbuf->st_size;
    }
    uv_fs_req_cleanup(&stat_req);

    if (!p_file_info->filesize) {  // TODO: Cornercase where filesize is 0 at the beginning - will not work for now. Known bug.
        freez(p_file_info);
        return NULL;
    }

    file_signature_init(p_file_info);  // TODO: Error check

    p_file_info->msg_buff = circ_buff_init();

    register_file_changed_listener(p_file_info);  // TODO: Error check

    // All set up successfully - add p_file_info to list of all p_file_info structs
    p_file_infos_arr->data = reallocz(p_file_infos_arr->data,
                                       (++p_file_infos_arr->count) * (sizeof p_file_info));
    p_file_infos_arr->data[p_file_infos_arr->count - 1] = p_file_info;

    return p_file_info;
}

/**
 * @brief Read configuration of log sources to monitor.
 * @todo How to handle duplicate entries?
 * @todo How to handle missing config file? 
 */
static void config_init(){
    int rc = 0;

    char config_filename[FILENAME_MAX + 1];
    snprintf(config_filename, FILENAME_MAX, "%s/log_management.conf", netdata_configured_user_config_dir);

    if(config_filename && *config_filename) {
        rc = appconfig_load(&log_management_config, config_filename, 0, NULL); // What does 3rd argument do?
        if(!rc){
            fprintf_log(LOGS_MANAG_ERROR, stderr, "CONFIG: cannot load config file '%s'.", config_filename); // TODO: Load stock configuration in this case?
            return;
        }
    }
    else return;
    #if 0
    else {
        config_filename = strdupz_path_subpath(netdata_configured_user_config_dir, "netdata.conf");

        ret = config_load(filename, overwrite_used, NULL);
        if(!ret) {
            info("CONFIG: cannot load user config '%s'. Will try the stock version.", filename);
            freez(filename);

            filename = strdupz_path_subpath(netdata_configured_stock_config_dir, "netdata.conf");
            ret = config_load(filename, overwrite_used, NULL);
            if(!ret)
                info("CONFIG: cannot load stock config '%s'. Running with internal defaults.", filename);
        }

        freez(filename);
    }
    #endif

    struct section *config_section = log_management_config.first_section;
    do{
        fprintf(stderr, "NDLGS Processing section: %s\n", config_section->name);
        int enabled = appconfig_get_boolean(&log_management_config, config_section->name, "enabled", 0);
        fprintf(stderr, "NDLGS Enabled value: %d for section: %s\n", enabled, config_section->name);
        fprintf(stderr, "NDLGS config_section->next NULL? %s\n", config_section->next ? "yes" : "no");

        if(enabled){ // log monitoring for this section is enabled
            char *log_source_path = appconfig_get(&log_management_config, config_section->name, "log path", NULL);
            fprintf(stderr, "NDLGS log path value: %s for section: %s\n==== \n", log_source_path ? log_source_path : "NULL!", config_section->name);
            if(log_source_path && log_source_path[0]!='\0'){ // log source path exists and is valid
                struct File_info *p_file_info = monitor_log_file_init(log_source_path);
                if(p_file_info){ // monitor_log_file_init() was successful
                    char *log_format = appconfig_get(&log_management_config, config_section->name, "log format", NULL);
                    fprintf(stderr, "NDLGS log format value: %s for section: %s\n==== \n", log_format ? log_format : "NULL!", config_section->name);
                    const char delimiter = ' '; // TODO!!: TO READ FROM CONFIG
                    if(!log_format){
                        // TODO: Set default log format and delimiter if not found in config? 
                    }
                    p_file_info->parser_config = read_parse_config(log_format, delimiter);
                    fprintf(stderr, "NDLGS Read parser_config for %s: %s\n", p_file_info->filename, p_file_info->parser_config ? "success!" : "failed!");
                    if(p_file_info->parser_config){ 
                        p_file_info->chart_name = config_section->name ? strdup(config_section->name) : p_file_info->filename;
                        p_file_info->parser_metrics = callocz(1, sizeof(Log_parser_metrics_t));
                        p_file_info->parser_mut = mallocz(sizeof(uv_mutex_t));
                        rc = uv_mutex_init(p_file_info->parser_mut);
                        if(rc) fatal("Failed to initialise parser_mut for %s\n", p_file_info->filename);
                        fprintf(stderr, "NDLGS parser_mut initialised for %s\n", p_file_info->filename);
                    }
                    else{
                        // TODO: Terminate monitor_log_file_init() if p_file_info->parser_config is NULL? 
                    }
                }
            }
        }

        config_section = config_section->next;

    } while(config_section);
}

/**
 * @brief The main function of the program.
 * @details Any static asserts are most likely going to be inluded here. 
 * After any initialisation routines, the default uv_loop_t is executed indefinitely. 
 * @todo Any cleanup required on program exit? 
 */
//int logsmanagement_main(int argc, const char *argv[]) {
void logsmanagement_main(void) {
    //int rc = 0;

    main_loop = mallocz(sizeof(uv_loop_t));
    fatal_assert(uv_loop_init(main_loop) == 0);

    // Static asserts
    COMPILE_TIME_ASSERT(DB_FLUSH_BUFF_INTERVAL > LOG_FILE_READ_INTERVAL);                                      // Do not flush to DB more frequently than reading the logs from the sources.
    COMPILE_TIME_ASSERT(DB_FLUSH_BUFF_INTERVAL / LOG_FILE_READ_INTERVAL < CIRCULAR_BUFF_SIZE);                 // Check if enough circ buffer spaces
    COMPILE_TIME_ASSERT((CIRCULAR_BUFF_SIZE != 0) && ((CIRCULAR_BUFF_SIZE & (CIRCULAR_BUFF_SIZE - 1)) == 0));  // CIRCULAR_BUFF_SIZE must be a power of 2.
    COMPILE_TIME_ASSERT(LOGS_MANAG_DEBUG ? 1 : !VALIDATE_COMPRESSION);                                         // Ensure VALIDATE_COMPRESSION is disabled in release versions.

    // Setup timing
    uint64_t end_time;
    uint64_t start_time = get_unix_time_ms();

    // Initialise array of File_Info pointers
    p_file_infos_arr = mallocz(sizeof(struct File_infos_arr));
    *p_file_infos_arr = (struct File_infos_arr){0};
    (void)uv_mutex_init(&p_file_infos_arr->fs_events_reenable_lock);
    (void)uv_cond_init(&p_file_infos_arr->fs_events_reenable_cond);
    uv_thread_create(&fs_events_reenable_thread_id, fs_events_reenable_thread, NULL);

    config_init();

    fprintf_log(LOGS_MANAG_INFO, stderr, "File monitoring setup completed. Running db_init().\n" LOG_SEPARATOR);
    
    db_init();

    // Timing of setup routines
    end_time = get_unix_time_ms();
    fprintf_log(LOGS_MANAG_INFO, stderr,
                "It took %" PRIu64 "ms to setup.\n" LOG_SEPARATOR,
                end_time - start_time);

#if defined(__STDC_VERSION__)
    fprintf_log(LOGS_MANAG_INFO, stderr, "__STDC_VERSION__: %d\n", __STDC_VERSION__);
#else
    fprintf_log(LOGS_MANAG_INFO, stderr, "__STDC_VERSION__ undefined\n");
#endif
    fprintf_log(LOGS_MANAG_INFO, stderr, "libuv version: %s\n" LOG_SEPARATOR, uv_version_string());
    fprintf_log(LOGS_MANAG_INFO, stderr, "LZ4 version: %s\n" LOG_SEPARATOR, LZ4_versionString());
    char *sqlite_version = db_get_sqlite_version();
    fprintf_log(LOGS_MANAG_INFO, stderr, "SQLITE version: %s\n" LOG_SEPARATOR, sqlite_version);
    freez(sqlite_version);

#if LOGS_MANAGEMENT_STRESS_TEST
    fprintf_log(LOGS_MANAG_INFO, stderr, "p_file_infos_arr->count: %d\n", p_file_infos_arr->count);
    fprintf_log(LOGS_MANAG_INFO, stderr, LOG_SEPARATOR "Running Netdata with logs_management stress test enabled!\n" LOG_SEPARATOR);
    static uv_thread_t run_stress_test_queries_thread_id;
    uv_thread_create(&run_stress_test_queries_thread_id, run_stress_test_queries_thread, NULL);
#endif  // LOGS_MANAGEMENT_STRESS_TEST

    // Run uvlib loop
    uv_run(main_loop, UV_RUN_DEFAULT);
}
