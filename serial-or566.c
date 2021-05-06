
#include "emu-serial.h"

#include "emu-80c18x.h"
#include "serial-or566.h"

#include "80c186eb.h"
#include "emu-con.h"

int serial_proc ()
{
	int err = 0;

	while (1) {
		// Input stream

		while (1) {
			//int res = serial_poll ();
			int res = con_poll_key();
			if (!res) break;

			word_t c;
			//err = serial_recv (&c);
			err = con_get_key(&c);
			if (err) break;

			// TODO: overflow detection
			pcb_write(R0BUF, (word_t)c);
			word_t curr_s0sts;
			pcb_read(S0STS, &curr_s0sts);
			pcb_write(S0STS, curr_s0sts | 0x40); // set Receive Interrupt (RI)
			//serial_int ();
			break;
			}

		if (err) break;

		// Output stream

		while (1) {
			word_t curr_s0sts;
			pcb_read(S0STS, &curr_s0sts);

			if (curr_s0sts & 8) break;

			word_t curr_t0buf;
			pcb_read(T0BUF, &curr_t0buf);
			//err = serial_send ((byte_t) curr_t0buf);
			err = con_put_char((byte_t) curr_t0buf);
			if (err) break;

			curr_s0sts |= 8; // transmiter empty (TXE) set
			pcb_write(S0STS, curr_s0sts);
			//serial_int ();
			break;
		}

		break;
	}

	return err;
}

// Serial I/O read
int serial_io_read (word_t p, word_t * w)
{
	if (p == R0BUF)
	{
		word_t curr_s0sts;
		pcb_read(S0STS, &curr_s0sts);
		curr_s0sts &= 0xBF; // turn off RI
		pcb_write(S0STS, curr_s0sts);
	}
	//printf("%s: read from port %04x\n", __func__, p);
	/*int r = p >> 1;
	*w = serial_regs [r];

	if (r == SERIAL_REG_RDATA) {
		serial_regs [SERIAL_REG_STATUS] &= ~SERIAL_STATUS_RDR;
		serial_int ();
		}
*/
	return -1;
}

// Serial I/O write
int serial_io_write (word_t p, word_t w)
{
	//printf("%s: %04x write to port %04x\n", __func__, w, p);
	switch (p)
	{
		case T0BUF:
		{
			//printf("write T0BUF: '%c' (%02x)\n", w, w);
			word_t curr_s0sts;
			pcb_read(S0STS, &curr_s0sts);
			pcb_write(S0STS, curr_s0sts & 0xfff7);
			break;
		}
	}
	return -1;
}

void serial_dev_term ()
	{
	}

void serial_dev_init ()
{
	pcb_write(S0STS, 0x0008);
	pcb_write(T0BUF, 0x0000);
	pcb_write(R0BUF, 0x0000);
}
