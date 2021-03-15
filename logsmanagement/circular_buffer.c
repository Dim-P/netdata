/** @file circular_buffer.c
 *  @brief This is an implementation of a circular buffer to be user for temporarily 
 *         storing the logs before they are inserted into the database.
 *
 *  @author Dimitris Pantazis
 */

#include "circular_buffer.h"
#include "compression.h"
#include "helper.h"

static uv_loop_t *circ_buff_loop = NULL;

/**
 * @brief Cleans up after an execution of the msg_parser function. 
 */
static void msg_parser_cleanup(uv_work_t *req, int status){
    freez(req);
}

/**
 * @brief Performs all the processing required for a raw log message
 * @details For the time being, this function compresses a raw log message. In the
 * future, additional processing (such as parsing and/or streaming) will be performed.
 */
static void msg_parser(uv_work_t *req){
    Circ_buff_t *buff = (Circ_buff_t *) req->data;

    uv_mutex_lock(&buff->mut);
    m_assert(buff->parsed_index != buff->head_index, 
        "More msg_parser() called than Message_t items in buff that need parsing");

    /* This needs to be within mut lock as another msg_parser() in different thread 
     * (albeit unlikely) could be changing parsed_index at the same time as this thread */
    Message_t *buff_msg_current = &buff->msgs[(buff->parsed_index) & BUFF_SIZE_MASK];  
    uv_mutex_unlock(&buff->mut);

    compress_text(buff_msg_current);
#if VALIDATE_COMPRESSION
    Message_t *temp_msg = mallocz(sizeof(Message_t));
    temp_msg->text_compressed_size = buff_msg_current->text_compressed_size;
    temp_msg->text_compressed = mallocz(temp_msg->text_compressed_size);
    memcpy(temp_msg->text_compressed, buff_msg_current->text_compressed, buff_msg_current->text_compressed_size);
    decompress_text(temp_msg);
    int cmp_res = memcmp(buff_msg_current->text, temp_msg->text, buff_msg_current->text_size);
    m_assert(!cmp_res, "Decompressed text != compressed text!");
    freez(temp_msg->text);
    freez(temp_msg->text_compressed);
    freez(temp_msg);
#endif  // VALIDATE_COMPRESSION

    uv_mutex_lock(&buff->mut);
    buff->parsed_index++;
    uv_mutex_unlock(&buff->mut);
}

/**
 * @brief Insert Message_t type items into the circular buffer of the file struct.
 * @details If the circular buffer is full, no new data will be inserted until there 
 * is enough space again.
 * If COMPRESSION_ENABLED is true, this function will also synchronously compress the text
 * of the Message_t inserted into the buffer.
 * If VALIDATE_COMPRESSION is true, the compressed text will be validated by decompressing
 * it and comparing it against the original text.
 * The individual item buffers will grow as required (times BUFF_SCALE_FACTOR and remain at maximum size. 
 * @param p_file_info The file info struct containing the circular buffer and the text to 
 * be imported into it.
 */
void circ_buff_write(struct File_info *p_file_info) {
    uint64_t end_time;
    const uint64_t start_time = get_unix_time_ms();

    Circ_buff_t *buff = p_file_info->msg_buff;

    uv_mutex_lock(&buff->mut);
    if (((buff->head_index + 1) & BUFF_SIZE_MASK) == (buff->tail_index & BUFF_SIZE_MASK)) {
        uv_mutex_unlock(&buff->mut);
        fprintf_log(LOGS_MANAG_WARNING, stderr, "Buffer out of space! Losing data!\n");
        return;
    }
    uv_mutex_unlock(&buff->mut);

    Message_t *buff_msg_current = &buff->msgs[(buff->head_index) & BUFF_SIZE_MASK];  // No need to lock mut for reading head_index - only circ_buff_write can change it

    char *temp_text = buff_msg_current->text;
    size_t temp_size_max = buff_msg_current->text_size_max;

    buff_msg_current->db_fileInfos_Id = p_file_info->db_fileInfos_Id;
    buff_msg_current->text = p_file_info->buff;
    buff_msg_current->text_size = p_file_info->buff_size;
    buff_msg_current->text_size_max = p_file_info->buff_size_max;
    buff_msg_current->timestamp = get_unix_time_ms();

    p_file_info->buff = temp_text;
    p_file_info->buff_size = 0;
    p_file_info->buff_size_max = temp_size_max;

    fprintf_log(LOGS_MANAG_DEBUG, stderr, "timestamp of msg to write in buff: %" PRIu64 "\n",
                buff_msg_current->timestamp);
    fprintf_log(LOGS_MANAG_DEBUG, stderr, "buff_msg->text_size: %zuB\n", buff_msg_current->text_size);
    fprintf_log(LOGS_MANAG_DEBUG, stderr, "buff_msg->text_size_max: %zuB\n", buff_msg_current->text_size_max);

    p_file_info->filesize += buff_msg_current->text_size - 1;

    uv_mutex_lock(&buff->mut);
    buff->head_index++;
    buff->size++;
    uv_mutex_unlock(&buff->mut);

    // TODO: Can we get rid of malloc here?
    uv_work_t *req = mallocz(sizeof(uv_work_t));
    req->data = (void *) buff; 
    uv_queue_work(circ_buff_loop, req, msg_parser, msg_parser_cleanup);

    end_time = get_unix_time_ms();
    fprintf_log(LOGS_MANAG_INFO, stderr, "It took %" PRIu64 "ms to insert message into buffer.\n", end_time - start_time);
}

/**
 * @brief Read items from the circular buffer.
 * @details This function will return a pointer to the next item in the circular buffer
 * each time it is called, until the read_index matches the parsed_index i.e. all the 
 * compressed (and parsed) items in the buffer have been read. Only then the tail_index 
 * will be updated to indicate that all the read items space can be reused. Otherwise, 
 * the items between the tail_index and the read_index cannot be written.
 * @param buff The circular buffer to read items from.
 * @return Pointer to the Message_t type item of the circular buffer to be read.
 * */
Message_t *circ_buff_read(Circ_buff_t *buff) {
    uv_mutex_lock(&buff->mut);
    if ((buff->parsed_index & BUFF_SIZE_MASK) == (buff->read_index & BUFF_SIZE_MASK)) {
        buff->tail_index = buff->read_index;
        uv_mutex_unlock(&buff->mut);
        fprintf_log(LOGS_MANAG_DEBUG, stderr, "No more items to read from circular buffer!\n");
        return NULL;
    }
    Message_t *p_msg = &buff->msgs[(buff->read_index++) & BUFF_SIZE_MASK];
    // TODO: buff->size should be updated when tail_index is moved, not when read_index is moved.
    buff->size--;
    uv_mutex_unlock(&buff->mut);
    return p_msg;
};

/**
 * @brief Search circular buffer according to the query_params.
 * @details Currently, the buffer can only be searched according to 
 * the timestamp range of the query parameters and not the keyword.
 * @warning It is not required to acquire buff->mut when reading tail_index, 
 * because it can only be changed through circ_buff_read() and this function
 * i.e. circ_buff_search() and circ_buff_read() are mutually exclusive due 
 * to db_set_lock() and db_release_lock() in queries and when writing to DB.
 * @param buff Buffer to be searched
 * @param query_params Query parameters to search according to.
 */
void circ_buff_search(Circ_buff_t *buff, DB_query_params_t *query_params) {
    uv_mutex_lock(&buff->mut);
    uint8_t head_index_tmp = (buff->head_index & BUFF_SIZE_MASK);
    uv_mutex_unlock(&buff->mut);

    if (head_index_tmp == (buff->tail_index & BUFF_SIZE_MASK)) {
        fprintf_log(LOGS_MANAG_INFO, stderr, "Circ buff empty! Won't be searched.\n");
        return;  // Nothing to do if buff is emtpy
    }

    for (uint8_t i = (buff->tail_index & BUFF_SIZE_MASK);
         i != head_index_tmp; i = (i + 1) % CIRCULAR_BUFF_SIZE) {
        fprintf_log(LOGS_MANAG_DEBUG, stderr, "tail:%d head:%d i:%d\n",
                    buff->tail_index & BUFF_SIZE_MASK, head_index_tmp, i);
        if (buff->msgs[i].timestamp >= query_params->start_timestamp && buff->msgs[i].timestamp < query_params->end_timestamp) {
            fprintf_log(LOGS_MANAG_DEBUG, stderr, "Found text in circ buffer with timestamp: %" PRIu64 "\n",
                        buff->msgs[i].timestamp);
            size_t query_params_results_size_new = query_params->results_size + buff->msgs[i].text_size;
            query_params->results = reallocz(query_params->results, query_params_results_size_new);
            fprintf_log(LOGS_MANAG_DEBUG, stdout, "Text to add: %s\n", buff->msgs[i].text);
            memcpy(&query_params->results[query_params->results_size],
                   buff->msgs[i].text, buff->msgs[i].text_size);
            query_params->results_size = query_params_results_size_new - 1;
        }
    }

    query_params->start_timestamp = buff->msgs[head_index_tmp - 1].timestamp + 1;
}

uint8_t circ_buff_get_size(Circ_buff_t *buff) {
    uv_mutex_lock(&buff->mut);
    uint8_t size = buff->size;
    uv_mutex_unlock(&buff->mut);
    return size;
}

/**
 * @brief Process the events of the uv_loop_t related to the circular buffer
 */
static void circ_buff_loop_run(void *arg){
    uv_run(circ_buff_loop, UV_RUN_DEFAULT);
}

/**
 * @brief Create and initialise a new Circ_buff_t circular buffer
 * @ The first time that the function is executed, it will also create the event loop
 * to by used by any circular buffer functions (as and when required). 
 * @return Pointer to the initialised circular buffer
 */ 
Circ_buff_t *circ_buff_init() {
    int rc = 0;
    Circ_buff_t *buff = mallocz(sizeof(Circ_buff_t));
    *buff = (Circ_buff_t){0};
    buff->tail_index = buff->read_index = buff->parsed_index = buff->head_index = 0;
    buff->size = 0;
    rc = uv_mutex_init(&buff->mut);
    if (unlikely(rc)){
        fprintf_log(LOGS_MANAG_ERROR, stderr, "uv_mutex_init() error: (%d) %s\n", rc, uv_strerror(rc));
        fatal("uv_mutex_init() error: (%d) %s\n", rc, uv_strerror(rc));
    }
    if(!circ_buff_loop){
        circ_buff_loop = mallocz(sizeof(uv_loop_t));
        rc = uv_loop_init(circ_buff_loop);
        if (unlikely(rc)) fatal("uv_loop_init() error");
        uv_thread_t *circ_buff_loop_run_thread = mallocz(sizeof(uv_thread_t));
        rc = uv_thread_create(circ_buff_loop_run_thread, circ_buff_loop_run, NULL);
        if (unlikely(rc)) fatal("uv_thread_create() error");
    }
    return buff;
}
