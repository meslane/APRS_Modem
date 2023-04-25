/*
 * ui.c
 *
 *  Created on: Apr 24, 2023
 *      Author: merri
 */
#include <msp430.h>

#include <ui.h>
#include <math.h>
//#include <serial.h>

enum encoder_dir encoder_state = NONE;

char sleep_done = 0;

#pragma vector=PORT4_VECTOR
__interrupt void PORT4_ISR(void) {
    if ((P4IFG & 0x10) == 0x10) { //if clock triggered
        if ((P4IN & 0x80) == 0x80) {
            encoder_state = CW;
        } else {
            encoder_state = CCW;
        }
    }
    else { //if button triggered
        encoder_state = BUTTON;
    }

    P4IFG &= ~(0x50); //reset interrupt flags
}


enum encoder_dir get_encoder_state(void) {
    enum encoder_dir temp;
    temp = encoder_state;
    encoder_state = NONE;
    return temp;
}

/* delay ISR */
#pragma vector=TIMER2_B0_VECTOR
__interrupt void TIMER2_B0_VECTOR_ISR (void) {
    TB2CTL |= TBCLR;
    TB2CTL &= ~(0b11 << 4); //stop timer

    TB2CCTL0 &= ~CCIFG; //reset interrupt

    sleep_done = 1;
    //__bic_SR_register_on_exit(LPM3_bits); //exit sleep mode
}

/* slow delay clock, f = 32.768 kHz divded by 8, p = 244.144us*/
void hardware_delay(unsigned int d) { //4 ~= 1ms
    sleep_done = 0;

    TB2CCTL0 = CCIE; //interrupt mode
    TB2CCR0 = d; //trigger value
    TB2CTL = TBSSEL__ACLK | MC__UP | ID_3 | TBCLR; //slow clock, count up, no division, clear at start

    while(sleep_done == 0);
    sleep_done = 0;
    //__bis_SR_register(LPM3_bits | GIE); //sleep until interrupt is triggered
}

void software_delay(unsigned long long d) {
    unsigned long long i;
    for(i=0;i<d;i++);
}

void init_encoder(void) {
    P4DIR &= ~(0x10); //set P4.4 to input
    P4DIR &= ~(0x40); //set P4.6 to input
    P4DIR &= ~(0x80); //set P4.7 to input
    P4REN &= ~(0xD0); //disable pull resistors
    P4IES &= ~(0x10); //trigger clock on rising edge
    P4IES |= (0x40); //trigger button on falling edge
    P4IE |= 0x10; //enable interrupts
    P4IE |= 0x40;
}

void init_LCD(void) {
    /* LCD Pinout:
     * P1.2 = EN (enable)
     * P1.3 = RS (register select)
     * --------
     * P1.4 = D0
     * P5.3 = D1
     * P5.1 = D2
     * P5.0 = D3
     * P5.4 = D4
     * P1.1 = D5
     * P3.5 = D6
     * P3.1 = D7
     */

    P5SEL1 = 0x00; //set port 5 to GPIO

    P1DIR |= 0x1E; //set P1.1-P1.4 to output
    P3DIR |= 0x22; //set P3.3 and P3.5 to output
    P5DIR |= 0x1B; //set P5.0, P5.1, P5.3 and P5.4 to output

    hardware_delay(450); //delay to let LCD settle

    write_LCD(COMMAND, 0x38);
    write_LCD(COMMAND, 0x06);
    write_LCD(COMMAND, 0x0F);
    write_LCD(COMMAND, 0x01);

    hardware_delay(45);
}

void write_LCD(enum LCD_input type, unsigned char data) {
    switch (type) {
    case DATA:
        P1OUT |= 0x08;
        break;
    case COMMAND:
        P1OUT &= ~(0x08);
        break;
    }

    //set parallel bits (I'm sorry)
    set_bit((char*)&P1OUT, 4, (data >> 0) & 0x01);
    set_bit((char*)&P5OUT, 3, (data >> 1) & 0x01);
    set_bit((char*)&P5OUT, 1, (data >> 2) & 0x01);
    set_bit((char*)&P5OUT, 0, (data >> 3) & 0x01);
    set_bit((char*)&P5OUT, 4, (data >> 4) & 0x01);
    set_bit((char*)&P1OUT, 1, (data >> 5) & 0x01);
    set_bit((char*)&P3OUT, 5, (data >> 6) & 0x01);
    set_bit((char*)&P3OUT, 1, (data >> 7) & 0x01);

    P1OUT |= 0x04;
    hardware_delay(1);
    P1OUT &= ~(0x04);
    hardware_delay(1);
}

void set_LCD_cursor(unsigned char pos) {
    if (pos < 16) {
        write_LCD(COMMAND, 0x80 + pos);
    } else if (pos < 32) {
        write_LCD(COMMAND, 0xB0 + pos);
    }
}

void hide_LCD_cursor(void) {
    write_LCD(COMMAND, 0x0C);
}

void show_LCD_cursor(void) {
    write_LCD(COMMAND, 0x0F);
}

void clear_LCD(void) {
    write_LCD(COMMAND, 0x01);
    hardware_delay(10);
}

void LCD_print(char* str, unsigned char pos) {
    unsigned char i=0;

    while(str[i] != '\0') {
        set_LCD_cursor(pos);
        write_LCD(DATA, str[i]);
        i++;
        pos++;
    }
}
