
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

//------------------------------------------------------------------------------
// Interrupt handlers
//------------------------------------------------------------------------------

// BIOS video services

static byte_t col_prev = 0;

static byte_t num_hex (byte_t n)
	{
	byte_t h = n + '0';
	if (h > '9') h += 'A' - '9';
	return h;
	}

static int int_10h ()
	{
	byte_t ah = reg8_get (REG_AH);

	byte_t al;

	word_t es;
	word_t bp;
	word_t cx;

	addr_t a;
	byte_t c;

	switch (ah)
		{
		// Set cursor position
		// Detect new line as return to first column

		case 0x02:
			c = reg8_get (REG_DL); // column
			if (c == 0 && col_prev != 0)
				{
				//serial_send (13);  // CR
				serial_send (10);  // LF
				}

			col_prev = c;
			break;

		// Get cursor position

		case 0x03:
			reg16_set (REG_CX, 0);  // null cursor
			reg16_set (REG_DX, 0);  // upper-left corner (0,0)
			break;

		// Select active page

		case 0x05:
			break;

		// Scroll up

		case 0x06:
			break;

		// Write character at current cursor position

		case 0x09:
		case 0x0A:
			serial_send (reg8_get (REG_AL));  // Redirect to serial port
			break;

		// Write as teletype to current page
		// Page ignored in video mode 7

		case 0x0E:
			serial_send (reg8_get (REG_AL));  // Redirect to serial port
			break;

		// Get video mode

		case 0x0F:
			reg8_set (REG_AL, 7);   // text monochrome 80x25
			reg8_set (REG_AH, 80);  // 80 columns
			reg8_set (REG_BH, 0);   // page 0 active
			break;

		// Get EGA video configuration

		case 0x12:
			reg8_set (REG_BH, 1);	// mono mode
			reg8_set (REG_BL, 0);	// 64k EGA
			reg8_set (REG_CH, 0);	// feature bits
			reg8_set (REG_CL, 0);	// switch settings
			break;

		// Get VGA video configuration

		case 0x1A:
			if (reg8_get(REG_AL) != 0x00)
				goto notimp;
			reg8_set (REG_AL, 0);	// no VGA
			break;

		// Write string

		case 0x13:
			es = seg_get (SEG_ES);
			bp = reg16_get (REG_BP);
			cx = reg16_get (REG_CX);
			a = addr_seg_off (es, bp);

			while (cx--)
				{
				serial_send (mem_read_byte (a++));  // Redirect to serial port
				}

			break;

		// Write byte as hexadecimal

		case 0x1D:
			al = reg8_get (REG_AL);
			c = num_hex (al >> 4);
			serial_send (c);  // Redirect to serial port
			c = num_hex (al & 0x0F);
			serial_send (c);  // Redirect to serial port
			break;

		default:
	notimp:
			fflush(stdout);
			printf ("fatal: INT 10h: AH=%hxh not implemented\n", ah);
			assert (0);
		}

	return 0;
	}


// BIOS memory services

static int int_12h ()
	{
	// 512 KiB of low memory
	// no extended memory

	reg16_set (REG_AX, 512);
	return 0;
	}


// Image info

struct diskinfo
	{
	byte_t drive;
	word_t cylinders;
	byte_t heads;
	byte_t sectors;
	int fd;
	};

struct diskinfo diskinfo[4] = {
{    0, 0, 0, 0, -1 },
{    0, 0, 0, 0, -1 },
{    0, 0, 0, 0, -1 },
{    0, 0, 0, 0, -1 }
};

#define SECTOR_SIZE 512

static struct diskinfo * find_drive (byte_t drive)
	{
	struct diskinfo *dp;

	for (dp = diskinfo; dp <= &diskinfo[sizeof(diskinfo)/sizeof(diskinfo[0])]; dp++)
		if (dp->fd != -1 && dp->drive == drive)
			return dp;
	return NULL;
	}


int image_load (char * path)
	{
	byte_t d = 0, h, s;
	word_t c;
	struct diskinfo *dp;
	struct stat sbuf;

	int fd = open (path, O_RDWR);
	if (fd < 0)
		{
		printf ("Can't open disk image: %s\n", path);
		return 1;
		}
	if (fstat (fd, &sbuf) < 0)
		{
		perror(path);
		return 1;
		}
	off_t size = sbuf.st_size / 1024;
	switch (size)
		{
		case 360:   c = 40; h = 2; s =  9; break;
		case 720:   c = 80; h = 2; s =  9; break;
		case 1440:  c = 80; h = 2; s = 18; break;
		case 2880:  c = 80; h = 2; s = 36; break;
		default:
			if (size < 2880)
				{
				printf ("Image size not supported: %s\n", path);
				close (fd);
				return 1;
				}
			d = 0x80;
			if (size <= 1032192UL)	// = 1024 * 16 * 63
				{
				c = 63; h = 16; s = 63;
				}
			else
				{
				c = (size / 4032) + 1;
				h = 64;
				s = 63;
				}
			break;
		}

		for (dp = diskinfo; dp <= &diskinfo[sizeof(diskinfo)/sizeof(diskinfo[0])]; dp++)
			{
			if (dp->fd == -1)
				{
				if (find_drive (d)) d++;
				dp->drive = d;
				dp->cylinders = c;
				dp->heads = h;
				dp->sectors = s;
				dp->fd = fd;
				printf ("info: disk image %s (DCHS %d/%d/%d/%d)\n", path, d, c, h, s);
				return 0;
				}
			}

		printf("Too many disk images: %s ignored\n", path);
		close (fd);
		return 1;
	}

void image_close (void)
		{
		struct diskinfo *dp;
		for (dp = diskinfo; dp <= &diskinfo[sizeof(diskinfo)/sizeof(diskinfo[0])]; dp++)
			{
			if (dp->fd != -1)
				close (dp->fd);
			}
		}


// Read/write disk

static int readwrite_sector (byte_t drive, int wflag, unsigned long lba,
			word_t seg, word_t off)
	{
	int err = -1;
	struct diskinfo *dp = find_drive (drive);
	if (!dp) return err;

	while (1)
		{
		ssize_t l;
		ssize_t (*op)();

		off_t o = lseek (dp->fd, lba * SECTOR_SIZE, SEEK_SET);
		if (o == -1) break;

		op = read;
		if (wflag) op = write;
		l = (*op) (dp->fd, mem_get_addr (addr_seg_off (seg, off)), SECTOR_SIZE);
		if (l != SECTOR_SIZE) break;

		// success

		err = 0;
		break;
		}

	return err;
	}


// BIOS disk services

static int int_13h ()
	{
	byte_t ah = reg8_get (REG_AH);
	byte_t d, h, s, n;
	word_t c, seg, off;
	int err = 1;
	struct diskinfo *dp;

	switch (ah)
		{
		case 0x00:  // reset drive
			break;

		case 0x02:  // read disk
		case 0x03:  // write disk
			d = reg8_get (REG_DL);        // drive
			c = reg8_get (REG_CH) | ((reg8_get (REG_CL) & 0xC0) << 2);
			h = reg8_get (REG_DH);        // head
			s = reg8_get (REG_CL) & 0x3F; // sector, base 1
			n = reg8_get (REG_AL);        // count
			seg = seg_get (SEG_ES);
			off = reg16_get (REG_BX);

			if (info_level & 1) printf ("%s: DCHS %d/%d/%d/%d @%x:%x, %d sectors\n",
				ah==2? "read_sector": "write_sector", d, c, h, s, seg, off, n);

			dp = find_drive (d);
			if (!dp || c >= dp->cylinders || h >= dp->heads || s > dp->sectors)
				{
				printf("INT 13h fn %xh: Invalid DCHS %d/%d/%d/%d\n", ah, d, c, h, s);
				break;
				}

			if (s + n > dp->sectors + 1)
				{
				printf("INT 13h fn %xh: Multi-track I/O operation rejected\n", ah);
				break;
				}

			off_t lba = (s-1) + dp->sectors * (h + c * dp->heads);
			err = 0;
			while (n-- != 0)
				{
				err |= readwrite_sector (d, ah==3, lba, seg, off);
				lba++;
				off += SECTOR_SIZE;
				}
			break;

		case 0x08:  // get drive parms
			d = reg8_get (REG_DL);
			dp = find_drive (d);
			if (!dp) break;
			c = dp->cylinders - 1;
			n = find_drive (d+1)? 2: 1;
			reg8_set (REG_BL, 4);   // CMOS 1.44M floppy
			reg8_set (REG_DL, n);   // # drives
			reg8_set (REG_CH, c & 0xFF);
			reg8_set (REG_CL, dp->sectors | (((c >> 8) & 0x03) << 6));
			reg8_set (REG_DH, dp->heads - 1);
			seg_set (SEG_ES, 0xFF00);   // fake DDPT, same as INT 1Eh
			reg16_set (REG_DI, 0x0000);
			err = 0;
			break;

		default:
			printf ("fatal: INT 13h: AH=%hxh not implemented\n", ah);
			assert (0);
		}

	flag_set (FLAG_CF, err? 1: 0);
	return 0;
	}


// BIOS misc services

static int int_15h ()
	{
	byte_t ah = reg8_get (REG_AH);
	switch (ah)
		{
		// Return CF=1 for all non implemented functions
		// as recommended by Alan Cox on the ELKS mailing list

		default:
			flag_set (FLAG_CF, 1);

		}

	return 0;
	}


// BIOS keyboard services

static byte_t key_prev = 0;

static int int_16h ()
	{
	int err = 0;
	byte_t c = 0;

	byte_t ah = reg8_get (REG_AH);
	switch (ah)
		{
		// Normal keyboard read

		case 0x00:
			if (key_prev)
				{
				reg8_set (REG_AL, key_prev);
				key_prev = 0;
				}
			else
				{
				err = serial_recv (&c);
				if (err) break;

				reg8_set (REG_AL, (byte_t) c);  // ASCII code
				}

			reg8_set (REG_AH, 0);  // No scan code
			break;

		// Peek character

		case 0x01:
			if (serial_poll ())
				{
				flag_set (FLAG_ZF, 0);
				err = serial_recv (&key_prev);
				if (err) break;

				reg8_set (REG_AL, key_prev);
				reg8_set (REG_AH, 0);
				}
			else
				{
				flag_set (FLAG_ZF, 1);  // no character in buffer
				}

			break;

		// Set typematic rate - ignore

		case 0x03:
			break;

		// Extended keyboard read

		case 0x10:
			err = serial_recv (&c);
			if (err) break;

			reg8_set (REG_AL, (byte_t) c);  // ASCII code
			reg8_set (REG_AH, 0);           // No scan code
			break;

		default:
			printf ("fatal: INT 16h: AH=%hxh not implemented\n", ah);
			assert (0);
		}

	return err;
	}


// BIOS printer services

static int int_17h ()
	{
	byte_t ah = reg8_get (REG_AH);
	switch (ah)
		{
		default:
			printf ("fatal: INT 17h: AH=%hxh not implemented\n", ah);
			assert (0);
		}

	return 0;
	}

// Disk boot

static int int_FEh ()
	{
	// Boot from first sector of first disk
	byte_t d = diskinfo[0].drive;

	if (readwrite_sector (d, 0, 0L, 0x0000, 0x7C00)) return -1;

	reg8_set (REG_DL, d);  // DL = BIOS boot drive number
	seg_set (SEG_CS, 0x0000);
	reg16_set (REG_IP, 0x7C00);

	return 0;
	}


// BIOS time services

static int int_1Ah ()
	{
	byte_t ah = reg8_get (REG_AH);
	switch (ah)
		{
		// Get system time

		case 0x00:
			reg16_set (REG_CX, 0);  // stay on 0
			reg16_set (REG_DX, 0);
			reg8_set (REG_AL, 0);   // no day wrap
			break;

		default:
			printf ("fatal: INT 1Ah: AH=%hxh not implemented\n", ah);
			assert (0);
		}

	return 0;
	}


// Interrupt handler table

int_num_hand_t _int_tab [] = {
	{ 0x03, int_03h },
	{ 0x10, int_10h },
	{ 0x12, int_12h },
	{ 0x13, int_13h },  // BIOS disk services
	{ 0x15, int_15h },
	{ 0x16, int_16h },
	{ 0x17, int_17h },
//	{ 0x19, int_19h },  // OS boot
	{ 0x1A, int_1Ah },
	{ 0xFE, int_FEh },  // disk boot
//	{ 0xFF, int_FFh },  // reserved for MON86
	{ 0,    NULL    }
	};

// BIOS extension check

static addr_t bios_boot = 0xF0000;

static void bios_check (addr_t b, int size)
	{
	byte_t sum = 0;

	for (addr_t a = b; a < (b + size); a++)
		sum += mem_read_byte (a);

	if (sum == 0)
		{
		// Add CALLF to valid extension

		mem_write_byte (bios_boot, 0x9A, 1);  // CALLF
		mem_write_word (bios_boot + 1, 0x0003, 1);
		mem_write_word (bios_boot + 3, b >> 4, 1);
		bios_boot += 5;
		}
	}

// Interrupt initialization

void int_init (void)
	{
	// ELKS saves and calls initial INT8 (timer)
	// So implement a stub for INT8 at startup

	mem_write_byte (0xF1000, 0xCF, 1);  // IRET @ F000:1000h

	mem_write_word (0x00020, 0x1000, 1);
	mem_write_word (0x00022, 0xF000, 1);

	// Dummy pointer to disk drive parameter table (DDPT) at INT 1Eh

	mem_write_word (0x00078, 0x2000, 1);
	mem_write_word (0x0007A, 0xF000, 1);

	// Scan ROM for any BIOS extension

	for (addr_t a = 0xC8000; a < 0xE0000; a += 0x00800)  // each 2K
		{
		if (mem_read_word (a) == 0xAA55)
			{
			int len = mem_read_byte (a + 2) * 512;
			bios_check (a, len);
			}
		}

	// Extension @ E000:0h must be 64K

	if (mem_read_word (0xE0000) == 0xAA55)
		{
		bios_check (0xE0000, 0x10000);
		}

	if (bios_boot != 0xF0000)
		{
		// End BIOS boot with INT 19h for OS boot

		mem_write_byte (bios_boot++, 0xCD, 1);  // INT 19h
		mem_write_byte (bios_boot++, 0x19, 1);
		}
	else
		{
		// No extension
		// Redirect to INT FEh for disk boot

		mem_write_byte (bios_boot++, 0xCD, 1);  // INT FEh
		mem_write_byte (bios_boot++, 0xFE, 1);

		// Point INT 19h to BIOS boot

		mem_write_word (0x00064, 0x0000, 1);
		mem_write_word (0x00066, 0xF000, 1);
		}

	// CPU boot @ FFFF:0h
	// Jump to BIOS boot

	mem_write_byte (0xFFFF0, 0xEA,   1);  // JMPF
	mem_write_word (0xFFFF1, 0x0000, 1);
	mem_write_word (0xFFFF3, 0xF000, 1);
	}

