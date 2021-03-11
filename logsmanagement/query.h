/** @file query.h
 *  @brief Header of query.c 
 *
 *  @author Dimitris Pantazis
 */

#ifndef QUERY_H_
#define QUERY_H_

#include <inttypes.h>
#include <stdlib.h>

/**
 * @brief Parameters of the query.
 */
typedef struct db_query_params {
    uint64_t start_timestamp;
    uint64_t end_timestamp;
    char *filename;
    char *keyword;
    char *results;
    size_t results_size;
} DB_query_params_t;

/** 
 * @brief Primary query API. 
 * @details This is the public API to perform the queries. It will search 
 * all the collected messages according to the given parameters and return 
 * any matching results into the returned character buffer. 
 * If there are no more results to return, *p_size will be 0 and the returned buffer
 * will be NULL. So, this function is intended to be called repeatedly until there
 * are no further results returned for the given timestamp range. 
 * @param[in] start_timestamp The lower boundary of the timestamp range to be searched. 
 * @param[in] end_timestamp The upper boundary of the timestamp range to be searched
 * @param[in] filename The full path of the log file to be searched.
 * @param[in] keyword The keyword to be searched for. (TODO)
 * @param[in,out] p_size Pointer to a variable indicating the maximum desirable size of the results page.
 * The actual number of bytes returned will be stored in this variable once the function returns.
 * @return Point to char buffer that contains the results.
 * @todo Implement keyword search (currently only search by timestamps is supported).
 * @todo Cornercase if filename not found in DB? Return specific message?
 */
char *execute_query(uint64_t start_timestamp, uint64_t end_timestamp, const char *filename, const char *keyword, size_t *p_size);

#endif  // QUERY_H_
