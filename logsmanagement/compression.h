/** @file compression.h
 *  @brief Header of compression.c 
 *
 *  @author Dimitris Pantazis
 */

#ifndef COMPRESSION_H_
#define COMPRESSION_H_

#include <lz4frame.h>
#include "circular_buffer.h"
#include "config.h"

static const LZ4F_preferences_t kPrefs = {
    {LZ4F_max64KB, LZ4F_blockLinked, LZ4F_noContentChecksum, LZ4F_frame,
     1 /* set content size flag */, 0 /* no dictID */, LZ4F_noBlockChecksum},
    0,         /* compression level; 0 == default */
    0,         /* autoflush */
    1,         /* favor decompression speed */
    {0, 0, 0}, /* reserved, must be set to 0 */
};

void compress_text(Message_t *msg);
void decompress_text(Message_t *msg, char* out_buf);

#endif  // COMPRESSION_H_
