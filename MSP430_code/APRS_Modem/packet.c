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

void push_bit(struct bitstream* s, char bit) {
    if ((bit & 0x01) == 1) {
        s->bytes[s->byte_pointer] |= (0x01 << s->bit_pointer);
    } else {
        s->bytes[s->byte_pointer] &= ~(0x01 << s->bit_pointer);
    }

    if (s->bit_pointer == 0) { //move to next byte if at the end of this one
        s->bit_pointer = 7;
        s->byte_pointer++;
    } else {
        s->bit_pointer--;
    }
}

void push_byte(struct bitstream* s, char byte) {
    int index;

    for(index=7;index>=0;index--) {
        push_bit(s, (byte >> index) & 0x01);
    }
}

char pop_bit(struct bitstream* s) {
    if (s->bit_pointer == 7) { //move to prior byte if at the start of this one
        s->bit_pointer = 0;
        s->byte_pointer--;
    } else {
        s->bit_pointer++;
    }

    return (s->bytes[s->byte_pointer] >> s->bit_pointer) & 0x01;
}

char peek_bit(struct bitstream* s, unsigned int index) {
    return (s->bytes[index/8] >> (7 - (index % 8))) & 0x01;
}

void set_stream_bit(struct bitstream* s, unsigned int index, char bit) {
    if ((bit & 0x01) == 1) {
        s->bytes[index/8] |= (0x01 << (7 -(index % 8)));
    } else {
        s->bytes[index/8] &= ~(0x01 << (7 -(index % 8)));
    }
}

void clear_bitstream(struct bitstream* s) {
    s->byte_pointer = 0;
    s->bit_pointer = 7;
}

void print_bitstream_bytes(struct bitstream* s) {
    unsigned int i;
    for(i=0;i<=s->byte_pointer;i++) {
        print_hex(s->bytes[i]);
        putchar(' ');
    }

    putchars("\n\r");
}

//return number of bits in stream
unsigned int get_len(struct bitstream* s) {
    return (s->byte_pointer * 8) + (7 - s->bit_pointer);
}

void array_to_bitstream(struct bitstream* output, char* input, unsigned int len) {
    unsigned int i;

    clear_bitstream(output);

    for (i=0;i<len;i++) {
        push_byte(output, input[i]);
    }
}

unsigned int bitstream_to_array(char* output, struct bitstream* input) {
    unsigned int i = 0;

    while (i<=input->byte_pointer) {
        output[i] = input->bytes[i];
        i++;
    }

    return i;
}

//====
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


//for testing
void print_packet_bits(char* packet, unsigned int len) {
    unsigned int i;
    unsigned int j;

   for(i=0;i<len;i++) {
       for(j=0;j<8;j++) {
           putchar(((packet[i] >> (7-j)) & 0x01) + 0x30);
           putchar(' ');
       }
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
    unsigned char k;
    char temp[7];
    char repeater_string[8] = {'\0'};

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

        if (digipeaters[i] == ',') { //advance pointer and clear repeater string if not end of string
            i++;

            for (k=0;k<8;k++) {
                repeater_string[k] = '\0';
            }

        }
    }

    output[index - 1] |= 0x01; //insert last address bit to indicate end of payload

    //append control and PID
    output[index] = 0x03;
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

//add a zero after every fifth bit
void stuff_bitstream(struct bitstream* output, struct bitstream* input) {
    unsigned int i;
    unsigned char ones = 0;
    char temp;

    clear_bitstream(output);

    for (i=0;i<get_len(input);i++) {
        temp = peek_bit(input, i);
        if (temp == 1) {
            ones++;
        } else {
            ones = 0;
        }

        push_bit(output, temp);

        if (ones == 5) {
            push_bit(output, 0); //pad with zero if there are 5 consecutive ones
            ones = 0;
        }
    }
}

void bitstream_NRZ_to_NRZI(struct bitstream* s) {
    char current = 0x01; //starting level of 1

    unsigned int i;

    for (i=0;i<get_len(s);i++) {
        if (peek_bit(s, i) == 1) { //1 = no level change
            set_stream_bit(s, i, current);
        } else { //0 = level change
            current ^= 1; //flip current
            set_stream_bit(s, i, current);
        }
    }
}

void bitstream_add_flags(struct bitstream* output, struct bitstream* input, unsigned char num_start_flags, unsigned char num_end_flags) {
    unsigned int i;
    char end_flag;

    clear_bitstream(output);

    for (i=0;i<num_start_flags;i++) {
        push_byte(output, 0x01); //prepend start flags
    }

    for (i=0;i<get_len(input);i++) {
        push_bit(output, peek_bit(input, i)); //add data from original packet
    }

    //append end flags
    if (peek_bit(input, get_len(input)-1) == 1) { //0x01 if last bit is 1
        end_flag = 0x01;
    } else { //0xFE if last bit is 0
        end_flag = 0xFE;
    }

    for (i=0;i<num_end_flags;i++) {
        push_byte(output, end_flag);
    }
}

unsigned int make_AX_25_packet(char* output, char* dest, char* src, char* digipeaters, char* payload, unsigned int num_start_flags, unsigned int num_end_flags) {
    unsigned int len;
    unsigned int crc;

    char bits_array[400] = {0};
    char bits_array2[400] = {0};

    struct bitstream bits = {
                             .bytes = bits_array,
                             .bit_pointer = 7,
                             .byte_pointer = 0
    };

    struct bitstream bits2 = {
                             .bytes = bits_array2,
                             .bit_pointer = 7,
                             .byte_pointer = 0
    };

    len = generate_AX_25_packet_bytes(output, dest, src, digipeaters, payload); //make raw packet bytes

    flip_bit_order(output, len); //flip bit order of non-CRC bytes

    crc = ~crc_16(output, len); //generate CRC

    output[len] = (crc >> 8) & 0xFF;
    len++;
    output[len] = (crc) & 0xFF;
    len++;

    array_to_bitstream(&bits, output, len);

    stuff_bitstream(&bits2, &bits);
    bitstream_NRZ_to_NRZI(&bits2);

    bitstream_add_flags(&bits, &bits2, num_start_flags, num_end_flags);

    len = bitstream_to_array(output, &bits);

    return len;
}
