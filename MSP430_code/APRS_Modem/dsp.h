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

extern struct data_queue symbol_queue;
extern char tx_queue_empty;

void init_ADC(const char channel);
unsigned int get_ADC_result(void);
unsigned int ADC_to_millivolts(unsigned int adcval);

void init_resistor_DAC(void);
void set_resistor_DAC(unsigned char val);

void init_PTT(void);
void PTT_on(void);
void PTT_off(void);

void init_DSP_timer(void);
void enable_DSP_timer(void);
void disable_DSP_timer(void);

unsigned int get_queue_distance(unsigned int bottom, unsigned int top, unsigned int max);
void push(struct data_queue* s, char data);
char pop(struct data_queue* s);
unsigned int queue_len(struct data_queue* s);
void push_string(struct data_queue* s, char* str);
void push_packet(struct data_queue* s, char* packet, unsigned int len);

#endif /* DSP_H_ */
