/*
 * packet.h
 *
 *  Created on: Apr 14, 2023
 *      Author: merri
 */

#ifndef PACKET_H_
#define PACKET_H_

#define FRAM_WRITE_DISABLE 0x00
#define FRAM_WRITE_ENABLE 0x01

struct bitstream {
    char* bits;
    unsigned int pointer;
};

void enable_FRAM_write(const char enable);

void append_bit(struct bitstream* stream, const char bit);
void append_bitstring(struct bitstream* stream, const char* bits);
char read_bit(struct bitstream* stream, const char bit);
void print_bitstream(struct bitstream* stream);

void append_and_stuff_bits(const char* src_bytes, struct bitstream* stream, const unsigned int len);
void NRZ_to_NRZI(struct bitstream* stream);

unsigned int bitstream_to_bytes(struct bitstream* stream, char* bytes);

char is_digit(char c);
char ascii_to_num(char c);

void print_packet(char* packet, unsigned int len);

void str_to_AX_25_addr(char* output, char* str);
void insert_bytes(char* src, char* dest, unsigned int index, unsigned int len);

unsigned int generate_AX_25_packet_bytes(char* output, char* dest, char* src, char* digipeaters, char* payload);
void flip_bit_order(char* data, unsigned int len);
unsigned int crc_16(char* buf, unsigned int len);
unsigned int stuff_bits(char* output, char* input, unsigned int len);

unsigned int make_AX_25_packet(char* output, char* dest, char* src, char* digipeaters, char* payload);

#endif /* PACKET_H_ */
