
#include <string.h>
#include <assert.h>
#include <stdio.h>

#include "emu-80c18x.h"
#include "sd-or566.h"
#include "serial-or566.h"

static word_t pcb_start_addr = IO_PCB_DEFAULT_ADDR;

static word_t pcb [IO_PCB_SIZE];

extern int info_level;

int is_in_pcb_range(word_t p)
{
    if ((p < pcb_start_addr) ||
        (p > (pcb_start_addr + IO_PCB_SIZE)))
    {
        return 0;
    }
    return 1;
}

int pcb_read(word_t pcb_entry, word_t *w)
{
    int r = 0;
    if ((pcb_entry >= IO_PCB_SERIAL0_START) &&
        (pcb_entry <= IO_PCB_SERIAL0_END))
    {
        r = serial_io_read(pcb_entry, w);
    }
    else if ((pcb_entry == IO_PCB_P1LTCH) ||
             (pcb_entry == IO_PCB_P2PIN))
    {
        r = sd_io_read(pcb_entry, w);
    }
    
    if (r == 0)
    {
        return r;
    }
    *w = pcb[pcb_entry];
    return 0;
}

int pcb_write(word_t pcb_entry, word_t w)
{
    int r = 0;
    if ((pcb_entry >= IO_PCB_SERIAL0_START) &&
        (pcb_entry <= IO_PCB_SERIAL0_END))
    {
        r = serial_io_write(pcb_entry, w);
    }
    else if ((pcb_entry == IO_PCB_P1LTCH) ||
             (pcb_entry == IO_PCB_P2PIN))
    {
        r = sd_io_write(pcb_entry, w);
    }
    if (r == 0)
    {
        return r;
    }
    pcb[pcb_entry] = w;
    return 0;
}

int io_80c18x_read_byte (word_t p, byte_t * b)
{
    if (is_in_pcb_range(p) == 0)
    {
        return -1;
    }
    word_t pcb_entry_aligned = ((p & 0xfffe) - pcb_start_addr);
    word_t pcb_val;

    if (pcb_read(pcb_entry_aligned, &pcb_val))
    {
        return -1;
    }

    if (p & 0x1)
    {
        *b = (byte_t)(pcb_val >> 8);
    }
    else
    {
        *b = (byte_t)pcb_val;
    }

    if (info_level & 1)
    {
        printf ("pcb read_byte: byte @ 0x%04x = 0x%02x\n", p, *b);
    }

    return 0;
}

int io_80c18x_write_byte (word_t p, byte_t b)
{
    if (is_in_pcb_range(p) == 0)
    {
        return -1;
    }
    word_t pcb_entry_aligned = ((p & 0xfffe) - pcb_start_addr);
    word_t pcb_val;

    if (pcb_read(pcb_entry_aligned, &pcb_val))
    {
        return -1;
    }

    if (info_level & 1)
    {
        printf ("pcb write_byte: byte @ 0x%04x\n", p);
    }

    if (p & 0x1)
    {
        pcb_val = (pcb_val & 0x00ff) | (((word_t)b) << 8);
    }
    else
    {
        pcb_val = (pcb_val & 0xff00) | b;
    }
    
    return pcb_write(pcb_entry_aligned, pcb_val);
}

int io_80c18x_read_word (word_t p, word_t * w)
{
    if (is_in_pcb_range(p) == 0)
    {
        return -1;
    }
    word_t pcb_entry_aligned = ((p & 0xfffe) - pcb_start_addr);

    if (pcb_read(pcb_entry_aligned, w))
    {
        return -1;
    }

    if (info_level & 1)
    {
        printf ("pcb read_word: byte @ 0x%04x = 0x%04x\n", p, *w);
    }

    return 0;
}

int io_80c18x_write_word (word_t p, word_t w)
{
    if (is_in_pcb_range(p) == 0)
    {
        return -1;
    }
    word_t pcb_entry_aligned = ((p & 0xfffe) - pcb_start_addr);

    if (info_level & 1)
    {
        printf ("pcb write_word: byte @ 0x%04x, set to 0x%04x\n", p, w);
    }

    return pcb_write(pcb_entry_aligned, w);
}

void mem_io_80c18x_reset ()
{
    // TODO: init map
    if (info_level & 1)
    {
        printf ("pcb mem_io_80c18x_reset\n");
    }
}

