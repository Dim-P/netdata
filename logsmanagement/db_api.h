/** @file db_api.h
 *  @brief Header of db_api.c
 *
 *  @author Dimitris Pantazis
 */

#ifndef DB_API_H_
#define DB_API_H_

#include "sqlite3.h"
#include <uv.h>
#include "query.h"
#include "file_info.h"	

void db_set_lock(uv_mutex_t *db_mut);
void db_release_lock(uv_mutex_t *db_mut);
char *db_get_sqlite_version(void);
void db_init(void);
void db_search(logs_query_params_t *query_params, struct File_info *p_file_info, size_t max_query_page_size);

#endif  // DB_API_H_
