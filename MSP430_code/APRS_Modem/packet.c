/*
 * packet.c
 *
 *  Created on: Apr 14, 2023
 *      Author: merri
 */
#include <msp430.h>
#include <packet.h>
#include <serial.h>
#include <math.h>

/* enable/disable writing to FRAM */
void enable_FRAM_write(const char enable) {
    switch(enable) {
    case FRAM_WRITE_ENABLE:
        SYSCFG0 = FRWPPW | DFWP; //enable FRAM write
        break;
    case FRAM_WRITE_DISABLE:
        SYSCFG0 = FRWPPW | PFWP | DFWP; //disable FRAM write
        break;
    default:
        break;
    }
}

void append_bit(struct bitstream* stream, const char bit) {
    stream->bits[stream->pointer] = (bit & 0x01);
    stream->pointer++;
}

void append_bitstring(struct bitstream* stream, const char* bits) {
    char b;
    unsigned int i = 0;

    b = bits[i];
    i++;
    while (b != '\0') {
        if (b == '1') {
            append_bit(stream, 1);
        } else {
            append_bit(stream, 0);
        }

        b = bits[i];
        i++;
    }
}

char read_bit(struct bitstream* stream, const char bit) {
    return stream->bits[bit];
}

void print_bitstream(struct bitstream* stream) {
    unsigned int i;

    for (i=0;i<stream->pointer;i++) {
        if (stream->bits[i] == 0) {
            putchar('0');
        }
        else {
            putchar('1');
        }
    }

    putchars("\n\r");
}

//append data bits and stuff them with zeros if there are 5 consecutive ones
void append_and_stuff_bits(const char* src_bytes, struct bitstream* stream, const unsigned int len) {
    unsigned int i;
    unsigned int j;

    unsigned char bit;

    unsigned char ones = 0;

    for(i=0;i<len;i++) {
        for(j=0;j<8;j++) {
            bit = get_bit(src_bytes[i], j);

            append_bit(stream, bit);

            //stuff bits (insert a 0 if 5 ones in a row)
            if (bit == 1) {
                ones++;
            }
            else {
                ones = 0;
            }

            if (ones >= 5) {
                append_bit(stream, 0x00); //append a zero after 5 ones
                ones = 0;
            }
        }
    }
}

//convert a standard (aka NRZ) bitstream to NRZI
//algorithm from: https://inst.eecs.berkeley.edu/~ee123/sp17/lab/lab5/Lab5_Part_C-APRS.html
void NRZ_to_NRZI(struct bitstream* stream) {
    char current = 1;

    unsigned int i;
    for (i=0;i<stream->pointer;i++) {
        if (stream->bits[i] == 1) {
            stream->bits[i] = current & 0x01;
        }
        else {
            stream->bits[i] = (~current) & 0x01;
        }
        current = stream->bits[i] & 0x01;
    }
}

unsigned int bitstream_to_bytes(struct bitstream* stream, char* bytes) {
    unsigned int i;
    unsigned char bit_index = 0;
    unsigned int byte_index = 0;
    unsigned char byte = 0x00;

    for (i=0;i<stream->pointer;i++) {
        if (read_bit(stream, i) == 1) {
            byte |= (0x01 << bit_index);
        }

        if (bit_index == 7) {
            bytes[byte_index] = byte;
            byte = 0;
            byte_index++;
            bit_index = 0;
        }
        else {
            bit_index++;
        }
    }

    if (bit_index != 0) { //if last bits don't fit evenly into a byte
        bytes[byte_index] = byte;
        byte_index++;
    }

    return byte_index;
}
