#include "scelbi_loader.h"

#include <string.h>

scelbi_load_status scelbi_load_transactional(
    uint8_t *ram,
    size_t ram_size,
    size_t base,
    size_t image_size,
    uint8_t *staging,
    size_t staging_capacity,
    scelbi_loader_read_fn read_fn,
    void *read_context)
{
    size_t total_read = 0;

    if (ram == NULL || ram_size == 0U) {
        return SCELBI_LOAD_INVALID_ARGUMENT;
    }
    if (base >= ram_size) {
        return SCELBI_LOAD_BASE_OUT_OF_RANGE;
    }
    if (image_size > ram_size - base) {
        return SCELBI_LOAD_IMAGE_TOO_LARGE;
    }
    if (image_size == 0U) {
        return SCELBI_LOAD_OK;
    }
    if (staging == NULL || read_fn == NULL) {
        return SCELBI_LOAD_INVALID_ARGUMENT;
    }
    if (image_size > staging_capacity) {
        return SCELBI_LOAD_STAGING_TOO_SMALL;
    }

    while (total_read < image_size) {
        const size_t remaining = image_size - total_read;
        const size_t chunk = read_fn(read_context,
                                     staging + total_read,
                                     remaining);
        if (chunk > remaining) {
            return SCELBI_LOAD_READ_ERROR;
        }
        if (chunk == 0U) {
            return SCELBI_LOAD_SHORT_READ;
        }
        total_read += chunk;
    }

    memcpy(ram + base, staging, image_size);
    return SCELBI_LOAD_OK;
}

const char *scelbi_load_status_string(scelbi_load_status status)
{
    switch (status) {
    case SCELBI_LOAD_OK:
        return "success";
    case SCELBI_LOAD_INVALID_ARGUMENT:
        return "invalid loader argument";
    case SCELBI_LOAD_BASE_OUT_OF_RANGE:
        return "load address is outside installed RAM";
    case SCELBI_LOAD_IMAGE_TOO_LARGE:
        return "binary would exceed installed RAM";
    case SCELBI_LOAD_STAGING_TOO_SMALL:
        return "temporary buffer is too small";
    case SCELBI_LOAD_SHORT_READ:
        return "short read while loading binary";
    case SCELBI_LOAD_READ_ERROR:
        return "invalid read result";
    default:
        return "unknown loader status";
    }
}
