#ifndef SCELBI_LOADER_H
#define SCELBI_LOADER_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef size_t (*scelbi_loader_read_fn)(void *context,
                                        uint8_t *destination,
                                        size_t capacity);

typedef enum scelbi_load_status {
    SCELBI_LOAD_OK = 0,
    SCELBI_LOAD_INVALID_ARGUMENT,
    SCELBI_LOAD_BASE_OUT_OF_RANGE,
    SCELBI_LOAD_IMAGE_TOO_LARGE,
    SCELBI_LOAD_STAGING_TOO_SMALL,
    SCELBI_LOAD_SHORT_READ,
    SCELBI_LOAD_READ_ERROR
} scelbi_load_status;

/*
 * Read exactly image_size bytes into staging before modifying RAM. A reader
 * may return fewer bytes than requested; it is called again until the image
 * is complete or it returns zero. On every error, ram[] remains unchanged.
 *
 * The caller owns all buffers. staging_capacity and image_size are in bytes.
 */
scelbi_load_status scelbi_load_transactional(
    uint8_t *ram,
    size_t ram_size,
    size_t base,
    size_t image_size,
    uint8_t *staging,
    size_t staging_capacity,
    scelbi_loader_read_fn read_fn,
    void *read_context);

const char *scelbi_load_status_string(scelbi_load_status status);

#ifdef __cplusplus
}
#endif

#endif /* SCELBI_LOADER_H */
