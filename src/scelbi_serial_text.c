#include "scelbi_serial_text.h"

/*
 * Optional: collapse every blank line (an LF that arrives when the cursor is
 * already at the start of an empty line is dropped). Off by default because it
 * also removes blank lines a BASIC program prints on purpose.
 * Enable with -DSCELBI_COLLAPSE_BLANK_LINES=1.
 */
#ifndef SCELBI_COLLAPSE_BLANK_LINES
#define SCELBI_COLLAPSE_BLANK_LINES 0
#endif

/*
 * The SCELBI BASIC was written for a Teletype: CR only returns the carriage,
 * LF only advances the paper. It echoes the CR you type and then sends its own
 * CR LF, so expanding every CR into CR+LF produces one blank line too many.
 * Here CR stays a plain CR and LF moves down, adding a CR only when the LF
 * does not come right after one.
 *
 * The BASIC also sends DC4 CR LF before every READY (DC4 switches the
 * Teletype punch off). On a screen that group only adds a blank line, so DC4
 * and the single CR and LF that follow it are dropped together.
 */
size_t scelbi_serial_text_normalize(uint8_t ch, bool *previous_was_cr,
                                    uint8_t output[2])
{
    /* True when the cursor sits at the start of an empty line (starts true:
     * the monitor leaves the cursor on a fresh line before the CPU runs). */
    static bool line_empty = true;
    /* Pending CR / LF to swallow after a DC4. */
    static bool swallow_cr = false;
    static bool swallow_lf = false;
    bool was_cr;

    (void)line_empty;   /* only read when SCELBI_COLLAPSE_BLANK_LINES=1 */

    if (previous_was_cr == NULL || output == NULL) {
        return 0U;
    }

    /* Teletype fill characters: not displayed, must not alter CR/LF state. */
    if (ch == 0x00U || ch == 0x7FU) {
        return 0U;
    }

    /* DC4: drop it and arm the removal of the CR LF that follows. */
    if (ch == 0x14U) {
        swallow_cr = true;
        swallow_lf = true;
        return 0U;
    }

    /*
     * Other non-printable controls are dropped too, except BEL, BS, TAB, CR
     * and LF. Dropped bytes do not alter the CR/LF state either.
     */
    if (ch < 0x20U && ch != '\r' && ch != '\n' &&
        ch != '\b' && ch != '\t' && ch != 0x07U) {
        return 0U;
    }

    /* The CR and LF right after a DC4 are swallowed, once each. */
    if (ch == '\r' && swallow_cr) {
        swallow_cr = false;
        return 0U;
    }
    if (ch == '\n' && swallow_lf) {
        swallow_cr = false;
        swallow_lf = false;
        return 0U;
    }
    if (ch != '\r' && ch != '\n') {
        swallow_cr = false;
        swallow_lf = false;
    }

    was_cr = *previous_was_cr;
    *previous_was_cr = false;

    if (ch == '\r') {
        output[0] = '\r';
        *previous_was_cr = true;
        return 1U;
    }

    if (ch == '\n') {
#if SCELBI_COLLAPSE_BLANK_LINES
        if (line_empty) {
            return 0U;
        }
#endif
        line_empty = true;
        if (was_cr) {
            output[0] = '\n';
            return 1U;
        }
        output[0] = '\r';
        output[1] = '\n';
        return 2U;
    }

    line_empty = false;
    output[0] = ch;
    return 1U;
}
