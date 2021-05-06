//-------------------------------------------------------------------------------
// EMU86 I/O emulation
// OR566 target
//-------------------------------------------------------------------------------

#include <stdio.h>
#include <stdint.h>
#include "sd-or566.h"
#include "emu-mem-io.h"

#include "80c186eb.h"


static word_t latch = 0;

static uint8_t last_byte = 0;
static uint32_t bit_counter = 0;

static uint8_t sd_cmd = 0;
static uint32_t sd_arg = 0;
static uint8_t sd_crc = 0;
static uint8_t sd_is_acmd = 0;

static uint8_t sd_response_simple = 0;
static uint8_t sd_response_with_r7[4] = { 0 };

static FILE* fh;

#define SD_CLK(x) (x & (1 << 1))
#define SD_CS(x) (x & (1 << 5))
#define SD_MOSI(x) (x & (1 << 0))
#define SD_CREATE_MISO(x) ((x) ? (1 << 6) : 0)

static enum
{
    NOT_ASSERTED = 0,
    ASSERTED_RECEIVING_DUMMY_8_BYTES = 1,
    ASSERTED_RX_CMD = 2,
    ASSERTED_RX_ARG3 = 3,
    ASSERTED_RX_ARG2 = 4,
    ASSERTED_RX_ARG1 = 5,
    ASSERTED_RX_ARG0 = 6,
    ASSERTED_RX_CRC = 7,
    ASSERTED_TX_RESPONSE_SIMPLE = 8,
    ASSERTED_TX_RESPONSE_WITH_R7 = 9,
    ASSERTED_TX_RESPONSE_SECTOR = 10
} states = NOT_ASSERTED;


int sd_io_read (word_t p, word_t * w)
{
    switch (p)
    {
        case P2PIN:
        {
            if ((states == ASSERTED_TX_RESPONSE_SIMPLE) || (states == ASSERTED_TX_RESPONSE_WITH_R7) || (states == ASSERTED_TX_RESPONSE_SECTOR))
            {
                *w = SD_CREATE_MISO(sd_response_simple & 0x80);
                //printf("%s: got a read event on simple, returning 0x%02x!\n", __func__, *w);
            }
            return 0;
        }
        case P1LTCH:
        {
            *w = latch;
            return 0;
        }
    }
    return -1;
}

int sd_io_write (word_t p, word_t w)
{
    switch (p)
    {
        case P1LTCH:
        {
            latch = w;
            //printf("%s: write to P1LTCH 0x%04x!\n", __func__, latch);
            if (SD_CS(latch) == 0)
            {
                if (SD_CLK(latch))
                {
                    //printf("%s: got a latch event with MOSI = %d!\n", __func__, SD_MOSI(latch) ? 1 : 0);

                    last_byte <<= 1;
                    last_byte |= SD_MOSI(latch) ? 1 : 0;

                    switch (states)
                    {
                        case NOT_ASSERTED:
                        {
                            //printf("%s: transition to RECEIVING_DUMMY\n", __func__);
                            states = ASSERTED_RECEIVING_DUMMY_8_BYTES;
                            bit_counter = 1;
                            break;
                        }

                        case ASSERTED_RECEIVING_DUMMY_8_BYTES:
                        {
                            if (++bit_counter == 8)
                            {
                                bit_counter = 0;
                                states = ASSERTED_RX_CMD;
                                //printf("%s: transition to RX_CMD\n", __func__);
                            }
                            break;
                        }

                        case ASSERTED_RX_CMD:
                        {
                            if (++bit_counter == 8)
                            {
                                bit_counter = 0;
                                states = ASSERTED_RX_ARG0;
                                sd_cmd = last_byte;
                                //printf("%s: transition to RX_ARG0, cmd was = 0x%02x\n", __func__, last_byte);
                            }
                            break;
                        }

                        case ASSERTED_RX_ARG0:
                        {
                            if (++bit_counter == 8)
                            {
                                bit_counter = 0;
                                states = ASSERTED_RX_ARG1;
                                sd_arg = last_byte;
                                //printf("%s: transition to RX_ARG1, arg0 was = 0x%02x\n", __func__, last_byte);
                            }
                            break;
                        }

                        case ASSERTED_RX_ARG1:
                        {
                            if (++bit_counter == 8)
                            {
                                bit_counter = 0;
                                states = ASSERTED_RX_ARG2;
                                sd_arg <<= 8;
                                sd_arg |= last_byte;
                                //printf("%s: transition to RX_ARG2, arg1 was = 0x%02x\n", __func__, last_byte);
                            }
                            break;
                        }

                        case ASSERTED_RX_ARG2:
                        {
                            if (++bit_counter == 8)
                            {
                                bit_counter = 0;
                                states = ASSERTED_RX_ARG3;
                                sd_arg <<= 8;
                                sd_arg |= last_byte;
                                //printf("%s: transition to RX_ARG3, arg2 was = 0x%02x\n", __func__, last_byte);
                            }
                            break;
                        }

                        case ASSERTED_RX_ARG3:
                        {
                            if (++bit_counter == 8)
                            {
                                bit_counter = 0;
                                states = ASSERTED_RX_CRC;
                                sd_arg <<= 8;
                                sd_arg |= last_byte;
                                //printf("%s: transition to RX_CRC, arg3 was = 0x%02x\n", __func__, last_byte);
                            }
                            break;
                        }

                        case ASSERTED_RX_CRC:
                        {
                            if (++bit_counter == 8)
                            {
                                bit_counter = 0;
                                sd_crc = last_byte;
                                //printf("%s: in short, cmd = 0x%02x, arg = 0x%08x, crc = 0x%02x\n", __func__, sd_cmd, sd_arg, sd_crc);
                                switch (sd_cmd)
                                {
                                    case 0x40:
                                    {
                                        sd_response_simple = 0x1;
                                        states = ASSERTED_TX_RESPONSE_SIMPLE;
                                        break;
                                    }

                                    case 0x48:
                                    {
                                        sd_response_simple = 0x1;
                                        sd_response_with_r7[2] = 0x01;
                                        sd_response_with_r7[3] = 0xaa;
                                        states = ASSERTED_TX_RESPONSE_WITH_R7;
                                        break;
                                    }

                                    case 0x77:
                                    {
                                        sd_is_acmd = 1;
                                        sd_response_simple = 1;
                                        states = ASSERTED_TX_RESPONSE_SIMPLE;
                                        break;
                                    }

                                    case 0x69:
                                    {
                                        if (sd_is_acmd)
                                        {
                                            sd_response_simple = 0;
                                            states = ASSERTED_TX_RESPONSE_SIMPLE;
                                        }
                                        break;
                                    }

                                    case 0x7a:
                                    {
                                        sd_response_simple = 0x0;
                                        sd_response_with_r7[0] = 0x00;
                                        states = ASSERTED_TX_RESPONSE_WITH_R7;
                                        break;
                                    }

                                    case 0x51:
                                    {
                                        //printf("%s: read sector 0x%x\n", __func__, sd_arg);
                                        states = ASSERTED_TX_RESPONSE_SECTOR;
                                        sd_response_simple = 0;
                                        int sector_start = sd_arg * 512;
                                        fseek(fh, sector_start, SEEK_SET);
                                        break;
                                    }
                                }
                                
                            }
                            break;
                        }

                        case ASSERTED_TX_RESPONSE_SECTOR:
                        {
                            sd_response_simple <<= 1;

                            bit_counter++;
                            if (bit_counter == 8)
                            {
                                //printf("%s: now returning 0xfe\n", __func__);
                                sd_response_simple = 0xfe;
                            }
                            else if (bit_counter >= 16)
                            {
                                if ((bit_counter % 8) == 0)
                                {
                                    fread(&sd_response_simple, 1, 1, fh);
                                    //printf("%s: reading %d => 0x%02x\n", __func__, (bit_counter / 8), sd_response_simple);
                                }
                            }
                            break;
                        }

                        case ASSERTED_TX_RESPONSE_WITH_R7:
                        {
                            sd_response_simple <<= 1;

                            bit_counter++;
                            if (bit_counter == 8)
                            {
                                sd_response_simple = sd_response_with_r7[0];
                                //printf("%s: first r7\n", __func__);
                            }
                            else if (bit_counter == 16)
                            {
                                sd_response_simple = sd_response_with_r7[1];
                                //printf("%s: second r7\n", __func__);
                            }
                            else if (bit_counter == 24)
                            {
                                sd_response_simple = sd_response_with_r7[2];
                                //printf("%s: third r7\n", __func__);
                            }
                            else if (bit_counter == 32)
                            {
                                sd_response_simple = sd_response_with_r7[3];
                                //printf("%s: last r7\n", __func__);
                            }
                            else if (bit_counter == 40)
                            {
                                states = NOT_ASSERTED;
                                //printf("%s: transition to NOT_ASSERTED?\n", __func__);
                            }
                            break;
                        }

                        case ASSERTED_TX_RESPONSE_SIMPLE:
                        {
                            sd_response_simple <<= 1;
                            if (++bit_counter == 8)
                            {
                                states = NOT_ASSERTED;
                                //printf("%s: transition to NOT_ASSERTED?\n", __func__);
                            }
                            break;
                        }

                        default:
                            break;
                    }
                }
            }
            else
            {
                if (states != NOT_ASSERTED)
                {
                    //printf("%s: forced transition to NOT_ASSERTED (cs deasserted, latch = 0x%04x, bit_counter = %d), cmd = 0x%02x, arg = 0x%08x, crc = 0x%02x, old state = %d\n", __func__, latch, bit_counter, sd_cmd, sd_arg, sd_crc, states);
                    sd_is_acmd = 0;
                    states = NOT_ASSERTED;
                }
            }
            return 0;
        }
    }
    return -1;
}

int sd_init(const char * sd_file)
{
    fh = fopen(sd_file, "rb");
    if (!fh)
    {
        return -1;
    }
    printf("info: SD emulation enabled, disk file = %s\n", sd_file);
    return 0;
}

