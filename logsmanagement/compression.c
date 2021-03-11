/** @file compression.c
 *  @brief This is the file containing the implementation of the compression and decompression logic.
 *
 *  @author Dimitris Pantazis
 */

#include "compression.h"
#include "helper.h"

/**
 * @brief Compress text
 * @details This function will compress the text stored in msg. The results will be stored
 * inside the Message_t msg struct as well.
 * @param[in,out] msg Message_t struct that holds the original text, its size, the 
 * compressed results and their size too.
 */
void compress_text(Message_t *msg) {
    uint64_t end_time;
    const uint64_t start_time = get_unix_time_ms();

    size_t const outbufCapacity = LZ4F_compressFrameBound(msg->text_size, &kPrefs);
    fprintf_log(DEBUG, stderr, "Max outbufCapacity: %zuB\n", outbufCapacity);
    if (!msg->text_compressed || outbufCapacity > msg->text_compressed_size_max) {
        msg->text_compressed_size_max = outbufCapacity * BUFF_SCALE_FACTOR;
        msg->text_compressed = m_realloc(msg->text_compressed, msg->text_compressed_size_max);
        fprintf_log(DEBUG, stderr, "Realloc'ing circ buff compressed item to: %zuB\n", msg->text_compressed_size_max);
    }

    msg->text_compressed_size = LZ4F_compressFrame(msg->text_compressed, outbufCapacity,
                                                   msg->text, msg->text_size,
                                                   &kPrefs);

    end_time = get_unix_time_ms();
    fprintf_log(INFO, stderr, "Original size: %zuB Compressed size: %zuB Ratio: x%zu\n",
                msg->text_size, msg->text_compressed_size, msg->text_size / msg->text_compressed_size);
    fprintf_log(INFO, stderr, "It took %" PRIu64 "ms to compress.\n", end_time - start_time);
}

#if 0 
static size_t get_block_size(const LZ4F_frameInfo_t* info) {
    switch (info->blockSizeID) {
        case LZ4F_default:
        case LZ4F_max64KB:  return 1 << 16;
        case LZ4F_max256KB: return 1 << 18;
        case LZ4F_max1MB:   return 1 << 20;
        case LZ4F_max4MB:   return 1 << 22;
        default:
            printf("Impossible with expected frame specification (<=v1.6.1)\n");
            exit(1);
    }
}
#endif

/**
 * @brief Decompress compressed text
 * @details This function will decompress the compressed text stored in msg. If out_buf is NULL,
 * the results of the decompression will be stored inside the Message_t msg struct. Otherwise,
 * the results will be stored in out_buf (which must be large enough to store them).
 * @param[in,out] msg Message_t struct that stores the compressed text, its size and the 
 * decompressed results as well, in case out_buf is NULL.
 * @param[in,out] out_buf Buffer to store the results of the decompression. In case it is NULL,
 * the results will be stored back in the msg struct. The out_buf must be pre-allocated and
 * large enough (equal to msg->text_size at least).
 */
void decompress_text(Message_t *msg, char* out_buf) {
    const uint64_t start_time = get_unix_time_ms();

    // Create decompression context
    LZ4F_dctx *dctx;
    size_t dctxStatus = LZ4F_createDecompressionContext(&dctx, LZ4F_VERSION);
    if (LZ4F_isError(dctxStatus)) {
        fprintf_log(ERROR, stderr, "LZ4F_dctx creation error: %s\n", LZ4F_getErrorName(dctxStatus));
    }

    // Retrieve Frame header
    LZ4F_frameInfo_t info;
    size_t consumedSize = LZ4F_HEADER_SIZE_MAX;  // Ensure that header will be read
    size_t const fires = LZ4F_getFrameInfo(dctx, &info, msg->text_compressed, &consumedSize);
    if (LZ4F_isError(fires)) {
        fprintf_log(ERROR, stderr, "LZ4F_getFrameInfo error: %s\n", LZ4F_getErrorName(fires));
    }
    fprintf_log(DEBUG, stderr, "Header size: %zu\n", consumedSize);
    //fprintf(stderr, "msg->text_size: %zu, info.contentSize: %llu\n", msg->text_size, info.contentSize);
    m_assert(msg->text_size == info.contentSize, "msg->text_size should be equal to info.contentSize");

    // Decompress
    msg->text_size = (size_t) info.contentSize;  // Expected size of decompressed text
    if(out_buf == NULL)
        msg->text = m_malloc(msg->text_size);        // Destination buffer
    size_t srcSize = msg->text_compressed_size;  // text_compressed_size needs to be known!
    fprintf_log(DEBUG, stderr, "sizeof decompressed content (msg->text_size): %zu\n", msg->text_size);
    fprintf_log(DEBUG, stderr, "sizeof msg->text_compressed (bytes to decompress): %zu\n", srcSize);

    size_t ret = 1;
    size_t decompressedSize = 0;
    size_t dstSize = msg->text_size;
    char **dstBuffer;
    if(out_buf == NULL)
        dstBuffer = &msg->text;
    else
        dstBuffer = &out_buf;
    int i = 0;
    while (ret != 0) {
        fprintf_log(DEBUG, stderr, "Iter: %d\n", i++);
        ret = LZ4F_decompress(dctx, *dstBuffer + decompressedSize, &dstSize,
                              (char *)msg->text_compressed + consumedSize,  // Cast to (char *) as (void *) arithmetic is illegal.
                              &srcSize, /* LZ4F_decompressOptions_t */ NULL);
        if (LZ4F_isError(ret)) 
            fprintf_log(ERROR, stderr, "Decompression error: %s\n", LZ4F_getErrorName(ret));

        fprintf_log(DEBUG, stderr, "Number of decompressed bytes (dstSize): %zu\n", dstSize);
        fprintf_log(DEBUG, stderr, "Number of consumed bytes (srcSize): %zu\n", srcSize);
        fprintf_log(DEBUG, stderr, "ret of LZ4F_decompress: %zu\n", ret);

        consumedSize += srcSize;
        decompressedSize += dstSize;
        dstSize = info.contentSize - decompressedSize;
        m_assert(i < 2, "There should only be 1 execution of decompression loop");
    }

    // Free decompression context
    dctxStatus = LZ4F_freeDecompressionContext(dctx); /* note : free works on NULL */
    if (LZ4F_isError(dctxStatus)) {
        fprintf_log(ERROR, stderr, "LZ4F_dctx creation error: %s\n", LZ4F_getErrorName(dctxStatus));
    }

    const uint64_t end_time = get_unix_time_ms();
    fprintf_log(DEBUG, stderr, "It took %" PRId64 "ms to decompress.\n", end_time - start_time);
}
