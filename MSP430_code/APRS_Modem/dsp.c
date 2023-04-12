/*
 * dsp.c
 *
 *  Created on: Apr 11, 2023
 *      Author: merri
 */

#include <dsp.h>
#include <msp430.h>
#include <math.h>

unsigned char counter = 0;

/* timer interrupt */
#pragma vector=TIMER1_B0_VECTOR
__interrupt void TIMER1_B0_VECTOR_ISR (void) {
    set_resistor_DAC(sine_2200(counter));

    counter++;
    if (counter == 22) {
        counter = 0;
    }

    TB1CCTL0 &= ~CCIFG; //reset interrupt
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
    TB1CTL |= MC__UP | TBCLR; //enable timer
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
