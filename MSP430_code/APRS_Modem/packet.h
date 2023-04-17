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

char is_digit(char c);
char ascii_to_num(char c);

void print_packet(char* packet, unsigned int len);
void print_packet_bits(char* packet, unsigned int len);

unsigned char NRZ_to_NRZI(char* packet, unsigned int len);

void str_to_AX_25_addr(char* output, char* str);
void insert_bytes(char* dest, char* src, unsigned int index, unsigned int len);

unsigned int generate_AX_25_packet_bytes(char* output, char* dest, char* src, char* digipeaters, char* payload);
void flip_bit_order(char* data, unsigned int len);

unsigned int crc_16(char* buf, unsigned int len);
unsigned int crc_16_alt(char* data, unsigned int len);

unsigned int stuff_bits(char* output, char* input, unsigned int len);

unsigned int add_flags(char* output, char* input, unsigned int input_len, unsigned int num_start_flags, unsigned int num_end_flags, unsigned char NRZI_bit);

unsigned int make_AX_25_packet(char* output, char* dest, char* src, char* digipeaters, char* payload, unsigned int num_start_flags, unsigned int num_end_flags);

#endif /* PACKET_H_ */
