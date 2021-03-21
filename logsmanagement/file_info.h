/** @file file_info.h
 *  @brief Includes the File_info structure definition
 *
 *  @author Dimitris Pantazis
 */

#ifndef FILE_INFO_H_
#define FILE_INFO_H_

#include <sqlite3.h>
#include "config_.h"

// Forward declaration to break circular dependency
struct Circ_buff;

struct File_info {
    uint8_t db_fileInfos_Id; /**< ID of File_info entry in respective metadata table in DB. */
    sqlite3 *db; /**< DB that stores metadata for this log source */
    const char *db_dir; /**< DB and log blob storage path */
    uv_mutex_t *db_mut; /**< DB access mutex */
    uv_file blob_handles[BLOB_MAX_FILES + 1]; /**< Item 0 not used - just for matching 1-1 with DB ids **/
    int blob_write_handle_offset;
    const char *filename;    /**< Full path of log source */
    const char *file_basename;    /**< Basename of log source */
    uv_fs_event_t *fs_event_req;
    uv_timer_t *enable_file_changed_events_timer;
    uint8_t force_file_changed_cb; /**< Boolean to indicate whether an immediate call of the file_changed_cb() function is needed */
    uint64_t filesize;             /**< Offset of where the next read operation needs to start from */
    int8_t access_lock;            /**< Boolean used to forbid a new file read operation before the previous one has finished */
    char *buff;                    /**< Pointed to the base of the buffer used to read the log messages into. */
    size_t buff_size;              /**< Size of the buffer used to read the log messages into. */
    size_t buff_size_max;          /**< Max size of the buffer used to read the log messages into. */
    uv_buf_t uvBuf;                /**< libuv buffer data type, primarily used to read the log messages into. Implemented using #buff and #buff_size. See also http://docs.libuv.org/en/v1.x/misc.html#c.uv_buf_t */
    uv_file file_handle;           /**< File handle */
    struct Circ_buff *msg_buff;         /**< Associated circular buffer - only one should exist per log source. */
    char *signature;               /**< Signature using #signature_size bytes from the beginning of the log to uniquely identify a log file */
    size_t signature_size;         /**< Size of #signature. */
};

struct File_infos_arr {
    // TODO: What is the maximum number of log files we can be monitoring? Currently limited only by fs_events_reenable_list
    struct File_info **data;
    uint8_t count;                      /**< Number of items in array */
    uint64_t fs_events_reenable_list;   /**< Binary list indicating offset of file to attempt to reopen in File_infos_arr. Up to 64 files. */
    uv_mutex_t fs_events_reenable_lock; /**< Mutex for fs_events_reenable_list */
    uv_cond_t fs_events_reenable_cond;
};

extern struct File_infos_arr *p_file_infos_arr;

#endif  // FILE_INFO_H_
