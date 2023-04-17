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

unsigned char sine(unsigned int index);

#endif /* MATH_H_ */
