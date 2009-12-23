
#include "htp.h"
#include "htp_decompressors.h"

/**
 *
 */
static int htp_gzip_decompressor_decompress(htp_decompressor_gzip_t *drec, htp_tx_data_t *d) {
    size_t consumed = 0;   

    // Return if we've previously had an error
    if (drec->initialized < 0) {
        return 0;
    }

    // Do we need to initialize?
    if (drec->initialized == 0) {
        // Check the header
        if ((drec->header_len == 0) && (d->len >= 10)) {
            // We have received enough data initialize; use the input buffer directly
            if ((d->data[0] != DEFLATE_MAGIC_1) || (d->data[1] != DEFLATE_MAGIC_2)) {
                printf("## No deflate magic bytes (1)!\n");
                drec->initialized = -1;
                return -1;
            }

            if (d->data[3] != 0) {
                printf("## Unable to handle flags!\n");
                drec->initialized = -1;
                return -1;
            }

            drec->initialized = 1;
            consumed = 10;
        } else {
            // We do not (or did not) have enough bytes, so we have
            // to copy some data into our internal header buffer.

            // How many bytes do we need?
            int copylen = 10 - drec->header_len;

            // Is there enough in input?
            if (copylen > d->len) copylen = d->len;

            // Copy the bytes
            memcpy(drec->header + drec->header_len, d->data, copylen);
            drec->header_len += copylen;
            consumed = copylen;

            // Do we have enough now?
            if (drec->header_len == 10) {
                // We do!
                if ((drec->header[0] != DEFLATE_MAGIC_1) || (drec->header[1] != DEFLATE_MAGIC_2)) {
                    printf("## No deflate magic bytes (2)!\n");
                    drec->initialized = -1;
                    return -1;
                }

                if (drec->header[3] != 0) {
                    printf("## Unable to handle flags!\n");
                    drec->initialized = -1;
                    return -1;
                }

                drec->initialized = 1;
            } else {
                // Need more data
                return 1;
            }
        }
    }

    // Decompress data
    int rc = 0;
    drec->stream.next_in = d->data + consumed;
    drec->stream.avail_in = d->len - consumed;

    while (drec->stream.avail_in != 0) {
        // If there's no more data left in the
        // buffer, send that information out
        if (drec->stream.avail_out == 0) {
            drec->crc = crc32(drec->crc, drec->buffer, GZIP_BUF_SIZE);           

            // Execute callback with available data
            htp_tx_data_t d2;
            d2.tx = d->tx;
            d2.data = drec->buffer;
            d2.len = GZIP_BUF_SIZE;
            // XXX Abort on error
            drec->super.callback(&d2);

            drec->stream.next_out = drec->buffer;
            drec->stream.avail_out = GZIP_BUF_SIZE;
        }

        rc = inflate(&drec->stream, Z_NO_FLUSH);

        if (rc == Z_STREAM_END) {
            // How many bytes do we have?
            size_t len = GZIP_BUF_SIZE - drec->stream.avail_out;

            // Update CRC
            drec->crc = crc32(drec->crc, drec->buffer, len);           

            // Execute callback with available data
            htp_tx_data_t d2;
            d2.tx = d->tx;
            d2.data = drec->buffer;
            d2.len = len;
            // XXX Abort on error
            drec->super.callback(&d2);

            // TODO Handle trailer

            // printf("## Zlib: Inflated %ld to %ld\n", drec->stream.total_in, drec->stream.total_out);

            return 1;
        }

        if (rc != Z_OK) {
            printf("## inflate failed: %d\n", rc);

            inflateEnd(&drec->stream);
            drec->zlib_initialized = 0;
            
            return -1;
        }
    }

    return 1;
}

/**s
 *
 */
static void htp_gzip_decompressor_destroy(htp_decompressor_gzip_t * drec) {
    if (drec == NULL) return;

    // TODO End inflate
    if (drec->zlib_initialized) {
        inflateEnd(&drec->stream);
        drec->zlib_initialized = 0;
    }

    free(drec->buffer);
    free(drec);
}

/**
 *
 */
htp_decompressor_t * htp_gzip_decompressor_create() {
    htp_decompressor_gzip_t *drec = calloc(1, sizeof (htp_decompressor_gzip_t));
    if (drec == NULL) return NULL;

    drec->super.decompress = (int (*)(htp_decompressor_t *, htp_tx_data_t *)) htp_gzip_decompressor_decompress;
    drec->super.destroy = (void (*)(htp_decompressor_t *))htp_gzip_decompressor_destroy;

    drec->buffer = malloc(GZIP_BUF_SIZE);
    if (drec->buffer == NULL) {
        free(drec);
        return NULL;
    }

    int rc = inflateInit2(&drec->stream, GZIP_WINDOW_SIZE);
    if (rc != Z_OK) {
        // TODO Message
        printf("## Failed inflateInit2: %d", rc);
        inflateEnd(&drec->stream);
        free(drec->buffer);
        free(drec);
        return NULL;
    }

    drec->zlib_initialized = 1;
    drec->stream.avail_out = GZIP_BUF_SIZE;
    drec->stream.next_out = drec->buffer;

    return (htp_decompressor_t *) drec;
}
