/*
 * serial.h
 *
 *  Created on: Apr 11, 2023
 *      Author: merri
 */

#ifndef SERIAL_H_
#define SERIAL_H_

enum serial_port{PORTA0, PORTA1};

struct data_queue {
    char* data;
    unsigned int head;
    unsigned int tail;

    unsigned int MAX_SIZE;
};

extern struct data_queue PORTA0_RX_queue;
extern struct data_queue PORTA1_RX_queue;

unsigned int get_queue_distance(unsigned int bottom, unsigned int top, unsigned int max);
void push(struct data_queue* s, char data);
char pop(struct data_queue* s);
unsigned int queue_len(struct data_queue* s);
void push_string(struct data_queue* s, char* str);
void push_packet(struct data_queue* s, char* packet, unsigned int len);
unsigned char queue_to_str(struct data_queue* s, char* str);
void clear_queue(struct data_queue* s);

void Software_Trim();
void init_clock();

void init_UART_UCA1(unsigned long baud);
void init_UART_UCA0(unsigned long baud);
void enable_UART_interrupt(enum serial_port port);
void disable_UART_interrupt(enum serial_port port);

void putchar(char c);
void putchars(char* msg);
char waitchar(enum serial_port port);

void print_dec(const long long data, const unsigned char len);
void print_binary(long long b, int n);
void print_hex(char h);

char streq(char* str1, char* str2, unsigned int len);
long long str_to_int(char* str);
void int_to_str(char* str, const long long data, const unsigned char len);

void FXP_print(int f);
void FXP_print_2_14(int f);

#endif /* SERIAL_H_ */
