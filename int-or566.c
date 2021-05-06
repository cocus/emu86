
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <assert.h>
#include <sys/stat.h>

#include "emu-mem-io.h"
#include "emu-proc.h"
#include "emu-serial.h"
#include "emu-int.h"

#include "int-elks.h"

extern int info_level;

//------------------------------------------------------------------------------
// Interrupt controller
//------------------------------------------------------------------------------


int _int_mode [INT_LINE_MAX] =
	{ 1, 1, 1, 1, 1, 1, 1, 1};

int _int_line_max = INT_LINE_MAX;
int _int_prio_max = INT_PRIO_MAX;

int _int_line [INT_LINE_MAX];

int _int_prio [INT_LINE_MAX] =
	{ 0, 1, 2, 3, 4, 5, 6, 7};

int _int_vect [INT_LINE_MAX] =
	{ 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F };

int _int_mask [INT_LINE_MAX] =
	{ 1, 1, 1, 1, 1, 1, 1, 1};

int _int_req [INT_LINE_MAX] =
	{ 0, 0, 0, 0, 0, 0, 0, 0};

int _int_serv [INT_LINE_MAX] =
	{ 0, 0, 0, 0, 0, 0, 0, 0};


// Interrupt handler table

int_num_hand_t _int_tab [] = {
	{ 0,    NULL    }
	};

// Interrupt initialization

void int_init (void)
	{
	}

