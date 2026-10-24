#ifndef SCELBI_SERIAL_TEXT_H
#define SCELBI_SERIAL_TEXT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Normalize one decoded 7-bit serial character for a text terminal.
 *
 * CR and standalone LF are emitted as CRLF. If an LF immediately follows a
 * CR, it is consumed because the CR has already emitted the complete CRLF.
 * This accepts programs that use CR-only or CRLF line endings without
 * producing an extra blank line for CRLF output.
 *
 * Returns the number of output bytes (0, 1, or 2). The caller supplies a
 * two-byte output buffer and retains previous_was_cr between characters.
 */
size_t scelbi_serial_text_normalize(uint8_t ch, bool *previous_was_cr,
                                    uint8_t output[2]);

#ifdef __cplusplus
}
#endif

#endif
