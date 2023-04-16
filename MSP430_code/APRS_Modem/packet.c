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
            byte |= (0x01 << (7 - bit_index));
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

char is_digit(char c) {
    if (c >= 0x30 && c <= 0x39) {
        return 1;
    } else {
        return 0;
    }
}

char ascii_to_num(char c) {
    if (c >= 0x30 && c <= 0x39) {
        return (c - 0x30);
    } else {
        return 0;
    }
}

void print_packet(char* packet, unsigned int len) {
    unsigned int i;

    for(i=0;i<len;i++) {
        print_hex(packet[i]);
        putchar(' ');
    }
    putchars("\n\r");
}

//convert an address string in the style of aprs.fi to the proper format for sending in a packet
void str_to_AX_25_addr(char* output, char* str) {
    char c;
    char d[2];

    unsigned char i = 0;
    unsigned char j;

    unsigned char SSID;

    c = str[i];
    while (c != '\0' && c != '-' && i < 6) { //check for dash or end of string if no dash
        output[i] = (c << 1); //must left shift by one
        i++;
        c = str[i];
    }

    for (j=i;j<6;j++) {
        output[j] = 0x40; //pad with spaces
    }

    if (c == '-') { //if there is an SSID
        i++;
        d[0] = str[i];
        i++;
        d[1] = str[i];

        if (is_digit(d[1]) && d[0] == '1') { //if there are two digits
            SSID = 10 + ascii_to_num(d[1]);
        } else {
            SSID = ascii_to_num(d[0]);
        }
    } else {
        SSID = 0;
    }

    output[6] = 0x60 | ((SSID & 0x0F) << 1);
}

void insert_bytes(char* dest, char* src, unsigned int index, unsigned int len) {
    unsigned int i;

    for(i=0;i<len;i++) {
        dest[index + i] = src[i];
    }
}

unsigned int generate_AX_25_packet_bytes(char* output, char* dest, char* src, char* digipeaters, char* payload) {
    unsigned int index = 0;
    unsigned int i;
    unsigned int j;
    char temp[7];
    char repeater_string[10];

    //insert destination
    str_to_AX_25_addr(temp, dest);
    insert_bytes(output, temp, 0, 7);
    index += 7;

    //insert source
    str_to_AX_25_addr(temp, src);
    insert_bytes(output, temp, index, 7);
    index += 7;

    i = 0;
    while(digipeaters[i] != '\0') { //append addresses until end of string
        j = 0;
        while(digipeaters[i] != ',' && digipeaters[i] != '\0') {
            repeater_string[j] = digipeaters[i];
            i++;
            j++;
        }

        str_to_AX_25_addr(temp, repeater_string);
        insert_bytes(output, temp, index, 7);
        index += 7;

        if (digipeaters[i] == ',') { //advance pointer if not end of string
            i++;
        }
    }

    output[index - 1] |= 0x01; //insert last address bit to indicate end of payload

    //append control and PID
    output[index] = 0x3F;
    index++;
    output[index] = 0xF0;
    index++;

    //append payload
    i=0;
    while(payload[i] != '\0') {
        output[index] = payload[i];
        index++;
        i++;
    }

    return index;
}

//invert bit order per octet as is expected by AX.25
void flip_bit_order(char* data, unsigned int len) {
    unsigned int i;
    unsigned char j;
    char temp;

    for(i=0;i<len;i++) {
        temp = data[i];
        data[i] = 0x00;
        for(j=0;j<8;j++) {
            data[i] |= ((temp >> (7-j)) & 0x01) << j;
        }
    }
}

//calculate the 16 bit CRC of a given sequence
//adapted from: https://stackoverflow.com/a/25256043
unsigned int crc_16(char *buf, unsigned int len) {
    const unsigned int poly = 0x1021;
    unsigned int crc = 0xFFFF;

    while (len--) {
        crc ^= *buf++ << 8;
        crc = crc & 0x8000 ? (crc << 1) ^ poly : crc << 1;
        crc = crc & 0x8000 ? (crc << 1) ^ poly : crc << 1;
        crc = crc & 0x8000 ? (crc << 1) ^ poly : crc << 1;
        crc = crc & 0x8000 ? (crc << 1) ^ poly : crc << 1;
        crc = crc & 0x8000 ? (crc << 1) ^ poly : crc << 1;
        crc = crc & 0x8000 ? (crc << 1) ^ poly : crc << 1;
        crc = crc & 0x8000 ? (crc << 1) ^ poly : crc << 1;
        crc = crc & 0x8000 ? (crc << 1) ^ poly : crc << 1;
    }

    return crc & 0xFFFF;
}

unsigned int stuff_bits(char* output, char* input, unsigned int len) {
    unsigned int input_byte_index = 0;
    unsigned char input_bit_index = 7;

    unsigned int output_byte_index = 0;
    unsigned char output_bit_index = 7;

    unsigned char ones = 0;

    char bit;

    unsigned int i;

    output[output_byte_index] = 0x7E; //add start flag
    output_byte_index++;

    while (input_byte_index < len) { //stuff data
        bit = ((input[input_byte_index] >> input_bit_index) & 0x01);
        output[output_byte_index] |= (bit << output_bit_index);

        if (bit == 1) {
            ones++;
        } else  {
            ones = 0;
        }

        if (ones == 5) {
            if (output_bit_index == 0) {
                output_bit_index = 7;
                output_byte_index++;
            } else {
                output_bit_index--;
            }
        }

        if (input_bit_index == 0) {
            input_bit_index = 7;
            input_byte_index++;
        } else {
            input_bit_index--;
        }

        if (output_bit_index == 0) {
            output_bit_index = 7;
            output_byte_index++;
        } else {
            output_bit_index--;
        }
    }

    for(i=0;i<8;i++) { //append end flag
        output[output_byte_index] |= (((0x7E >> i) & 0x01) << output_bit_index);

        if (output_bit_index == 0) {
            output_bit_index = 7;
            output_byte_index++;
        } else {
            output_bit_index--;
        }
    }

    return output_byte_index + 1;
}

unsigned int make_AX_25_packet(char* output, char* dest, char* src, char* digipeaters, char* payload) {
    unsigned int len;
    unsigned int crc;

    char packet[350];

    len = generate_AX_25_packet_bytes(packet, dest, src, digipeaters, payload); //make raw packet bytes
    flip_bit_order(packet, len); //flip bit order of non-CRC bytes
    crc = crc_16(packet, len); //generate CRC

    packet[len] = (crc >> 8) & 0xFF;
    len++;
    packet[len] = (crc) & 0xFF;
    len++;

    len = stuff_bits(output, packet, len);

    return len;
}
