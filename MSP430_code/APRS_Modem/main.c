#include <msp430.h> 

#include <serial.h>
#include <dsp.h>
#include <math.h>
#include <packet.h>

/* delay ISR */
#pragma vector=TIMER2_B0_VECTOR
__interrupt void TIMER2_B0_VECTOR_ISR (void) {
    TB2CTL |= TBCLR;
    TB2CTL &= ~(0b11 << 4); //stop timer

    TB2CCTL0 &= ~CCIFG; //reset interrupt
    __bic_SR_register_on_exit(LPM3_bits); //exit sleep mode
}

/* slow delay clock, f = 32.768 kHz, p = 30.518us*/
void hardware_delay(unsigned int d) {
    TB2CCTL0 = CCIE; //interrupt mode
    TB2CCR0 = d; //trigger value
    TB2CTL = TBSSEL__ACLK | MC__UP | ID_0 | TBCLR; //slow clock, count up, no division, clear at start

    __bis_SR_register(LPM3_bits | GIE); //sleep until interrupt is triggered
}

/**
 * main.c
 */
int main(void) {
	WDTCTL = WDTPW | WDTHOLD;	// stop watchdog timer
	PM5CTL0 &= ~LOCKLPM5; //disable high-impedance GPIO mode
	__bis_SR_register(GIE); //enable interrupts

	Software_Trim();
	init_clock();
	init_UART_UCA1(115200);

	//set P1.5 to ADC input
    P1SEL0 |= (1 << 5);
    P1SEL1 |= (1 << 5);
    init_ADC(5); //init for channel #5

    init_resistor_DAC();
    init_DSP_timer();
    init_PTT();

    char packet[400] = {0};
    unsigned int len;
    unsigned int i;

	for(;;) {
	    for (i=0;i<400;i++) {
	        packet[i] = 0x00;
	    }

	    putchars("Ready to transmit...\n\r");
	    waitchar();

	    //location format is: ddmm.mm/dddmm.mm
	    //should show a school icon on top of the belltower
	    len = make_AX_25_packet(packet, "APRS", "W6NXP", "WIDE1-1,WIDE2-1", "=3358.40N/11719.69WK", 63, 63); //create packet

	    putchars("Packet Contents:\n\r");
	    print_packet(packet, len);

	    push_packet(&symbol_queue, packet, len);

	    PTT_on();
	    enable_DSP_timer();
        while (tx_queue_empty == 0); //wait till empty
        disable_DSP_timer();
        tx_queue_empty = 0;
        PTT_off();

        hardware_delay(10000);
	}
	
	return 0;
}
