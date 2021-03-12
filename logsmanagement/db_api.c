/** @file db_api.c
 *  @brief This is the file containing the database API
 *
 *  @author Dimitris Pantazis
 */

#include "db_api.h"
#include <inttypes.h>
#include <stdio.h>
#include "circular_buffer.h"
#include "compression.h"
#include "helper.h"
#include "lz4.h"

#define MAIN_DB_DIR "databases" /**< Path to store all the databases and log blobs **/
#define MAIN_DB "main.db" /**< Primary DB with just 1 table - MAIN_COLLECTIONS_TABLE **/
#define MAIN_DB_PATH MAIN_DB_DIR "/" MAIN_DB
#define MAIN_COLLECTIONS_TABLE "LogCollections"
#define BLOB_STORE_FILENAME "logs.bin"
#define METADATA_DB_FILENAME "metadata.db"
#define LOGS_TABLE "Logs"
#define BLOBS_TABLE "Blobs"

static uv_loop_t *db_loop;
static sqlite3 *main_db;

void db_set_lock(uv_mutex_t db_mut) { 
	uv_mutex_lock(&db_mut); 
}

void db_release_lock(uv_mutex_t db_mut) { 
	uv_mutex_unlock(&db_mut);
}

/**
 * @brief Throws fatal SQLite3 error
 * @details In case of a fatal SQLite3 error, the SQLite3 error code will be 
 * translated to a readable error message and logged to stderr. 
 * @param[in] rc SQLite3 error code
 * @param[in] line_no Line number where the error occurred 
 */
static void fatal_sqlite3_err(int rc, int line_no){
	fprintf_log(LOGS_MANAG_ERROR, stderr, "SQLite error: %s (line %d)\n", sqlite3_errstr(rc), line_no);
	fatal("SQLite error: %s (line %d)\n", sqlite3_errstr(rc), line_no);
}

/**
 * @brief Throws fatal libuv error
 * @details In case of a fatal libuv error, the libuv error code will be 
 * translated to a readable error message and logged to stderr. 
 * @param[in] rc libuv error code
 * @param[in] line_no Line number where the error occurred 
 */
static void fatal_libuv_err(int rc, int line_no){
	fprintf_log(LOGS_MANAG_ERROR, stderr, "libuv error: %s (line %d)\n", uv_strerror(rc), line_no);
	fatal("libuv error: %s (line %d)\n", uv_strerror(rc), line_no);
}

/**
 * @brief Get version of SQLite
 * @return String that contains the SQLite version. Must be freed.
 */
char *db_get_sqlite_version() {
    int rc = 0;
    sqlite3_stmt *stmt_get_sqlite_version;
    rc = sqlite3_prepare_v2(main_db,
                            "SELECT sqlite_version();", -1, &stmt_get_sqlite_version, NULL);
    if (unlikely(rc != SQLITE_OK)) fatal_sqlite3_err(rc, __LINE__);
    rc = sqlite3_step(stmt_get_sqlite_version);
    if (unlikely(rc != SQLITE_ROW)) fatal_sqlite3_err(rc, __LINE__);
    char *text = m_malloc(sqlite3_column_bytes(stmt_get_sqlite_version, 0) + 1);
    strcpy(text, (char *)sqlite3_column_text(stmt_get_sqlite_version, 0));
    sqlite3_finalize(stmt_get_sqlite_version);
    return text;
}

static void db_writer(void *arg){
	int rc = 0;
	struct File_info *p_file_info = *((struct File_info **) arg);
	fprintf_log(LOGS_MANAG_DEBUG, stderr, "Entering writer thread for: %s\n", p_file_info->filename);
	
	uv_loop_t *writer_loop = m_malloc(sizeof(uv_loop_t));
    rc = uv_loop_init(writer_loop);
    if (unlikely(rc)) fatal_libuv_err(rc, __LINE__);
    
    /* Prepare LOGS_TABLE INSERT statement */
	sqlite3_stmt *stmt_logs_insert;
	rc = sqlite3_prepare_v2(p_file_info->db,
                        "INSERT INTO " LOGS_TABLE "("
                        "FK_BLOB_Id,"
                        "BLOB_Offset,"
                        "Timestamp,"
                        "Msg_compr_size,"
                        "Msg_decompr_size"
                        ") VALUES (?,?,?,?,?) ;",
                        -1, &stmt_logs_insert, NULL);
    if (unlikely(rc != SQLITE_OK)) fatal_sqlite3_err(rc, __LINE__);
     
    /* Prepare BLOBS_TABLE UPDATE statement */
	sqlite3_stmt *stmt_blobs_update;
	rc = sqlite3_prepare_v2(p_file_info->db,
                            "UPDATE " BLOBS_TABLE
                            " SET Filesize = Filesize + ?"
                            " WHERE Id = ? ;",
                            -1, &stmt_blobs_update, NULL);
    if (unlikely(rc != SQLITE_OK)) fatal_sqlite3_err(rc, __LINE__);
    
    /* Prepare BLOBS_TABLE Filename rotate statement */
	sqlite3_stmt *stmt_rotate_blobs;
	rc = sqlite3_prepare_v2(p_file_info->db,
							"UPDATE " BLOBS_TABLE
							" SET Filename = REPLACE(Filename, ?, ?);",
							-1, &stmt_rotate_blobs, NULL);
	if (unlikely(rc != SQLITE_OK)) fatal_sqlite3_err(rc, __LINE__);
	
	/* Prepare BLOBS_TABLE UPDATE SET zero filesize statement */
	sqlite3_stmt *stmt_blobs_set_zero_filesize;
	rc = sqlite3_prepare_v2(p_file_info->db,
                            "UPDATE " BLOBS_TABLE
                            " SET Filesize = 0"
                            " WHERE Id = ? ;",
                            -1, &stmt_blobs_set_zero_filesize, NULL);
    if (unlikely(rc != SQLITE_OK)) fatal_sqlite3_err(rc, __LINE__);
    
    /* Prepare LOGS_TABLE DELETE statement */
	sqlite3_stmt *stmt_logs_delete;
	rc = sqlite3_prepare_v2(p_file_info->db,
                            "DELETE FROM " LOGS_TABLE
                            " WHERE FK_BLOB_Id = ? ;",
                            -1, &stmt_logs_delete, NULL);
    if (unlikely(rc != SQLITE_OK)) fatal_sqlite3_err(rc, __LINE__);
		
	/* Get initial filesize of logs.bin.0 BLOB */
    sqlite3_stmt *stmt_retrieve_filesize_from_id;
	rc = sqlite3_prepare_v2(p_file_info->db,
							"SELECT Filesize FROM " BLOBS_TABLE 
							" WHERE Id = ? ;",
							-1, &stmt_retrieve_filesize_from_id, NULL);
	if (unlikely(rc != SQLITE_OK)) fatal_sqlite3_err(rc, __LINE__);
	rc = sqlite3_bind_int(stmt_retrieve_filesize_from_id, 1, p_file_info->blob_write_handle_offset);
    if (unlikely(rc != SQLITE_OK)) fatal_sqlite3_err(rc, __LINE__);
    rc = sqlite3_step(stmt_retrieve_filesize_from_id);
	if (unlikely(rc != SQLITE_ROW)) fatal_sqlite3_err(rc, __LINE__);
	int64_t blob_filesize = (int64_t) sqlite3_column_int64(stmt_retrieve_filesize_from_id, 0);
	sqlite3_finalize(stmt_retrieve_filesize_from_id);
	
	fprintf_log(LOGS_MANAG_DEBUG, stderr, "initial blob_filesize: %" PRId64 "\n", blob_filesize);
	
	
	Message_t *p_msg;
	uv_fs_t write_req;
	uv_fs_t dsync_req;
	uv_fs_t rename_req;
	uv_fs_t trunc_req;
	while(1){
		db_set_lock(p_file_info->db_mut);
		sqlite3_exec(p_file_info->db, "BEGIN TRANSACTION;", NULL, NULL, NULL);
		p_msg = circ_buff_read(p_file_info->msg_buff);
		while (p_msg) {  // Retrieve msgs and store in DB until there are no more msgs in the buffer
			
			/* Write log message in BLOB */
			uv_buf_t uv_buf = uv_buf_init((char *) p_msg->text_compressed, (unsigned int) p_msg->text_compressed_size);
			rc = uv_fs_write(writer_loop, &write_req, 
				p_file_info->blob_handles[p_file_info->blob_write_handle_offset], 
				&uv_buf, 1, blob_filesize, NULL); // Write synchronously at the end of the BLOB file
			if(unlikely(rc < 0)) fatal("Failed to write logs management BLOB");
			uv_fs_req_cleanup(&write_req);
			
			/* Write metadata of log message in LOGS_TABLE */
			fprintf_log(LOGS_MANAG_DEBUG, stderr, "DB msg timestamp: %" PRIu64 "\n", p_msg->timestamp);
            fprintf_log(LOGS_MANAG_DEBUG, stderr, "DB msg size: %zu\n", p_msg->text_size);
            rc = sqlite3_bind_int(stmt_logs_insert, 1, p_file_info->blob_write_handle_offset);
            if (unlikely(rc != SQLITE_OK)) fatal_sqlite3_err(rc, __LINE__);
            rc = sqlite3_bind_int64(stmt_logs_insert, 2, (sqlite3_int64) blob_filesize);
            if (unlikely(rc != SQLITE_OK)) fatal_sqlite3_err(rc, __LINE__);
            rc = sqlite3_bind_int64(stmt_logs_insert, 3, (sqlite3_int64) p_msg->timestamp);
            if (unlikely(rc != SQLITE_OK)) fatal_sqlite3_err(rc, __LINE__);
            rc = sqlite3_bind_int64(stmt_logs_insert, 4, (sqlite3_int64) p_msg->text_compressed_size);
            if (unlikely(rc != SQLITE_OK)) fatal_sqlite3_err(rc, __LINE__);
            rc = sqlite3_bind_int64(stmt_logs_insert, 5, (sqlite3_int64)p_msg->text_size);
            if (unlikely(rc != SQLITE_OK)) fatal_sqlite3_err(rc, __LINE__);
            rc = sqlite3_step(stmt_logs_insert);
            if (unlikely(rc != SQLITE_DONE)) fatal_sqlite3_err(rc, __LINE__);
            sqlite3_reset(stmt_logs_insert);
            
            /* Update metadata of BLOBs filesize in BLOBS_TABLE */
            rc = sqlite3_bind_int64(stmt_blobs_update, 1, (sqlite3_int64)p_msg->text_compressed_size);
            if (unlikely(rc != SQLITE_OK)) fatal_sqlite3_err(rc, __LINE__);
            rc = sqlite3_bind_int(stmt_blobs_update, 2, p_file_info->blob_write_handle_offset);
            if (unlikely(rc != SQLITE_OK)) fatal_sqlite3_err(rc, __LINE__);
            rc = sqlite3_step(stmt_blobs_update);
            if (unlikely(rc != SQLITE_DONE)) fatal_sqlite3_err(rc, __LINE__);
            sqlite3_reset(stmt_blobs_update);
			
			/* Increase BLOB offset and read next log message until no more messages in buff */
			blob_filesize += (int64_t) p_msg->text_compressed_size;
			p_msg = circ_buff_read(p_file_info->msg_buff);
		}
		rc = uv_fs_fdatasync(writer_loop, &dsync_req, 
			p_file_info->blob_handles[p_file_info->blob_write_handle_offset], NULL);
		if (unlikely(rc)) fatal_libuv_err(rc, __LINE__);
		sqlite3_exec(p_file_info->db, "END TRANSACTION;", NULL, NULL, NULL);
		//sqlite3_wal_checkpoint_v2(p_file_info->db,NULL,SQLITE_CHECKPOINT_PASSIVE,0,0);
		
		/* If the filesize of the current write-to BLOB is > BLOB_MAX_SIZE, rotate BLOBs */
		if(blob_filesize > BLOB_MAX_SIZE){
			const uint64_t start_time = get_unix_time_ms();
			char old_path[MAX_PATH_LENGTH + 1], new_path[MAX_PATH_LENGTH + 1];

			/* 1. Rotate BLOBS_TABLE Filenames and path of actual BLOBs. 
			 * Performed in 2 steps: 
			 * (a) First increase all of their endings numbers by 1 and 
			 * (b) then replace the maximum number with 0. */
			sqlite3_exec(p_file_info->db, "BEGIN TRANSACTION;", NULL, NULL, NULL);
			for(int i = BLOB_MAX_FILES - 1; i >= 0; i--){
				
				/* Rotate BLOBS_TABLE Filenames */
				rc = sqlite3_bind_int(stmt_rotate_blobs, 1, i);
				if (rc != SQLITE_OK) fatal_sqlite3_err(rc, __LINE__);
				rc = sqlite3_bind_int(stmt_rotate_blobs, 2, i + 1);
				if (rc != SQLITE_OK) fatal_sqlite3_err(rc, __LINE__);
				rc = sqlite3_step(stmt_rotate_blobs);
				if (unlikely(rc != SQLITE_DONE)) fatal_sqlite3_err(rc, __LINE__);
				sqlite3_reset(stmt_rotate_blobs);
				
				/* Rotate path of BLOBs */
				sprintf(old_path, "%s" BLOB_STORE_FILENAME ".%d", p_file_info->db_dir, i);
				sprintf(new_path, "%s" BLOB_STORE_FILENAME ".%d", p_file_info->db_dir, i + 1);
				rc = uv_fs_rename(writer_loop, &rename_req, old_path, new_path, NULL);
				if (unlikely(rc)) fatal_libuv_err(rc, __LINE__);
				uv_fs_req_cleanup(&rename_req);
			}
			/* Replace the maximum number with 0 in SQLite DB. */
			rc = sqlite3_bind_int(stmt_rotate_blobs, 1, BLOB_MAX_FILES);
			if (rc != SQLITE_OK) fatal_sqlite3_err(rc, __LINE__);
			rc = sqlite3_bind_int(stmt_rotate_blobs, 2, 0);
			if (rc != SQLITE_OK) fatal_sqlite3_err(rc, __LINE__);
			rc = sqlite3_step(stmt_rotate_blobs);
			if (unlikely(rc != SQLITE_DONE)) fatal_sqlite3_err(rc, __LINE__);
			sqlite3_reset(stmt_rotate_blobs);
			sqlite3_exec(p_file_info->db, "END TRANSACTION;", NULL, NULL, NULL);
			
			/* Replace the maximum number with 0 in BLOB files. */
			sprintf(old_path, "%s" BLOB_STORE_FILENAME ".%d", p_file_info->db_dir, BLOB_MAX_FILES);
			sprintf(new_path, "%s" BLOB_STORE_FILENAME ".%d", p_file_info->db_dir, 0);
			rc = uv_fs_rename(writer_loop, &rename_req, old_path, new_path, NULL);
			if (unlikely(rc)) fatal_libuv_err(rc, __LINE__);
			uv_fs_req_cleanup(&rename_req);
			
			/* (a) Update blob_write_handle_offset, (b) truncate new write-to BLOB, 
			 * (c) update filesize of truncated BLOB in SQLite DB, (d) delete
			 * respective logs in LOGS_TABLE for the truncated BLOB and (e)
			 * reset blob_filesize */
			/* (a) */ 
			p_file_info->blob_write_handle_offset = p_file_info->blob_write_handle_offset == 1 ? BLOB_MAX_FILES : p_file_info->blob_write_handle_offset - 1;
			/* (b) */ 
			rc = uv_fs_ftruncate(writer_loop, &trunc_req, p_file_info->blob_handles[p_file_info->blob_write_handle_offset], 0, NULL);						
			if (unlikely(rc)) fatal_libuv_err(rc, __LINE__);
			uv_fs_req_cleanup(&trunc_req);
			/* (c) */ 
			rc = sqlite3_bind_int(stmt_blobs_set_zero_filesize, 1, p_file_info->blob_write_handle_offset);
            if (rc != SQLITE_OK) fatal_sqlite3_err(rc, __LINE__);
            rc = sqlite3_step(stmt_blobs_set_zero_filesize);
			if (unlikely(rc != SQLITE_DONE)) fatal_sqlite3_err(rc, __LINE__);
			sqlite3_reset(stmt_blobs_set_zero_filesize);
			/* (d) */
			rc = sqlite3_bind_int(stmt_logs_delete, 1, p_file_info->blob_write_handle_offset);
            if (rc != SQLITE_OK) fatal_sqlite3_err(rc, __LINE__);
            rc = sqlite3_step(stmt_logs_delete);
			if (unlikely(rc != SQLITE_DONE)) fatal_sqlite3_err(rc, __LINE__);
			sqlite3_reset(stmt_logs_delete);
			/* (e) */
			blob_filesize = 0;

			fprintf_log(LOGS_MANAG_INFO, stderr,
                "It took %" PRId64 "ms to rotate BLOBs\n",
                (int64_t)get_unix_time_ms() - start_time);
		}
		// TODO: Can db_release_lock(p_file_info->db_mut) be moved before if(blob_filesize > BLOB_MAX_SIZE) ?
		db_release_lock(p_file_info->db_mut);
		sleep_ms(DB_FLUSH_BUFF_INTERVAL);
	}
}

void db_init() {
	int rc = 0;
    char *err_msg = 0;
    uv_fs_t mkdir_req;
    
    db_loop = m_malloc(sizeof(uv_loop_t));
    rc = uv_loop_init(db_loop);
    if (unlikely(rc)) fatal_libuv_err(rc, __LINE__);

	/* Create databases directory if it doesn't exist. */
    rc = uv_fs_mkdir(db_loop, &mkdir_req, MAIN_DB_DIR, 0755, NULL);
    if (likely(rc)) 
        fprintf_log(LOGS_MANAG_WARNING, stderr, "Mkdir " MAIN_DB_DIR "/ error: %s\n", uv_strerror(rc));
    uv_fs_req_cleanup(&mkdir_req);

#if 0 // FOR STRESS TESTING PURPOSES ONLY! - Temporarily comment out
    uv_fs_t unlink_req;
    rc = uv_fs_unlink(db_loop, &unlink_req, MAIN_DB_PATH, NULL);
    if (rc)
        fprintf(stderr, "Delete " MAIN_DB_PATH " error: %s\n", uv_strerror(rc));
    uv_fs_req_cleanup(&unlink_req);

    // TODO: Delete all external blob files if exist
#endif  // STRESS_TEST

	/* Create or open main db */
	rc = sqlite3_open(MAIN_DB_PATH, &main_db);
    if (unlikely(rc != SQLITE_OK)) fatal_sqlite3_err(rc, __LINE__);
    
    /* Configure main database */
    // TODO: Does the following enable foreign keys? Test.
    rc = sqlite3_exec(main_db,
                      "PRAGMA auto_vacuum = INCREMENTAL;"
                      "PRAGMA synchronous = 1;"
                      "PRAGMA journal_mode = WAL;"
                      "PRAGMA temp_store = MEMORY;"
                      "PRAGMA foreign_keys = ON;",
                      0, 0, &err_msg);
    if (unlikely(rc != SQLITE_OK)) {
        fprintf_log(LOGS_MANAG_ERROR, stderr, "Failed to configure database\n");
        fprintf_log(LOGS_MANAG_ERROR, stderr, "SQL error: %s\n", err_msg);
        sqlite3_free(err_msg);
        fatal("Failed to configure database, SQL error: %s\n", err_msg);
    } else {
        fprintf_log(LOGS_MANAG_INFO, stderr, "Database configured successfully\n");
    }
    
    /* Create new main DB LogCollections table if it doesn't exist */
    rc = sqlite3_exec(main_db,
                      "CREATE TABLE IF NOT EXISTS " MAIN_COLLECTIONS_TABLE "("
                      "Id 					INTEGER 	PRIMARY KEY,"
                      "Log_Source_Filename	TEXT		NOT NULL,"
                      "DB_Dir 				TEXT 		NOT NULL"
                      ");",
                      0, 0, &err_msg);
    if (unlikely(rc != SQLITE_OK)) {
        fprintf_log(LOGS_MANAG_ERROR, stderr, "Failed to create table\n");
        fprintf_log(LOGS_MANAG_ERROR, stderr, "SQL error: %s\n", err_msg);
        sqlite3_free(err_msg);
        m_assert(0, "Failed to create table" MAIN_COLLECTIONS_TABLE);
    } else fprintf_log(LOGS_MANAG_INFO, stderr, "Table %s created successfully\n", MAIN_COLLECTIONS_TABLE);
    
    /* Create DB subdirectories and metadata DBs for each log source. Create/open binaries. */
    sqlite3_stmt *stmt_get_last_id;
    rc = sqlite3_prepare_v2(main_db,
                            "SELECT MAX(Id) FROM " MAIN_COLLECTIONS_TABLE ";",
                            -1, &stmt_get_last_id, NULL);
    if (unlikely(rc != SQLITE_OK)) fatal_sqlite3_err(rc, __LINE__);
    
    sqlite3_stmt *stmt_search_if_log_source_exists;
    rc = sqlite3_prepare_v2(main_db,
                            "SELECT COUNT(*), Id, DB_Dir FROM " MAIN_COLLECTIONS_TABLE
                            " WHERE Log_Source_Filename = ? ;",
                            -1, &stmt_search_if_log_source_exists, NULL);
    if (unlikely(rc != SQLITE_OK)) fatal_sqlite3_err(rc, __LINE__);
    
    sqlite3_stmt *stmt_insert_log_collection_metadata;
    rc = sqlite3_prepare_v2(main_db,
                            "INSERT INTO " MAIN_COLLECTIONS_TABLE
                            " (Log_Source_Filename, DB_Dir) VALUES (?,?) ;",
                            -1, &stmt_insert_log_collection_metadata, NULL);
    if (unlikely(rc != SQLITE_OK)) fatal_sqlite3_err(rc, __LINE__);
    
    for (int i = 0; i < p_file_infos_arr->count; i++) {
        rc = sqlite3_bind_text(stmt_search_if_log_source_exists, 1, p_file_infos_arr->data[i]->filename, -1, NULL);
        if (rc != SQLITE_OK) fatal_sqlite3_err(rc, __LINE__);
        rc = sqlite3_step(stmt_search_if_log_source_exists);
        /* COUNT(*) query should always return SQLITE_ROW */
        if (unlikely(rc != SQLITE_ROW)) fatal_sqlite3_err(rc, __LINE__);
        
        int log_source_occurences = sqlite3_column_int(stmt_search_if_log_source_exists, 0);
        fprintf_log(LOGS_MANAG_INFO, stderr, "DB file occurences of %s: %d\n", 
                p_file_infos_arr->data[i]->filename, log_source_occurences);
        switch (log_source_occurences) {
            case 0:  /* Log collection metadata not found in main DB - create a new record */
				
				/* Bind log source filename */
                rc = sqlite3_bind_text(stmt_insert_log_collection_metadata, 1, p_file_infos_arr->data[i]->filename, -1, NULL);
                if (unlikely(rc != SQLITE_OK)) fatal_sqlite3_err(rc, __LINE__);
                
                /* Create directory of collection of logs for the particular log source and insert it into DB. 
                 * The directory will be in the format of '(log source file_basename)_(unique id)'. 
                 * The unique id is the maximum id of the MAIN_COLLECTIONS_TABLE. */
                rc = sqlite3_step(stmt_get_last_id);
                if (unlikely(rc != SQLITE_ROW)) fatal_sqlite3_err(rc, __LINE__);
                
				int last_row_id = sqlite3_column_int(stmt_get_last_id, 0);
                char *db_dir = m_malloc(snprintf(NULL, 0, MAIN_DB_DIR "/%s_%d/", p_file_infos_arr->data[i]->file_basename, last_row_id) + 1);
				sprintf(db_dir, MAIN_DB_DIR "/%s_%d/", p_file_infos_arr->data[i]->file_basename, last_row_id);
				
				rc = uv_fs_mkdir(db_loop, &mkdir_req, db_dir, 0755, NULL);
				if (unlikely(rc)) fatal_libuv_err(rc, __LINE__); // If db_dir exists but was not found in main DB, then that's a fatal()
				uv_fs_req_cleanup(&mkdir_req);
				
                rc = sqlite3_bind_text(stmt_insert_log_collection_metadata, 2, db_dir, -1, NULL);
                if (unlikely(rc != SQLITE_OK)) fatal_sqlite3_err(rc, __LINE__);
                rc = sqlite3_step(stmt_insert_log_collection_metadata);
                if (unlikely(rc != SQLITE_DONE)) fatal_sqlite3_err(rc, __LINE__);
					
					
                /* Backlink to db_fileInfos_Id of related p_file_info struct */
                // TODO: Is this needed? Do we still use db_fileInfos_Id?
                p_file_infos_arr->data[i]->db_fileInfos_Id = (uint8_t)sqlite3_last_insert_rowid(main_db);
                p_file_infos_arr->data[i]->db_dir = db_dir;
                
                sqlite3_reset(stmt_get_last_id);
                sqlite3_reset(stmt_insert_log_collection_metadata);
                break;
                
            case 1:  // File metadata found in DB - retrieve id
                p_file_infos_arr->data[i]->db_fileInfos_Id = (uint8_t)sqlite3_column_int(stmt_search_if_log_source_exists, 1);
                
                p_file_infos_arr->data[i]->db_dir = m_malloc((size_t)sqlite3_column_bytes(stmt_search_if_log_source_exists, 2));
                sprintf((char*) p_file_infos_arr->data[i]->db_dir, "%s", sqlite3_column_text(stmt_search_if_log_source_exists, 2));
                break;
                
            default:  // Error, file metadata can exist either 0 or 1 times in DB
                fprintf_log(LOGS_MANAG_ERROR, stderr,
                            "Error: %s record encountered multiple times in DB " MAIN_COLLECTIONS_TABLE " table \n",
                            p_file_infos_arr->data[i]->filename);
                m_assert(0, "Same file stored in DB more than once!");
                fatal("Error: %s record encountered multiple times in DB " MAIN_COLLECTIONS_TABLE " table \n",
                            p_file_infos_arr->data[i]->filename);
        }
        sqlite3_reset(stmt_search_if_log_source_exists);
        
        /* Create or open metadata DBs for each log collection */
        char *db_metadata_path = m_malloc(snprintf(NULL, 0, "%s" METADATA_DB_FILENAME, p_file_infos_arr->data[i]->db_dir) + 1);
		sprintf(db_metadata_path, "%s" METADATA_DB_FILENAME, p_file_infos_arr->data[i]->db_dir);
		rc = sqlite3_open(db_metadata_path, &p_file_infos_arr->data[i]->db);
		if (unlikely(rc != SQLITE_OK)) fatal_sqlite3_err(rc, __LINE__);
		m_free(db_metadata_path);

		/* Initialise DB mutex */
		rc = uv_mutex_init(&p_file_infos_arr->data[i]->db_mut);
	    if (unlikely(rc)) fatal_libuv_err(rc, __LINE__);
		
		/* Configure metadata DB */
		rc = sqlite3_exec(p_file_infos_arr->data[i]->db,
						  "PRAGMA auto_vacuum = INCREMENTAL;"
						  "PRAGMA synchronous = 1;"
						  "PRAGMA journal_mode = WAL;"
						  "PRAGMA temp_store = MEMORY;"
						  "PRAGMA foreign_keys = ON;",
						  0, 0, &err_msg);
		if (unlikely(rc != SQLITE_OK)) {
			fprintf_log(LOGS_MANAG_ERROR, stderr, "Failed to configure database for %s\n", p_file_infos_arr->data[i]->filename);
			fprintf_log(LOGS_MANAG_ERROR, stderr, "SQL error: %s\n", err_msg);
			sqlite3_free(err_msg);
			fatal("Failed to configure database for %s\n, SQL error: %s\n", p_file_infos_arr->data[i]->filename, err_msg);
		} else fprintf_log(LOGS_MANAG_INFO, stderr, "Database configured successfully\n");
		
		/* Check if BLOBS_TABLE exists or not */
		sqlite3_stmt *stmt_check_if_BLOBS_TABLE_exists;
		rc = sqlite3_prepare_v2(p_file_infos_arr->data[i]->db,
								"SELECT COUNT(*) FROM sqlite_master" 
								" WHERE type='table' AND name='"BLOBS_TABLE"';",
								-1, &stmt_check_if_BLOBS_TABLE_exists, NULL);
		if (unlikely(rc != SQLITE_OK)) fatal_sqlite3_err(rc, __LINE__);
		rc = sqlite3_step(stmt_check_if_BLOBS_TABLE_exists);
		if (unlikely(rc != SQLITE_ROW)) fatal_sqlite3_err(rc, __LINE__); /* COUNT(*) query should always return SQLITE_ROW */
		
		/* If BLOBS_TABLE doesn't exist, create and populate it */
		if(sqlite3_column_int(stmt_check_if_BLOBS_TABLE_exists, 0) == 0){
			
			/* 1. Create it */
			rc = sqlite3_exec(p_file_infos_arr->data[i]->db,
                      "CREATE TABLE IF NOT EXISTS " BLOBS_TABLE "("
                      "Id 		INTEGER 	PRIMARY KEY,"
                      "Filename	TEXT		NOT NULL,"
                      "Filesize INTEGER 	NOT NULL"
                      ");",
                      0, 0, &err_msg);
			if (unlikely(rc != SQLITE_OK)) {
				fprintf_log(LOGS_MANAG_ERROR, stderr, "Failed to create %s. SQL error: %s\n", BLOBS_TABLE, err_msg);
				sqlite3_free(err_msg);
				fatal("Failed to create %s. SQL error: %s\n", BLOBS_TABLE, err_msg);
			} else fprintf_log(LOGS_MANAG_INFO, stderr, "Table %s created successfully\n", BLOBS_TABLE);
			
			/* 2. Populate it */
			sqlite3_stmt *stmt_init_BLOBS_table;
			rc = sqlite3_prepare_v2(p_file_infos_arr->data[i]->db,
                            "INSERT INTO " BLOBS_TABLE 
                            " (Filename, Filesize) VALUES (?,?) ;",
                            -1, &stmt_init_BLOBS_table, NULL);
			if (unlikely(rc != SQLITE_OK)) fatal_sqlite3_err(rc, __LINE__);
			for( int i = 0; i < BLOB_MAX_FILES; i++){
				char *filename = m_malloc(snprintf(NULL, 0, BLOB_STORE_FILENAME ".%d", i) + 1);
				sprintf(filename, BLOB_STORE_FILENAME ".%d", i);
				rc = sqlite3_bind_text(stmt_init_BLOBS_table, 1, filename, -1, NULL);
                if (rc != SQLITE_OK) fatal_sqlite3_err(rc, __LINE__);
                rc = sqlite3_bind_int64(stmt_init_BLOBS_table, 2, (sqlite3_int64) 0);
                if (rc != SQLITE_OK) fatal_sqlite3_err(rc, __LINE__);      
				rc = sqlite3_step(stmt_init_BLOBS_table);
				if (rc != SQLITE_DONE) fatal_sqlite3_err(rc, __LINE__);
				sqlite3_reset(stmt_init_BLOBS_table);
				m_free(filename);
			}
			sqlite3_finalize(stmt_init_BLOBS_table);
		}
		sqlite3_finalize(stmt_check_if_BLOBS_TABLE_exists);
		
		/* If LOGS_TABLE doesn't exist, create it */
		rc = sqlite3_exec(p_file_infos_arr->data[i]->db,
                      "CREATE TABLE IF NOT EXISTS " LOGS_TABLE "("
                      "Id 					INTEGER 	PRIMARY KEY,"
                      "FK_BLOB_Id			INTEGER		NOT NULL,"
                      "BLOB_Offset			INTEGER		NOT NULL,"
                      "Timestamp 			INTEGER		NOT NULL,"
                      "Msg_compr_size 		INTEGER		NOT NULL,"
                      "Msg_decompr_size 	INTEGER		NOT NULL,"
                      "FOREIGN KEY (FK_BLOB_Id) REFERENCES " BLOBS_TABLE " (Id) ON DELETE CASCADE ON UPDATE CASCADE"
                      ");",
                      0, 0, &err_msg);
		if (unlikely(rc != SQLITE_OK)) {
			fprintf_log(LOGS_MANAG_ERROR, stderr, "Failed to create %s. SQL error: %s\n", LOGS_TABLE, err_msg);
			sqlite3_free(err_msg);
			fatal("Failed to create %s. SQL error: %s\n", LOGS_TABLE, err_msg);
		} else fprintf_log(LOGS_MANAG_INFO, stderr, "Table %s created successfully\n", LOGS_TABLE);
		
		// Create index on LOGS_TABLE Timestamp
		/* TODO: If this doesn't speed up queries, check SQLITE R*tree module. 
		 * Requires benchmarking with/without index. */
		rc = sqlite3_exec(p_file_infos_arr->data[i]->db,
						  "CREATE INDEX IF NOT EXISTS logs_timestamps_idx "
						  "ON " LOGS_TABLE "(Timestamp);",
						  0, 0, &err_msg);
		if (unlikely(rc != SQLITE_OK)) {
			fprintf_log(LOGS_MANAG_ERROR, stderr, "Failed to create logs_timestamps_idx. SQL error: %s\n", err_msg);
			sqlite3_free(err_msg);
		} else fprintf_log(LOGS_MANAG_INFO, stderr, "logs_timestamps_idx created successfully\n");

		/* Remove excess BLOBs beyond BLOB_MAX_FILES (from both DB and disk storage) */
		{
			sqlite3_stmt *stmt_get_BLOBS_TABLE_size;
			rc = sqlite3_prepare_v2(p_file_infos_arr->data[i]->db,
				"SELECT MAX(Id) FROM " BLOBS_TABLE ";",
				-1, &stmt_get_BLOBS_TABLE_size, NULL);
			if (rc != SQLITE_OK) fatal_sqlite3_err(rc, __LINE__);
			rc = sqlite3_step(stmt_get_BLOBS_TABLE_size);
			if (rc != SQLITE_ROW) fatal_sqlite3_err(rc, __LINE__);
			int blobs_table_max_id = sqlite3_column_int(stmt_get_BLOBS_TABLE_size, 0);

			sqlite3_stmt *stmt_retrieve_filename_last_digits; // This statement retrieves the last digit(s) from the Filename column of BLOBS_TABLE
			rc = sqlite3_prepare_v2(p_file_infos_arr->data[i]->db,
				"WITH split(word, str) AS ( SELECT '', (SELECT Filename FROM " BLOBS_TABLE " WHERE Id = ? ) || '.' "
				"UNION ALL SELECT substr(str, 0, instr(str, '.')), substr(str, instr(str, '.')+1) FROM split WHERE str!='' ) "
				"SELECT word FROM split WHERE word!='' ORDER BY LENGTH(str) LIMIT 1;",
				-1, &stmt_retrieve_filename_last_digits, NULL);
			if (rc != SQLITE_OK) fatal_sqlite3_err(rc, __LINE__);

			sqlite3_stmt *stmt_delete_row_by_id; 
			rc = sqlite3_prepare_v2(p_file_infos_arr->data[i]->db,
				"DELETE FROM " BLOBS_TABLE " WHERE Id = ?;",
				-1, &stmt_delete_row_by_id, NULL);
			if (rc != SQLITE_OK) fatal_sqlite3_err(rc, __LINE__);

			for (int id = 1; id <= blobs_table_max_id; id++){

				// fprintf_log(LOGS_MANAG_DEBUG, stderr, "----Id:: %d\n", id);
				rc = sqlite3_bind_int(stmt_retrieve_filename_last_digits, 1, id);
				if (rc != SQLITE_OK) fatal_sqlite3_err(rc, __LINE__);
				rc = sqlite3_step(stmt_retrieve_filename_last_digits);
				if (rc != SQLITE_ROW) fatal_sqlite3_err(rc, __LINE__);
				int last_digits = sqlite3_column_int(stmt_retrieve_filename_last_digits, 0);
				sqlite3_reset(stmt_retrieve_filename_last_digits);

				/* If last_digits > BLOB_MAX_FILES - 1, then some BLOB files will need to be removed
				 * (both from DB BLOBS_TABLE and also from the disk) */
				if(last_digits > BLOB_MAX_FILES - 1){

					// Delete entry from DB BLOBS_TABLE
					rc = sqlite3_bind_int(stmt_delete_row_by_id, 1, id);
					if (rc != SQLITE_OK) fatal_sqlite3_err(rc, __LINE__);
					rc = sqlite3_step(stmt_delete_row_by_id);
					if (rc != SQLITE_DONE) fatal_sqlite3_err(rc, __LINE__);
					sqlite3_reset(stmt_delete_row_by_id);

					// Delete BLOB file from filesystem
					char blob_delete_path[MAX_PATH_LENGTH + 1];
					sprintf(blob_delete_path, "%s" BLOB_STORE_FILENAME ".%d", p_file_infos_arr->data[i]->db_dir, last_digits);
					uv_fs_t unlink_req;
				    rc = uv_fs_unlink(db_loop, &unlink_req, blob_delete_path, NULL);
				    if (rc) fprintf(stderr, "Delete %s error: %s\n", blob_delete_path, uv_strerror(rc));
				    uv_fs_req_cleanup(&unlink_req);
				   
				}
			}
			sqlite3_finalize(stmt_retrieve_filename_last_digits);
			sqlite3_finalize(stmt_delete_row_by_id);


			int old_blobs_table_ids[BLOB_MAX_FILES];
			int off = 0;
			sqlite3_stmt *stmt_retrieve_all_ids; 
			rc = sqlite3_prepare_v2(p_file_infos_arr->data[i]->db,
				"SELECT Id FROM " BLOBS_TABLE " ORDER BY Id ASC;",
				-1, &stmt_retrieve_all_ids, NULL);
			if (rc != SQLITE_OK) fatal_sqlite3_err(rc, __LINE__);

			rc = sqlite3_step(stmt_retrieve_all_ids);
			while(rc == SQLITE_ROW){
				old_blobs_table_ids[off++] = sqlite3_column_int(stmt_retrieve_all_ids, 0);
				rc = sqlite3_step(stmt_retrieve_all_ids);
			}
			if (rc != SQLITE_DONE) fatal_sqlite3_err(rc, __LINE__);
			sqlite3_finalize(stmt_retrieve_all_ids);

			sqlite3_stmt *stmt_update_id; 
			rc = sqlite3_prepare_v2(p_file_infos_arr->data[i]->db,
				"UPDATE " BLOBS_TABLE " SET Id = ? WHERE Id = ?;",
				-1, &stmt_update_id, NULL);
			if (rc != SQLITE_OK) fatal_sqlite3_err(rc, __LINE__);

			for (int i = 0; i < BLOB_MAX_FILES; i++){
				// fprintf_log(LOGS_MANAG_DEBUG, stderr, "----Id to set:: %d\n", i + 1);
				// fprintf_log(LOGS_MANAG_DEBUG, stderr, "----Id before:: %d\n", old_blobs_table_ids[i]);
				rc = sqlite3_bind_int(stmt_update_id, 1, i + 1);
				if (rc != SQLITE_OK) fatal_sqlite3_err(rc, __LINE__);
				rc = sqlite3_bind_int(stmt_update_id, 2, old_blobs_table_ids[i]);
				if (rc != SQLITE_OK) fatal_sqlite3_err(rc, __LINE__);
				rc = sqlite3_step(stmt_update_id);
				if (rc != SQLITE_DONE) fatal_sqlite3_err(rc, __LINE__);
				sqlite3_reset(stmt_update_id);
			}
			sqlite3_finalize(stmt_update_id);

		}

		/* Traverse BLOBS_TABLE, open logs.bin.X files and store their file handles in p_file_info array. */
		sqlite3_stmt *stmt_retrieve_metadata_from_id;
		rc = sqlite3_prepare_v2(p_file_infos_arr->data[i]->db,
								"SELECT Filename, Filesize FROM " BLOBS_TABLE 
								" WHERE Id = ? ;",
								-1, &stmt_retrieve_metadata_from_id, NULL);
		if (rc != SQLITE_OK) fatal_sqlite3_err(rc, __LINE__);
		
		sqlite3_stmt *stmt_retrieve_total_logs_size;
		rc = sqlite3_prepare_v2(p_file_infos_arr->data[i]->db,
								"SELECT SUM(Msg_compr_size) FROM " LOGS_TABLE 
								" WHERE FK_BLOB_Id = ? GROUP BY FK_BLOB_Id ;",
								-1, &stmt_retrieve_total_logs_size, NULL);
		if (rc != SQLITE_OK) fatal_sqlite3_err(rc, __LINE__);
		
		uv_fs_t open_req;
		for(int id = 1; id <= BLOB_MAX_FILES; id++){
			/* Open BLOB file based on filename stored in BLOBS_TABLE. */
			rc = sqlite3_bind_int(stmt_retrieve_metadata_from_id, 1, id);
            if (rc != SQLITE_OK) fatal_sqlite3_err(rc, __LINE__);
			rc = sqlite3_step(stmt_retrieve_metadata_from_id);
			if (rc != SQLITE_ROW) fatal_sqlite3_err(rc, __LINE__);
			char *filename = m_malloc(snprintf(NULL, 0, "%s%s", 
				p_file_infos_arr->data[i]->db_dir, 
				sqlite3_column_text(stmt_retrieve_metadata_from_id, 0)) + 1);
			sprintf(filename, "%s%s", p_file_infos_arr->data[i]->db_dir, 
				sqlite3_column_text(stmt_retrieve_metadata_from_id, 0));
			rc = uv_fs_open(db_loop, &open_req, filename, 
				UV_FS_O_RDWR | UV_FS_O_CREAT | UV_FS_O_APPEND | UV_FS_O_RANDOM , 0644, NULL);
			if (unlikely(rc < 0)) fatal_libuv_err(rc, __LINE__);
			fprintf_log(LOGS_MANAG_DEBUG, stderr, "Opened file: %s\n", filename);
			p_file_infos_arr->data[i]->blob_handles[id] = open_req.result; 	// open_req.result of a uv_fs_t is the file descriptor in case of the uv_fs_open
			int64_t metadata_filesize = (int64_t) sqlite3_column_int64(stmt_retrieve_metadata_from_id, 1);
			
			/* Retrieve total log messages compressed size from LOGS_TABLE for current FK_BLOB_Id
			 * Only for asserting whether correct - not used elsewhere. If no rows are returned, it means
			 * it is probably the initial execution of the program so still valid (except if rc is other
			 * than SQLITE_DONE, which is an error then). */
			rc = sqlite3_bind_int(stmt_retrieve_total_logs_size, 1, id);
            if (rc != SQLITE_OK) fatal_sqlite3_err(rc, __LINE__);
			rc = sqlite3_step(stmt_retrieve_total_logs_size);
			if (rc == SQLITE_ROW){ 
				int64_t total_logs_filesize = (int64_t) sqlite3_column_int64(stmt_retrieve_total_logs_size, 0);
				m_assert(total_logs_filesize == metadata_filesize, "Metadata filesize != total logs filesize");
			}
			else if (unlikely(rc != SQLITE_DONE)) fatal_sqlite3_err(rc, __LINE__);
			
			/* Get filesize of BLOB file. */ 
			uv_fs_t stat_req;
			rc = uv_fs_stat(db_loop, &stat_req, filename, NULL);
			if (unlikely(rc)) fatal_libuv_err(rc, __LINE__);
			uv_stat_t *statbuf = uv_fs_get_statbuf(&stat_req);
			int64_t blob_filesize = (int64_t) statbuf->st_size;
			uv_fs_req_cleanup(&stat_req);
			
			do{
				/* Case 1: blob_filesize == metadata_filesize (equal, either both zero or not): All good */
				if(likely(blob_filesize == metadata_filesize))
					break;
				
				/* Case 2: blob_filesize == 0 && metadata_filesize > 0: fatal(), however could it mean that 
				 * EXT_BLOB_STORE_FILENAME was rotated but the SQLite metadata wasn't updated? So can it 
				 * maybe be recovered by un-rotating? Either way, treat as fatal() for now. */
				 // TODO: Can we avoid fatal()? 
				if(unlikely(blob_filesize == 0 && metadata_filesize > 0)){
					fprintf_log(LOGS_MANAG_ERROR, stderr, "blob_filesize == 0 but metadata_filesize > 0\n");
					fatal("blob_filesize == 0 but metadata_filesize > 0");
				}
				
				/* Case 3: blob_filesize > metadata_filesize: Truncate binary to sqlite filesize, program 
				 * crashed or terminated after writing BLOBs to external file but before metadata was updated */
				if(unlikely(blob_filesize > metadata_filesize)){
					fprintf_log(LOGS_MANAG_WARNING, stderr, "blob_filesize > metadata_filesize for '%s'. Will attempt to fix.\n", filename);
					uv_fs_t trunc_req;
				    rc = uv_fs_ftruncate(db_loop, &trunc_req, p_file_infos_arr->data[i]->blob_handles[id], 
				    	metadata_filesize, NULL);
				    if(unlikely(rc)) fatal_libuv_err(rc, __LINE__);
				    uv_fs_req_cleanup(&trunc_req);
				    break;
				}

				/* Case 4: blob_filesize < metadata_filesize: unrecoverable (and what-should-be impossible
				 * state), delete external binaries, clear metadata record and then fatal() */
				// TODO: Delete external BLOB and clear metadata from DB, start from clean state but the most recent logs.
		    	if(unlikely(blob_filesize < metadata_filesize)){
		    		fprintf_log(LOGS_MANAG_ERROR, stderr, "blob_filesize < metadata_filesize for '%s'. \n", filename);
		        	fatal("blob_filesize < metadata_filesize for '%s'. \n", filename);
		        }

			    /* Case 5: default if none of the above, should never reach here, fatal() */
				fatal("invalid case when comparing blob_filesize with metadata_filesize");
			} while(0);
			
			
			/* Initialise blob_write_handle with logs.bin.0 */
			if(filename[strlen(filename) - 1] == '0')
				p_file_infos_arr->data[i]->blob_write_handle_offset = id;
				
			m_free(filename);
			uv_fs_req_cleanup(&open_req);
			sqlite3_reset(stmt_retrieve_total_logs_size);
			sqlite3_reset(stmt_retrieve_metadata_from_id);
		}
		sqlite3_finalize(stmt_retrieve_metadata_from_id);
		
		/* Create synchronous writer thread, one for each log source */
		uv_thread_t *db_writer_thread = m_malloc(sizeof(uv_thread_t));
		rc = uv_thread_create(db_writer_thread, db_writer, &p_file_infos_arr->data[i]);
		if (unlikely(rc)) fatal_libuv_err(rc, __LINE__);
		
    }
    sqlite3_finalize(stmt_get_last_id);
    sqlite3_finalize(stmt_search_if_log_source_exists);
    sqlite3_finalize(stmt_insert_log_collection_metadata);
}

/**
 * @brief Search database
 * @details This function searches the database for any results matching the
 * query parameters. If any results are found, it will decompress the text
 * of each returned row and add it to the results buffer, up to a maximum
 * amount of bytes (defined in query_params->results_size). 
 * @todo Implement keyword search as well. For now, this only searches for timestamps.
 * @todo What happens in case SQLITE_CORRUPT error? See if it can be handled, for now just fatal().
 * @todo Change results buffer to be long-lived.
 */
void db_search(DB_query_params_t *query_params, struct File_info *p_file_info) {
    int rc = 0;
    size_t max_query_page_size = query_params->results_size;
    query_params->results_size = 0;
    Message_t temp_msg = {0};
    int64_t blob_offset = 0;
    int blob_handles_offset = 0;

    // Prepare "SELECT" statement used to retrieve log metadata in case of query
    // TODO: Avoid preparing statement for each db_search call, but how if tied to specific db handle?
    // TODO: Limit number of results returned through SQLite Query to speed up search?
    sqlite3_stmt *stmt_retrieve_log_msg_metadata;
    rc = sqlite3_prepare_v2(p_file_info->db,
    						"SELECT Timestamp, Msg_compr_size , Msg_decompr_size, BLOB_Offset, " BLOBS_TABLE".Id "
    						"FROM " LOGS_TABLE " INNER JOIN " BLOBS_TABLE 
    						" ON " LOGS_TABLE ".FK_BLOB_Id = " BLOBS_TABLE ".Id "
    						"WHERE Timestamp BETWEEN ? AND ?;",
                            -1, &stmt_retrieve_log_msg_metadata, NULL);
    if (unlikely(rc != SQLITE_OK)) fatal_sqlite3_err(rc, __LINE__);

    sqlite3_exec(p_file_info->db, "BEGIN TRANSACTION;", NULL, NULL, NULL);
    rc = sqlite3_bind_int64(stmt_retrieve_log_msg_metadata, 1, (sqlite3_int64)query_params->start_timestamp);
    if (rc != SQLITE_OK) fatal_sqlite3_err(rc, __LINE__);
    rc = sqlite3_bind_int64(stmt_retrieve_log_msg_metadata, 2, (sqlite3_int64)query_params->end_timestamp);
    if (rc != SQLITE_OK) fatal_sqlite3_err(rc, __LINE__);

    rc = sqlite3_step(stmt_retrieve_log_msg_metadata);
    fprintf_log(LOGS_MANAG_DEBUG, stderr, "Query: %s\n rc:%d\n", sqlite3_expanded_sql(stmt_retrieve_log_msg_metadata), rc);
    if (rc != SQLITE_ROW && rc != SQLITE_DONE) fatal_sqlite3_err(rc, __LINE__);

    while (rc == SQLITE_ROW) {
    	/* Retrieve metadata from DB */
        temp_msg.timestamp = (uint64_t)sqlite3_column_int64(stmt_retrieve_log_msg_metadata, 0);
        fprintf_log(LOGS_MANAG_DEBUG, stderr, "Timestamp retrieved: %" PRIu64 "\n", (uint64_t)temp_msg.timestamp);
        temp_msg.text_compressed_size = (size_t)sqlite3_column_int64(stmt_retrieve_log_msg_metadata, 1);
        temp_msg.text_size = (size_t)sqlite3_column_int64(stmt_retrieve_log_msg_metadata, 2);
        fprintf_log(LOGS_MANAG_DEBUG, stderr, "Size of results from query: %zu\n", temp_msg.text_size);
        blob_offset = (int64_t) sqlite3_column_int64(stmt_retrieve_log_msg_metadata, 3);
        blob_handles_offset = sqlite3_column_int(stmt_retrieve_log_msg_metadata, 4);

        /* Retrieve compressed log messages from BLOB file */
        temp_msg.text_compressed = m_malloc(temp_msg.text_compressed_size);
        uv_buf_t uv_buf = uv_buf_init(temp_msg.text_compressed, temp_msg.text_compressed_size);
	    uv_fs_t read_req;
	    // TODO: Using db_loop here in separate thread (although synchronously) - thread-safe ?
	    rc = uv_fs_read(db_loop, &read_req, p_file_info->blob_handles[blob_handles_offset], &uv_buf, 1, blob_offset, NULL);
        if (rc < 0) fatal_libuv_err(rc, __LINE__);
	    uv_fs_req_cleanup(&read_req);

        size_t query_params_results_size_new = query_params->results_size + temp_msg.text_size;

        /* If adding the current temp_msg text to the results buffer would exceed the max_query_page_size,
		 * save the timestamp to continue from in the next call of the querying API and stop the current
		 * execution (providing that query_params->results_size != 0 i.e. we have retrieved at least 1 row of results). */
        if (query_params->results_size && query_params_results_size_new > max_query_page_size) {
            query_params->start_timestamp = temp_msg.timestamp;  // Save the timestamp of where to continue from next time!
            fprintf_log(LOGS_MANAG_DEBUG, stderr, "New timestamp to 'cont' from: %" PRId64 "\n", 
                    (int64_t)query_params->start_timestamp);
            break;
        } else {
            // If need be, grow the results buffer.
            query_params->results = m_realloc(query_params->results, query_params_results_size_new);
            decompress_text(&temp_msg, &query_params->results[query_params->results_size]);
            query_params->results_size = query_params_results_size_new - 1;  // -1 due to terminating NUL char

            fprintf_log(LOGS_MANAG_DEBUG, stderr, "Timestamp decompressed: %" PRIu64 "\n", (uint64_t)temp_msg.timestamp);
            m_free(temp_msg.text_compressed);
        }

        rc = sqlite3_step(stmt_retrieve_log_msg_metadata);
        fprintf_log(LOGS_MANAG_DEBUG, stderr, "Query: %s\n rc:%d\n", sqlite3_expanded_sql(stmt_retrieve_log_msg_metadata), rc);
        if (rc != SQLITE_ROW && rc != SQLITE_DONE) fatal_sqlite3_err(rc, __LINE__);
    }

    if (temp_msg.timestamp && rc == SQLITE_DONE) {
        query_params->start_timestamp = temp_msg.timestamp + 1;  // Save the timestamp of where to continue from next time!
    }

    sqlite3_reset(stmt_retrieve_log_msg_metadata);
    sqlite3_exec(p_file_info->db, "END TRANSACTION;", NULL, NULL, NULL);
}