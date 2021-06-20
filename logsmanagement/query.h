/** @file query.h
 *  @brief Header of query.c 
 *
 *  @author Dimitris Pantazis
 */

#ifndef QUERY_H_
#define QUERY_H_

#include <inttypes.h>
#include <stdlib.h>
#include "../libnetdata/libnetdata.h"

/**
 * @brief Parameters of the query.
 * @param start_timestamp Start timestamp of query in milliseconds.
 * @param end_timestamp End timestamp of query in milliseconds.
 */
typedef struct logs_query_params {
    uint64_t start_timestamp;
    uint64_t end_timestamp;
    char *chart_name;
    char *filename;
    char *keyword;
    BUFFER *results_buff;
} logs_query_params_t;

/** 
 * @brief Primary query API. 
 * @return -1 if chart name not found, -2 if query returns no results, 0 if successful. 
 * @todo Implement keyword search (currently only search by timestamps is supported).
 * @todo Cornercase if filename not found in DB? Return specific message?
 */
int execute_query(logs_query_params_t *p_query_params);

#endif  // QUERY_H_
