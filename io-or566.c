//-------------------------------------------------------------------------------
// EMU86 I/O emulation
// OR566 target
//-------------------------------------------------------------------------------

#include <stdio.h>
#include "emu-mem-io.h"
#include "emu-80c18x.h"

extern int info_level;

//-------------------------------------------------------------------------------

int io_read_byte (word_t p, byte_t * b)
	{
	if (is_in_pcb_range(p))
		return io_80c18x_read_byte (p, b);

	switch (p)
		{
		default:
			*b = 0xFF;
			break;
		}
	if (info_level & 4) printf("[ INB %3xh AL %02xh]\n", p, *b);
	return 0;
	}

int io_write_byte (word_t p, byte_t b)
	{
	if (is_in_pcb_range(p))
		return io_80c18x_write_byte (p, b);
	switch (p)
		{
		default:
			if (info_level & 4) printf("[OUTB %3xh AL %0xh]\n", p, b);
		}
	return 0;
	}

//-------------------------------------------------------------------------------

int io_read_word (word_t p, word_t * w)
	{
	int err;

	if (p & 0x0001) {
		// bad alignment
		err = -1;
		}
	else {
		if (is_in_pcb_range(p))
			return io_80c18x_read_word (p, w);
		// no port
		if (info_level & 4) printf("[ INW %3xh AX %04xh]\n", p, *w);
		*w = 0xFFFF;
		err = 0;
		}

	return err;
	}

int io_write_word (word_t p, word_t w)
	{
	int err;

	if (p & 0x0001) {
		// bad alignment
		err = -1;
		}
	else {
		if (is_in_pcb_range(p))
			return io_80c18x_write_word (p, w);
		// no port
		if (info_level & 4) printf("[OUTW %3xh AX %0xh]\n", p, w);
		err = 0;
		}

	return err;
	}

//-------------------------------------------------------------------------------

