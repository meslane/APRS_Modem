/*
 * packet.h
 *
 *  Created on: Apr 14, 2023
 *      Author: merri
 */

#ifndef PACKET_H_
#define PACKET_H_

#define MAX_PACKET 300

struct bitstream {
    char* bytes;
    unsigned int byte_pointer;
    unsigned char bit_pointer;
};

void push_bit(struct bitstream* s, char bit);
void push_byte(struct bitstream* s, char byte);
char pop_bit(struct bitstream* s);

char peek_bit(struct bitstream* s, unsigned int index);
void set_stream_bit(struct bitstream* s, unsigned int index, char bit);

void clear_bitstream(struct bitstream* s);

void print_bitstream_bytes(struct bitstream* s);

unsigned int get_len(struct bitstream* s);

void stuff_bitstream(struct bitstream* output, struct bitstream* input);
void unstuff_bitstream(struct bitstream* output, struct bitstream* input);
void bitstream_NRZ_to_NRZI(struct bitstream* s);
void bitstream_NRZI_to_NRZ(struct bitstream* s);
void bitstream_add_flags(struct bitstream* output, struct bitstream* input, unsigned char num_start_flags, unsigned char num_end_flags);

void array_to_bitstream(struct bitstream* output, char* input, unsigned int len);
unsigned int bitstream_to_array(char* output, struct bitstream* input);

//=========
char is_digit(char c);
char ascii_to_num(char c);

void print_packet(char* packet, unsigned int len);
void print_packet_bits(char* packet, unsigned int len);

unsigned char NRZ_to_NRZI(char* packet, unsigned int len);

void str_to_AX_25_addr(char* output, char* str);
void insert_bytes(char* dest, char* src, unsigned int index, unsigned int len);

unsigned int generate_AX_25_packet_bytes(char* output, char* dest, char* src, char* digipeaters, char* payload);
void flip_bit_order(char* data, unsigned int len);
unsigned char num_ones(long var, unsigned char n);

unsigned int crc_16(char* buf, unsigned int len);

unsigned int make_AX_25_packet(char* output, char* dest, char* src, char* digipeaters, char* payload, unsigned int num_start_flags, unsigned int num_end_flags);
unsigned int demod_AX_25_packet(char* output, char* NRZI_bytes, unsigned int len);

#endif /* PACKET_H_ */
