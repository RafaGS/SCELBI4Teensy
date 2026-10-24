#ifndef SCELBI_I8008_H
#define SCELBI_I8008_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct i8008;

struct i8008 *i8008_create(void);
void i8008_free(struct i8008 *cpu);
void i8008_reset(struct i8008 *cpu);
void i8008_trace(struct i8008 *cpu, unsigned int onoff);
void i8008_dump(struct i8008 *cpu);
uint16_t i8008_pc(struct i8008 *cpu);
int i8008_stuff(struct i8008 *cpu, uint8_t *bytes, int len);
int i8008_halted(struct i8008 *cpu);
void i8008_resume(struct i8008 *cpu);
void i8008_breakpoint(struct i8008 *cpu, uint16_t addr);
void i8008_singlestep(struct i8008 *cpu, unsigned int onoff);
void i8008_halt(struct i8008 *cpu, unsigned int onoff);
unsigned int i8008_execute(struct i8008 *cpu, unsigned int tstates);
unsigned int i8008_get_cycles(struct i8008 *cpu);

/* Platform-specific callbacks implemented by main.cpp. */
uint8_t mem_read(struct i8008 *cpu, uint16_t addr, unsigned int trace);
void mem_write(struct i8008 *cpu, uint16_t addr, uint8_t val);
uint8_t io_read(struct i8008 *cpu, uint8_t addr);
void io_write(struct i8008 *cpu, uint8_t addr, uint8_t val);

#ifdef __cplusplus
}
#endif

#endif /* SCELBI_I8008_H */
