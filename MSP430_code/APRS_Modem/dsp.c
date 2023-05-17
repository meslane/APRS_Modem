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

#define TX_SAMPLE_PERIOD 302
#define RX_SAMPLE_PERIOD 834

#define PLL_LIMIT 0x1000 //~0.25
#define PLL_SAMPLES_PER_SYMBOL 0x2000 //~0.5
#define PLL_INCREMENT 0x0400 //0.0625

unsigned char symbol_counter = 0;
unsigned int phase_counter = 0;

char current_symbol;
unsigned char bit_index = 0; //0-7
unsigned char current_bit;
unsigned char prev_bit;

char symbol_queue_array[400] = {0};
struct data_queue symbol_queue = {.data = symbol_queue_array,
                                  .head = 0,
                                  .tail = 0,
                                  .MAX_SIZE = 400};

char tx_queue_empty = 0;

char rx_ready; //PLL has pulsed
char rx_bit; //output from sampling

enum DSP_STATE dsp_state = DSP_TX;

/* timer interrupt */
#pragma vector=TIMER1_B0_VECTOR
__interrupt void TIMER1_B0_VECTOR_ISR (void) {
    P1OUT |= (0x01 << 4); //for test only

    static int sample;
    static int output;

    //highpass unit delays
    static int HPF_delay_in = 0;
    static int HPF_delay_out = 0;

    //highpass output
    static int HPF_out = 0;

    //mixer delay queue
    static int mixer_delay[4];
    static unsigned char mixer_ptr = 0;

    //mixer output
    static int mixer_out;

    //lowpass unit delays
    static int LPF_delay_in = 0;
    static int LPF_delay_out = 0;

    //lowpass output
    static int LPF_out = 0;

    //comparator output
    static int comp_out = 0;

    //PLL
    static int last_symbol = 0;
    static int PLL_count = 0;

    switch(dsp_state) {
    case DSP_TX:
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

            current_bit = (current_symbol >> (7 - bit_index)) & 0x01;
        }

        //modulate and increment counter
        if (tx_queue_empty == 0) {
            set_resistor_DAC(sine(phase_counter));

            if (current_bit == 1) { //1200 Hz
                phase_counter += 41;
            }
            else { //1 = 2200 Hz
                phase_counter += 75;
            }

            if (phase_counter > 899) {
                phase_counter -= 900;
            }

            symbol_counter++;
        }

        //shift in next bit and reset counter if done
        if (symbol_counter == 22) {
            symbol_counter = 0;

            bit_index = ((bit_index < 7) ? (bit_index + 1) : 0);
        }
        break;
    case DSP_RX: //recieve routine
        sample = (get_ADC_result() << 2); //scale to fixed point normalized (0 - 1)

        //highpass IIR filter (DC block)
        HPF_out = FXP_mul_2_14(sample, 0x3EF8) + FXP_mul_2_14(HPF_delay_in, 0xC108) - FXP_mul_2_14(HPF_delay_out, 0xC20F);
        HPF_delay_in = sample;
        HPF_delay_out = HPF_out;

        //mixer detector
        mixer_out = FXP_mul_2_14(HPF_out, mixer_delay[mixer_ptr]);
        mixer_delay[mixer_ptr] = HPF_out; //add to delay queue
        mixer_ptr = (mixer_ptr < 3) ? mixer_ptr + 1 : 0;

        //lowpass IIR filter
        LPF_out = FXP_mul_2_14(mixer_out, 0x12BF) + FXP_mul_2_14(LPF_delay_in, 0x12BF) - FXP_mul_2_14(LPF_delay_out, 0xE57D);
        LPF_delay_in = mixer_out;
        LPF_delay_out = LPF_out;

        //comparator
        if (LPF_out < 0) {
            comp_out = 1;
            P1OUT |= 0x02; //P1.1 for test only
        } else {
            comp_out = 0;
            P1OUT &= ~0x02;
        }

        //PLL goes here (God help me)
        P1OUT &= ~0x08; //pll clock pulse
        if (comp_out != last_symbol) { //nudge pll
            last_symbol = comp_out;

            if (PLL_count > PLL_LIMIT) {
                PLL_count -= PLL_SAMPLES_PER_SYMBOL;
            }

            PLL_count -= FXP_mul_2_14(PLL_count, PLL_SAMPLES_PER_SYMBOL);
        } else {
            if (PLL_count > PLL_LIMIT) {
                PLL_count -= PLL_SAMPLES_PER_SYMBOL;

                rx_ready = 1; //indicate to main loop that we should sample
                rx_bit = comp_out;
                P1OUT |= 0x08;
            }
        }
        PLL_count += PLL_INCREMENT; //increment PLL by ~0.0625

        break;
    default:
        break;
    }

    //reset interrupt
    TB1CCTL0 &= ~CCIFG;

    P1OUT &= ~(0x01 << 4);
}

/* ADC */
void init_ADC(const char channel) {
    ADCCTL0 &= ~ADCENC; //disable conversion
    ADCCTL0 &= ~ADCSHT; //clear sample and hold field
    ADCCTL0 |= ADCSHT_0; //4 cycle sample and hold
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

/* TX PTT switch */
void init_PTT(void) {
    P3SEL0 &= ~0x01;
    P3DIR |= 0x01;
    P3OUT &= ~0x01;
}

//enable push to talk
void PTT_on(void) {
    P3OUT |= 0x01;
}

//disable push to talk
void PTT_off(void) {
    P3OUT &= ~0x01;
}

/* sampling timer */
void init_DSP_timer(enum DSP_STATE state) {
    TB1CCTL0 = CCIE; //interrupt mode

    dsp_state = state;

    if (state == DSP_TX) {
        TB1CCR0 = TX_SAMPLE_PERIOD; //sampling rate of approximately 26.4 kHz (THIS MUST BE PRECISE - makes or breaks ability to demodulate)
    } else {
        TB1CCR0 = RX_SAMPLE_PERIOD; //period of approx. 9600 Hz
    }

    TB1CTL = TBSSEL__SMCLK | ID_0 | TBCLR; //fast peripheral clock, no division, clear at start
}

void enable_DSP_timer(void) {
    TB1CTL |= MC__UP | TBCLR;
}

void disable_DSP_timer(void) {
    TB1CTL |= TBCLR;
    TB1CTL &= ~(0b11 << 4); //stop timer
}
