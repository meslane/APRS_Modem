/*
 * dsp.h
 *
 *  Created on: Apr 11, 2023
 *      Author: merri
 */

#ifndef DSP_H_
#define DSP_H_

struct data_queue {
    char* data;
    unsigned int head;
    unsigned int tail;

    unsigned int MAX_SIZE;
};

void init_ADC(const char channel);
unsigned int get_ADC_result(void);
unsigned int ADC_to_millivolts(unsigned int adcval);

void init_resistor_DAC(void);
void set_resistor_DAC(unsigned char val);

void init_DSP_timer(void);

unsigned int get_queue_distance(unsigned int bottom, unsigned int top, unsigned int max);
void push(struct data_queue* s, char data);
char pop(struct data_queue* s);

#endif /* DSP_H_ */
