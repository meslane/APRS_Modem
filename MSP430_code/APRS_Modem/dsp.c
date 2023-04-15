/*
 * dsp.c
 *
 *  Created on: Apr 11, 2023
 *      Author: merri
 */

#include <dsp.h>
#include <msp430.h>
#include <math.h>
#include <serial.h>

unsigned char symbol_counter = 0;
unsigned char phase_counter = 0;

char current_symbol;
unsigned char bit_index = 0; //0-7
unsigned char current_bit;
unsigned char prev_bit;

char symbol_queue_array[128] = {0};
struct data_queue symbol_queue = {.data = symbol_queue_array,
                                  .head = 0,
                                  .tail = 0,
                                  .MAX_SIZE = 128};

char tx_queue_empty = 1;

/* timer interrupt */
#pragma vector=TIMER1_B0_VECTOR
__interrupt void TIMER1_B0_VECTOR_ISR (void) {
    //grab next symbol
    if (symbol_counter == 0) {
        if (bit_index == 0) { //grab new symbol
            if (queue_len(&symbol_queue) == 0) {
                tx_queue_empty = 1;
            }
            else {
                current_symbol = pop(&symbol_queue);
            }
        }

        //grab next bit and preserve phase coherence
        prev_bit = current_bit;
        current_bit = (current_symbol >> (7 - bit_index)) & 0x01;

        if (current_bit != prev_bit) {
            if (current_bit == 0) { //1200 Hz
                phase_counter = phase_2200_to_1200(phase_counter);
            }
            else { //2200 Hz
                phase_counter = phase_1200_to_2200(phase_counter);
            }
        }
    }

    //modulate and increment counter
    if (tx_queue_empty == 0) {
        if (current_bit == 0) { //0 = 1200 Hz
            set_resistor_DAC(sine_1200(phase_counter));
        }
        else { //1 = 2200 Hz
            set_resistor_DAC(sine_2200(phase_counter));
        }

        symbol_counter++;

        if (current_bit == 0) { //1200 Hz
            phase_counter = (phase_counter < 21) ? (phase_counter + 1) : 0;
        }
        else { //2200 Hz
            phase_counter = (phase_counter < 11) ? (phase_counter + 1) : 0;
        }
    }

    //shift in next bit and reset counter if done
    if (symbol_counter == 22) {
        symbol_counter = 0;

        bit_index = ((bit_index < 7) ? (bit_index + 1) : 0);
    }

    //reset interrupt
    TB1CCTL0 &= ~CCIFG;
}

/* ADC */
void init_ADC(const char channel) {
    ADCCTL0 &= ~ADCENC; //disable conversion
    ADCCTL0 &= ~ADCSHT; //clear sample and hold field
    ADCCTL0 |= ADCSHT_2; //16 cycle sample and hold
    ADCCTL0 |= ADCON; //ADC on

    ADCCTL1 |= ADCSSEL_2; //SMCLK clock source
    ADCCTL1 |= ADCSHP;

    ADCCTL2 &= ~ADCRES;
    ADCCTL2 |= ADCRES_2; //12 bit resolution

    ADCMCTL0 &= ~(0xF); //clear channel select bits
    if (channel <= 0xF) {
        ADCMCTL0 |= channel;
    }
}

unsigned int get_ADC_result(void) {
    ADCCTL0 |= ADCENC | ADCSC; //start conversion
    while((ADCIFG & ADCIFG0) == 0); //wait until complete
    return ADCMEM0;
}

unsigned int ADC_to_millivolts(unsigned int adcval) {
    unsigned long long result = ((unsigned long long)adcval * 3300)/4096;
    return (unsigned int)result;
}

/* DAC */
void init_resistor_DAC(void) {
    P6SEL0 &= ~(0b1); //set P6 to I/O
    P6DIR |= 0x1F; //set P6.0-P6.4 to output
    P6OUT &= ~(0x1F); //set output low
}

void set_resistor_DAC(unsigned char val) {
    P6OUT = (val & 0x1F);
}

/* sampling timer */
void init_DSP_timer(void) {
    TB1CCTL0 = CCIE; //interrupt mode
    TB1CCR0 = 301; //sampling rate of approximately 26.4 kHz
    TB1CTL = TBSSEL__SMCLK | ID_0 | TBCLR; //fast peripheral clock, no division, clear at start
}

void enable_DSP_timer(void) {
    TB1CTL |= MC__UP | TBCLR;
}

void disable_DSP_timer(void) {
    TB1CTL |= TBCLR;
    TB1CTL &= ~(0b11 << 4); //stop timer
}

/* queue functions */
unsigned int get_queue_distance(unsigned int bottom, unsigned int top, unsigned int max) {
    if (top >= bottom) {
        return top - bottom;
    }
    else {
        return (max - bottom) + top;
    }
}

void push(struct data_queue* s, char data) {
    s->data[s->head] = data;
    s->head = s->head < (s->MAX_SIZE - 1) ? s->head + 1 : 0;
}

char pop(struct data_queue* s) {
    char temp = 0;

    temp = s->data[s->tail];
    s->tail = s->tail < (s->MAX_SIZE - 1) ? s->tail + 1 : 0;

    return temp;
}

unsigned int queue_len(struct data_queue* s) {
    return get_queue_distance(s->tail, s->head, s->MAX_SIZE);
}

void push_string(struct data_queue* s, char* str) {
    unsigned int i = 0;
    char c;

    c = str[i];
    while(c != '\0') {
        push(s, c);
        i++;
        c = str[i];
    }
}
