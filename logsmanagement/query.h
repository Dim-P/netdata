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
 * @param[in] start_timestamp Start timestamp of query in epoch milliseconds.
 * @param[in,out] end_timestamp End timestamp of query in epoch milliseconds. It returns the actual end timestamp of the query which may not match the
                                end timestamp passed as input, in case there are more results to be retrieved that would exceed the desired max quota.
 * @param[in] chart_name Chart name of log source to be queried, as it appears on the netdata dashboard. 
 *                       If this is defined and not an empty string, the filename parameter is ignored.
 * @param[in] filename Full path of log source to be queried. Will only be used if the chart_name is not used.
 * @param[in] keyword The keyword to be searched. IMPORTANT! Regular expressions are supported but have not been tested extensively, so their use should be avoided for now!
 * @param[in] ignore_case If set to any integer other than 0, the query will be case-insensitive. If not set or if set to 0, the query will be case-sensitive.
 * @param[in,out] results_buff Buffer of BUFFER type to store the results of the query in. 
                               results_buff->size Defines the maximum quota of results to be expected. If exceeded, the query will return the results obtained so far.
                               results_buff->len The exact size of the results matched. 
                               results_buff->buffer String containing the results of the query.
 */
typedef struct logs_query_params {
    uint64_t start_timestamp;
    uint64_t end_timestamp;
    char *chart_name;
    char *filename;
    char *keyword;
    int ignore_case;
    BUFFER *results_buff;
} logs_query_params_t;

/** 
 * @brief Primary query API. See documentation of logs_query_params_t on how to use argument.
 * @return -1 for generic errors, -2 if chart name or filename not found, -3 if query returns no results, 0 if successful. 
 * @todo Implement keyword search (currently only search by timestamps is supported).
 * @todo Cornercase if filename not found in DB? Return specific message?
 * @todo This function is allowed to be executed even before logsmanagement_main() is initialised, which leads to a segmentation fault...
 */
int execute_query(logs_query_params_t *p_query_params);

#endif  // QUERY_H_
