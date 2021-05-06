//------------------------------------------------------------------------------
// EMU86 - ELKS ROM stub (BIOS)
//------------------------------------------------------------------------------

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include <assert.h>
#include <sys/stat.h>

#include "sd-or566.h"

extern int info_level;


int rom_image_load (char * path)
	{
		return sd_init(path);
	}

void rom_term (void)
	{

	}

// ROM stub (BIOS) initialization

void rom_init (void)
	{

	}
