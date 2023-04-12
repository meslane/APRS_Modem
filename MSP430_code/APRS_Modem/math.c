/*
 * math.c
 *
 *  Created on: Apr 11, 2023
 *      Author: merri
 */

#include <math.h>

unsigned char LUT_2200[22] = {16,23,29,31,29,23,16,8,2,0,2,8,16,23,29,31,29,23,16,8,2,0};
unsigned char LUT_1200[22] = {16,20,24,27,30,31,31,30,
                              27,24,20,16,11,7,4,1,
                              0,0,1,4,7,11};

/* power function */
unsigned long long pow(unsigned long long base, unsigned long long exp) {
    unsigned long long result = 1;
    unsigned long long i;

    for(i=0;i<exp;i++) {
        result *= base;
    }

    return result;
}

unsigned char sine_2200(unsigned char sample) {
    return LUT_2200[sample];
}

unsigned char sine_1200(unsigned char sample) {
    return LUT_1200[sample];
}
