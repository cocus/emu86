
#include <stdio.h>

#include "op-common.h"


// Output helpers

void print_string (char *s)
	{
	byte_t d = 0;
	char c;

	while (1)
		{
		c= s [d];
		if (!c) break;

		putchar (c);
		d++;
		}
	}

void print_column (char * s, byte_t w)
	{
	byte_t d = 0;
	char c;

	while (1)
		{
		c= s [d];
		if (!c) break;

		putchar (c);
		d++;
		}

	while (d < w)
		{
		putchar (' ');
		d++;
		}
	}
