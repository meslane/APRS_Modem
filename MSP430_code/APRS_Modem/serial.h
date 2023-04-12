/*
 * serial.h
 *
 *  Created on: Apr 11, 2023
 *      Author: merri
 */

#ifndef SERIAL_H_
#define SERIAL_H_

void Software_Trim();
void init_clock();

void init_UART_UCA1(unsigned long baud);
void putchar(char c);
void putchars(char* msg);

void print_dec(const long long data, const unsigned char len);

#endif /* SERIAL_H_ */
