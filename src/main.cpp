/*
 * Keep strict warnings enabled for project code, but do not report legacy
 * diagnostics emitted while parsing the pinned Teensy core and SD/SPI headers.
 * The diagnostic state is restored before any project headers/code are parsed.
 */
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpedantic"
#pragma GCC diagnostic ignored "-Wdeprecated-copy"
#pragma GCC diagnostic ignored "-Wunused-parameter"
#endif
#include <Arduino.h>
#include <SD.h>
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "i8008.h"
#include "platform_console.h"
#include "scelbi_loader.h"
#include "scelbi_serial_text.h"

#ifndef SCELBI_RAM_KB
#define SCELBI_RAM_KB 4
#endif

#if (SCELBI_RAM_KB != 1) && (SCELBI_RAM_KB != 4)
#error "[ERR] SCELBI-8H supports SCELBI_RAM_KB=1 or SCELBI_RAM_KB=4"
#endif

static constexpr uint16_t kAddressSpaceBytes = 16384;
static constexpr uint16_t kRamBytes = SCELBI_RAM_KB * 1024;
static constexpr uint8_t kPageCount = kAddressSpaceBytes / 256;
static constexpr uint8_t MF_READ = 1;
static constexpr uint8_t MF_WRITE = 2;

static struct i8008 *cpu = nullptr;
static uint8_t memory[kAddressSpaceBytes];
static uint8_t memflags[kPageCount];
static bool sdReady = false;
static bool ignoreNextLf = false;
static bool hexDebug = false;   /* D/d: log serial bytes as [hex] */

/*
 * stdio diagnostics in the original CPU core are redirected here because
 * ordinary printf() is not guaranteed to reach Teensy USB Serial.
 */
extern "C" void console_printf(const char *format, ...)
{
    char buffer[256];
    va_list args;
    va_start(args, format);
    const int count = vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    if (count <= 0) {
        return;
    }

    const size_t bytes = (count < (int)sizeof(buffer))
        ? (size_t)count
        : sizeof(buffer) - 1;
    Serial.write(reinterpret_cast<const uint8_t *>(buffer), bytes);
}

/*
 * Host input filter: the SCELBI BASIC expects a single CR as line terminator.
 * CR LF and LF alone coming from the terminal are collapsed to one CR.
 * Shares ignoreNextLf with the monitor line reader so an LF that follows the
 * CR of a monitor command (e.g. "c" + Enter) is never delivered to the CPU.
 * Returns -1 when no byte is available.
 */
static int readHostByte()
{
    while (Serial.available() > 0) {
        int c = Serial.read();
        if (c < 0) {
            return -1;
        }
        if (ignoreNextLf) {
            ignoreNextLf = false;
            if (c == '\n') {
                continue;           // LF following a CR: discard
            }
        }
        if (c == '\r') {
            ignoreNextLf = true;
        } else if (c == '\n') {
            c = '\r';               // lone LF -> CR
        }
        if (hexDebug) {
            console_printf("<%02X>", (unsigned)c);
        }
        return c;
    }
    return -1;
}

/* ------------------------- 8008 memory interface ------------------------- */
extern "C" uint8_t mem_read(struct i8008 * /*cpu*/, uint16_t addr,
                            unsigned int /*trace*/)
{
    if (addr >= kAddressSpaceBytes) {
        return 0xFF;
    }

    const uint8_t page = (uint8_t)((addr >> 8) & 0x3F);
    if ((memflags[page] & MF_READ) == 0) {
        return 0xFF;
    }
    return memory[addr];
}

extern "C" void mem_write(struct i8008 * /*cpu*/, uint16_t addr, uint8_t val)
{
    if (addr >= kAddressSpaceBytes) {
        return;
    }

    const uint8_t page = (uint8_t)((addr >> 8) & 0x3F);
    if ((memflags[page] & MF_WRITE) != 0) {
        memory[addr] = val;
    }
}

/* -------------------------- SCELBI I/O devices --------------------------- */
extern "C" uint8_t io_read(struct i8008 * /*cpu*/, uint8_t port)
{
    if (port == 4) {
        /* Byte-oriented compatibility input port. */
        const int c = readHostByte();
        if (c >= 0) {
            return (uint8_t)(c | 0x80);
        }
        return 0;
    }

    if (port == 5) {
        /*
         * Simplified bit-banged serial receiver matching the source emulator's
         * expected convention: bit 7 is the sampled line, idle is high, and
         * incoming bytes are presented as a low start bit then LSB-first data.
         * This is functional compatibility, not cycle-accurate UART timing.
         */
        static uint8_t rxByte = 0;
        static uint8_t rxBit = 0;  // 0 = waiting for a byte/start bit

        if (rxBit == 0) {
            const int c = readHostByte();
            if (c < 0) {
                return 0x80;        // serial line idle/high
            }
            /* Match the reference emulator: force the stop bit high. */
            rxByte = (uint8_t)c | 0x80;
            rxBit = 1;
            return 0;              // start bit is low
        }

        const uint8_t bit = (uint8_t)(rxBit - 1);
        const uint8_t value = (rxByte & (1u << bit)) ? 0x80 : 0;
        ++rxBit;
        if (rxBit == 9) {
            rxBit = 0;
        }
        return value;
    }

    /* Video and Scopewriter ports are intentionally not emulated in stage 1. */
    return 0xFF;
}

extern "C" void io_write(struct i8008 * /*cpu*/, uint8_t port, uint8_t val)
{
    if (port != 016) {
        /* Video/Scopewriter and other parallel ports are not wired yet. */
        return;
    }

    /*
     * Simplified bit-banged serial transmitter from the reference emulator.
     * Port 016 receives start, eight data bits, then stop.
     */
    static uint8_t txBit = 0;
    static uint8_t txByte = 0;
    static bool previousWasCr = false;

    if (txBit == 0) {
        txByte = 0;
        txBit = 1;                 // start bit
        return;
    }

    if (txBit == 9) {
        if (hexDebug) {
            console_printf("[%02X]", (unsigned)txByte);
        }
        uint8_t output[2];
        const size_t outputLength = scelbi_serial_text_normalize(
            txByte & 0x7F, &previousWasCr, output);
        if (outputLength > 0U) {
            Serial.write(output, outputLength);
        }
        txBit = 0;
        return;
    }

    if (val & 1) {
        txByte |= (uint8_t)(1u << (txBit - 1));
    }
    ++txBit;
}

/* ------------------------------- Monitor -------------------------------- */
static void printHelp()
{
    Serial.println();
    Serial.println(F("SCELBI-8H monitor commands (addresses/values are octal):"));
    Serial.println(F("  h or ?            help"));
    Serial.println(F("  p                 virtual front panel"));
    Serial.println(F("  r                 dump CPU registers"));
    Serial.println(F("  c or Enter        resume execution"));
    Serial.println(F("  b [address]       set breakpoint; no address clears it"));
    Serial.println(F("  g address,length  dump memory (comma or space separator)"));
    Serial.println(F("  w address,value   write memory"));
    Serial.println(F("  i byte [byte byte] inject 1-3 instruction bytes"));
    Serial.println(F("  x address         inject an unconditional jump"));
    Serial.println(F("  S / s             single-step on / off"));
    Serial.println(F("  T / t             trace on / off"));
    Serial.println(F("  D / d             hex log of serial bytes ([tx] <rx>) on / off"));
    Serial.println(F("  l FILE [address]  load binary from microSD; default address 0000"));
    Serial.println(F("  z                 reset CPU (RAM is preserved)"));
    Serial.println(F("  q                 report that the embedded monitor cannot exit"));
    Serial.println(F("  Ctrl-C            stop a running CPU and return to the monitor"));
    Serial.println();
}

static bool readCommandLine(char *dest, size_t capacity)
{
    if (capacity == 0) {
        return false;
    }

    size_t used = 0;
    for (;;) {
        while (Serial.available() <= 0) {
            yield();
        }

        const int raw = Serial.read();
        if (raw < 0) {
            continue;
        }
        const char c = (char)raw;

        if (ignoreNextLf) {
            ignoreNextLf = false;
            if (c == '\n') {
                continue;
            }
        }

        if (c == '\r' || c == '\n') {
            dest[used] = '\0';
            Serial.print(F("\r\n"));
            if (c == '\r') {
                ignoreNextLf = true;
            }
            return true;
        }

        if (c == 8 || c == 127) {
            if (used > 0) {
                --used;
                Serial.print(F("\b \b"));
            }
            continue;
        }

        if ((uint8_t)c >= 0x20 && (uint8_t)c <= 0x7E) {
            if (used + 1 < capacity) {
                dest[used++] = c;
                Serial.write((uint8_t)c);
            }
        }
    }
}

static char *skipSpaces(char *p)
{
    while (*p == ' ' || *p == '\t') {
        ++p;
    }
    return p;
}

static bool parseOctal(char *text, unsigned long *value)
{
    if (text == nullptr || value == nullptr) {
        return false;
    }

    text = skipSpaces(text);
    if (*text == '\0' || *text == '-') {
        return false;
    }

    char *end = nullptr;
    const unsigned long parsed = strtoul(text, &end, 8);
    if (end == text) {
        return false;
    }
    end = skipSpaces(end);
    if (*end != '\0') {
        return false;
    }
    *value = parsed;
    return true;
}

static bool parseOctalPair(char *text, unsigned long *first,
                           unsigned long *second)
{
    if (text == nullptr || first == nullptr || second == nullptr) {
        return false;
    }

    text = skipSpaces(text);
    char *end = nullptr;
    const unsigned long a = strtoul(text, &end, 8);
    if (end == text) {
        return false;
    }

    const bool hadSpace = (*end == ' ' || *end == '\t');
    if (*end == ',') {
        ++end;
    } else if (!hadSpace) {
        return false;
    }
    end = skipSpaces(end);

    char *end2 = nullptr;
    const unsigned long b = strtoul(end, &end2, 8);
    if (end2 == end) {
        return false;
    }
    end2 = skipSpaces(end2);
    if (*end2 != '\0') {
        return false;
    }

    *first = a;
    *second = b;
    return true;
}

static void showPanel()
{
    const uint16_t address = i8008_pc(cpu);
    const uint8_t content = mem_read(cpu, address, 0);
    Serial.println();
    Serial.println(F("SCELBI-8H VIRTUAL FRONT PANEL"));
    console_printf("PAGE (derived)     %02o\r\n", (address >> 8) & 077);
    console_printf("MEMORY ADDRESS     %04o (octal)\r\n", address);
    Serial.print(F("MEMORY CONTENTS    "));
    for (int bit = 7; bit >= 0; --bit) {
        Serial.write((content & (1u << bit)) ? '1' : '0');
    }
    console_printf("  (%03o octal, %02X hex)\r\n", content, content);
    Serial.println(F("CPU STATE          HALTED/PAUSED (emulator state)"));
    Serial.println(F("Note: PAGE is derived from PC; this is not a physical panel readout."));
    Serial.println();
}

static size_t readFromSdFile(void *context, uint8_t *destination,
                             size_t capacity)
{
    File *file = static_cast<File *>(context);
    return file->read(destination, capacity);
}

static void loadBinaryFromSd(char *args)
{
    if (!sdReady) {
        Serial.println(F("SD unavailable: check the card and BUILTIN_SDCARD."));
        return;
    }

    char *filename = strtok(args, " \t");
    char *baseText = strtok(nullptr, " \t");
    if (filename == nullptr) {
        Serial.println(F("Usage: l filename.bin [octal-address]"));
        return;
    }

    unsigned long base = 0;
    if (baseText != nullptr && !parseOctal(baseText, &base)) {
        Serial.println(F("Invalid octal load address."));
        return;
    }
    if (base >= kRamBytes) {
        Serial.println(F("Load address is outside installed RAM."));
        return;
    }

    File file = SD.open(filename, FILE_READ);
    if (!file) {
        Serial.print(F("Cannot open: "));
        Serial.println(filename);
        return;
    }

    const uint32_t fileSize = file.size();

    /* Stage the full read first; RAM changes only after a complete read. */
    static uint8_t loadBuffer[kAddressSpaceBytes];
    const scelbi_load_status status = scelbi_load_transactional(
        memory, kRamBytes, (size_t)base, (size_t)fileSize,
        loadBuffer, sizeof(loadBuffer), readFromSdFile, &file);
    file.close();

    if (status != SCELBI_LOAD_OK) {
        if (status == SCELBI_LOAD_IMAGE_TOO_LARGE) {
            Serial.println(F("Binary would exceed installed RAM; nothing loaded."));
        } else if (status == SCELBI_LOAD_SHORT_READ) {
            Serial.println(F("Short read while loading binary; RAM unchanged."));
        } else {
            Serial.print(F("Loader error: "));
            Serial.println(scelbi_load_status_string(status));
        }
        return;
    }

    console_printf("[Loaded %lu bytes at %04lo-%04lo from %s]\r\n",
                   (unsigned long)fileSize, base,
                   fileSize ? base + fileSize - 1UL : base, filename);
}

static void executeMonitorCommand(char *line)
{
    char *cmd = skipSpaces(line);
    if (*cmd == '\0' || *cmd == 'c') {
        i8008_resume(cpu);
        return;
    }

    const char command = *cmd++;
    cmd = skipSpaces(cmd);

    switch (command) {
    case '?':
    case 'h':
        printHelp();
        return;

    case 'p':
        showPanel();
        return;

    case 'r':
        i8008_dump(cpu);
        return;

    case 'b': {
        if (*cmd == '\0') {
            i8008_breakpoint(cpu, 0xFFFF);
            Serial.println(F("Breakpoint cleared."));
            return;
        }
        unsigned long address;
        if (!parseOctal(cmd, &address) || address >= kAddressSpaceBytes) {
            Serial.println(F("Usage: b [octal-address]"));
            return;
        }
        i8008_breakpoint(cpu, (uint16_t)address);
        console_printf("Breakpoint set at %04lo (octal).\r\n", address);
        return;
    }

    case 'g': {
        unsigned long start, length;
        if (!parseOctalPair(cmd, &start, &length) ||
            start >= kAddressSpaceBytes || length > kAddressSpaceBytes ||
            start + length > kAddressSpaceBytes) {
            Serial.println(F("Usage: g octal-address,octal-length"));
            return;
        }
        for (unsigned long offset = 0; offset < length; ++offset) {
            const unsigned long address = start + offset;
            const uint8_t value = mem_read(cpu, (uint16_t)address, 1);
            console_printf("%04lo  %03o  (%u)\r\n", address, value, value);
        }
        return;
    }

    case 'w': {
        unsigned long address, value;
        if (!parseOctalPair(cmd, &address, &value) ||
            address >= kAddressSpaceBytes || value > 0xFF) {
            Serial.println(F("Usage: w octal-address,octal-byte"));
            return;
        }
        if (address >= kRamBytes) {
            Serial.println(F("Address is outside installed RAM; write ignored."));
            return;
        }
        mem_write(cpu, (uint16_t)address, (uint8_t)value);
        Serial.println(F("Memory written."));
        return;
    }

    case 'i': {
        uint8_t bytes[3];
        int count = 0;
        while (*cmd != '\0' && count < 3) {
            cmd = skipSpaces(cmd);
            if (*cmd == '\0') {
                break;
            }
            char *end = nullptr;
            const unsigned long value = strtoul(cmd, &end, 8);
            if (end == cmd || value > 0xFF) {
                Serial.println(F("Usage: i octal-byte [octal-byte octal-byte]"));
                return;
            }
            bytes[count++] = (uint8_t)value;
            cmd = end;
            if (*cmd != '\0' && *cmd != ' ' && *cmd != '\t') {
                Serial.println(F("Separate instruction bytes with spaces."));
                return;
            }
        }
        if (count == 0 || *skipSpaces(cmd) != '\0') {
            Serial.println(F("Usage: i octal-byte [octal-byte octal-byte]"));
            return;
        }
        if (i8008_stuff(cpu, bytes, count) != 0) {
            Serial.println(F("Instruction injection failed."));
        }
        return;
    }

    case 'x': {
        unsigned long address;
        if (!parseOctal(cmd, &address) || address >= kAddressSpaceBytes) {
            Serial.println(F("Usage: x octal-address"));
            return;
        }
        uint8_t jump[3];
        jump[0] = 0104;
        jump[1] = (uint8_t)(address & 0xFF);
        jump[2] = (uint8_t)(address >> 8);
        if (i8008_stuff(cpu, jump, 3) != 0) {
            Serial.println(F("Jump injection failed."));
        }
        return;
    }

    case 'S':
        i8008_singlestep(cpu, 1);
        Serial.println(F("Single-step enabled; enter c to execute one instruction."));
        return;

    case 's':
        i8008_singlestep(cpu, 0);
        Serial.println(F("Single-step disabled."));
        return;

    case 'T':
        i8008_trace(cpu, 1);
        Serial.println(F("Trace enabled."));
        return;

    case 't':
        i8008_trace(cpu, 0);
        Serial.println(F("Trace disabled."));
        return;

    case 'D':
        hexDebug = true;
        Serial.println(F("Serial hex log enabled: [tx byte] <rx byte>."));
        return;

    case 'd':
        hexDebug = false;
        Serial.println(F("Serial hex log disabled."));
        return;

    case 'l':
        loadBinaryFromSd(cmd);
        return;

    case 'z':
        i8008_reset(cpu);
        Serial.println(F("CPU reset; RAM preserved."));
        return;

    case 'q':
        Serial.println(F("The embedded monitor has no host process to quit. Use z to reset the CPU."));
        return;

    default:
        Serial.println(F("Unknown command. Enter h for help."));
        return;
    }
}

void setup()
{
    Serial.begin(115200);
    const uint32_t waitStart = millis();
    while (!Serial && (uint32_t)(millis() - waitStart) < 3000) {
        delay(10);
    }

    memset(memory, 0, sizeof(memory));
    memset(memflags, 0, sizeof(memflags));
    for (uint16_t page = 0; page < (kRamBytes / 256); ++page) {
        memflags[page] = MF_READ | MF_WRITE;
    }

    cpu = i8008_create();
    if (cpu == nullptr) {
        Serial.println(F("[ERR] FATAL: unable to allocate the 8008 CPU state."));
        return;
    }
    i8008_reset(cpu);   // Reference emulator starts halted on power-up.
    i8008_trace(cpu, 0);

    sdReady = SD.begin(BUILTIN_SDCARD);

    Serial.println();
    Serial.println(F("===================================="));
    Serial.println(F(" SCELBI4Teensy - SCELBI-8H emulator "));
    Serial.println();
    Serial.println(F(" by RafaGS "));
    Serial.println(F(" http://minibots.wordpress.com "));
    Serial.println(F("===================================="));

    console_printf("[SYS] RAM enabled: %u KB; USB console: 115200 baud.\r\n",
                   (unsigned)(kRamBytes / 1024));
    Serial.println(sdReady
        ? F("[SYS] microSD: ready (binary loader enabled).")
        : F("[SYS] microSD: not detected (monitor and CPU remain available)."));
    Serial.println(F("[SYS] CPU starts halted. Enter h for monitor help."));
}

void loop()
{
    if (cpu == nullptr) {
        return;
    }

    if (i8008_halted(cpu)) {
        Serial.print(F("H."));
        char line[96];
        if (readCommandLine(line, sizeof(line))) {
            executeMonitorCommand(line);
        }
        return;
    }

    /* Ctrl-C is the monitor break key while the emulated CPU is running. */
    if (Serial.available() > 0 && Serial.peek() == 0x03) {
        (void)Serial.read();
        i8008_halt(cpu, 1);
        Serial.println();
        Serial.println(F("[CPU stopped by Ctrl-C]"));
        return;
    }

    i8008_execute(cpu, 2500);   // Reference scheduler quantum: approx. 5 ms at 500 kHz.
    if (i8008_halted(cpu)) {
        Serial.println();
        Serial.println(F("[CPU halted or paused: HLT, breakpoint, or single-step]"));
    }
    delay(5);
}
