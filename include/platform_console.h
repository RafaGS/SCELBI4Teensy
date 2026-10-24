#ifndef SCELBI_PLATFORM_CONSOLE_H
#define SCELBI_PLATFORM_CONSOLE_H

#ifdef __cplusplus
extern "C" {
#endif

/* printf-compatible output routed to the Teensy USB serial console. */
void console_printf(const char *format, ...);

#ifdef __cplusplus
}
#endif

#endif
