
#pragma once

#include "op-common.h"

#define IO_PCB_DEFAULT_ADDR 0xff00
#define IO_PCB_SIZE 0xff

#define IO_PCB_SERIAL0_START 0x60
#define IO_PCB_SERIAL0_END 0x6a

#define IO_PCB_P1LTCH 0x56
#define IO_PCB_P2PIN 0x5a

// I/O operations

int is_in_pcb_range(word_t p);

int pcb_read(word_t pcb_entry, word_t *w);
int pcb_write(word_t pcb_entry, word_t w);

int io_80c18x_read_byte (word_t p, byte_t * b);
int io_80c18x_write_byte (word_t p, byte_t b);

int io_80c18x_read_word (word_t p, word_t * w);
int io_80c18x_write_word (word_t p, word_t w);

void mem_io_80c18x_reset ();
