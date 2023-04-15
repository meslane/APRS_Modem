/*
 * math.c
 *
 *  Created on: Apr 11, 2023
 *      Author: merri
 */

#include <math.h>

unsigned char LUT_2200[12] = {16,23,29,31,29,23,16,8,2,0,2,8};
unsigned char LUT_1200[22] = {16,20,24,27,30,31,31,30,
                              27,24,20,16,11,7,4,1,
                              0,0,1,4,7,11};


unsigned char LUT_2200_to_1200[12] = {0,2,4,5,7,9,11,13,15,16,18,20};
unsigned char LUT_1200_to_2200[22] = {0,1,1,2,2,3,3,4,4,5,5,6,7,7,8,8,9,9,10,10,11,11};

/* power function */
unsigned long long pow(unsigned long long base, unsigned long long exp) {
    unsigned long long result = 1;
    unsigned long long i;

    for(i=0;i<exp;i++) {
        result *= base;
    }

    return result;
}

void set_bit(char* c, const unsigned char n, const unsigned char val) {
    if ((val & 0x01) == 0x01) {
        *c |= (0x01 << n); //set
    }
    else {
        *c &= ~(0x01 << n); //clear
    }
}

char get_bit(char c, const unsigned char n) {
    return (c >> n) & 0x01;
}

unsigned char sine_2200(unsigned char sample) {
    return LUT_2200[sample];
}

unsigned char sine_1200(unsigned char sample) {
    return LUT_1200[sample];
}

unsigned char phase_2200_to_1200(const unsigned char phase) {
    return LUT_2200_to_1200[phase];
}

unsigned char phase_1200_to_2200(const unsigned char phase) {
    return LUT_1200_to_2200[phase];
}
