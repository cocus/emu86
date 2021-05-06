
#pragma once

#include "op-common.h"

int sd_io_read (word_t p, word_t * w);
int sd_io_write (word_t p, word_t w);

int sd_init(const char * sd_file);

