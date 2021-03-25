/** @file circular_buffer.h
 *  @brief Header of circular_buffer.c 
 *
 *  @author Dimitris Pantazis
 */

#ifndef CIRCULAR_BUFFER_H_
#define CIRCULAR_BUFFER_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <uv.h>
#include "config_.h"
#include "query.h"
#include "file_info.h"

#define BUFF_SIZE_MASK (CIRCULAR_BUFF_SIZE - 1U)

/** 
 * @struct Message
 * @brief Structure representing a single log message.
 * @details This can be a multi-line text which is stored exactly
 * as it was read from the log file. 
 */
typedef struct Message {
    uint64_t timestamp;              /**< Unix timestamp in milliseconds. */
    uint8_t db_fileInfos_Id;         /**< ID of File_info that the current message is associated to in the respective metadata table in DB. */
    char *text;                      /**< Uncompressed text of the message */
    size_t text_size;                /**< Size of #text string */
    size_t text_size_max;            /**< Size of #text buffer (never reducing, always growing) */
    void *text_compressed;           /**< Compressed text of the message */
    size_t text_compressed_size;     /**< Size of #text_compressed */
    size_t text_compressed_size_max; /**< Size of #text_compressed buffer (never reducing, always growing */
} Message_t;

typedef struct Circ_buff {
    Message_t msgs[CIRCULAR_BUFF_SIZE];
    uint8_t head_index;                 /**< Index pointing at one item after the last inserted msg */
    uint8_t tail_index;                 /**< Index pointing at the oldest valid msg in the buffer */
    uint8_t read_index;                 /**< Index pointing at one item after the last read msg */
    uint8_t parsed_index;               /**< Index pointing at one item after the last parsed (i.e. ready to be read) msg */
    uint8_t size;
    uv_mutex_t mut;
} Circ_buff_t;

void circ_buff_write(struct File_info *p_file_info);
Message_t *circ_buff_read(Circ_buff_t *buff);
void circ_buff_search(Circ_buff_t *buff, logs_query_params_t *query_params, size_t max_query_page_size);
uint8_t circ_buff_get_size(Circ_buff_t *buff);
Circ_buff_t *circ_buff_init();

#endif  // CIRCULAR_BUFFER_H_
