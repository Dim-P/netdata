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
 */
typedef struct logs_query_params {
    uint64_t start_timestamp;
    uint64_t end_timestamp;
    char *filename;
    char *keyword;
    BUFFER *results_buff;
} logs_query_params_t;

/** 
 * @brief Primary query API. 
 * @todo Implement keyword search (currently only search by timestamps is supported).
 * @todo Cornercase if filename not found in DB? Return specific message?
 */
int execute_query(logs_query_params_t *p_query_params);

#endif  // QUERY_H_
