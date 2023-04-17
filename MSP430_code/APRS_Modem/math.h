/*
 * math.h
 *
 *  Created on: Apr 11, 2023
 *      Author: merri
 */

#ifndef MATH_H_
#define MATH_H_

unsigned long long pow(unsigned long long base, unsigned long long exp);


void set_bit(char* c, const unsigned char n, const unsigned char val);
char get_bit(char c, const unsigned char n);

unsigned char sine_2200(unsigned char sample);
unsigned char sine_1200(unsigned char sample);

unsigned char phase_2200_to_1200(const unsigned char phase);
unsigned char phase_1200_to_2200(const unsigned char phase);

unsigned char sine(unsigned int index);

#endif /* MATH_H_ */
