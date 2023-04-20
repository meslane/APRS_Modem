#include <msp430.h> 

#include <serial.h>
#include <dsp.h>
#include <math.h>
#include <packet.h>
#include <gps.h>

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
	init_UART_UCA1(115200); //terminal output
	init_UART_UCA0(9600); //GPS module

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

    char c;
    char GPS_str[256];
    struct data_queue GPS_queue = {.data = GPS_str,
                                  .head = 0,
                                  .tail = 0,
                                  .MAX_SIZE = 256};

    char UTC_str[16];
    char coord_str[32];
    char elev_str[16];
    int fix_status;

	for(;;) {
	    /*
	    for (i=0;i<400;i++) {
	        packet[i] = 0x00;
	    }

	    putchars("Ready to transmit...\n\r");
	    waitchar();

	    //location format is: ddmm.mm/dddmm.mm
	    //should show a school icon on top of the belltower
	    len = make_AX_25_packet(packet, "APRS", "W6NXP", "WIDE1-1,WIDE2-1", "=3357.40N/11718.69WK", 63, 63); //create packet

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
        */
	    while(queue_len(&PORTA0_RX_queue) > 0) {
	        c = pop(&PORTA0_RX_queue);
	        //putchar(c);
	        push(&GPS_queue, c);

	        if (c == '\n') { //end of data string
	            fix_status = parse_GPGGA_string(&GPS_queue, UTC_str, coord_str, elev_str);
	            clear_queue(&GPS_queue);
	            if (fix_status != -1) {
	                putchars("UTC: ");
                    putchars(UTC_str);
                    putchars("\n\r");
                    putchars("Coords: ");
                    putchars(coord_str);
                    putchars("\n\r");
                    putchars("Elevation: ");
                    putchars(elev_str);
                    putchars("\n\r");

                    if (fix_status == 0) {
                        putchars("NO GPS LOCK\n\r");
                    } else {
                        putchars("GPS LOCKED\n\r");
                    }
	            }
	        }
	    }
	}
	
	return 0;
}
