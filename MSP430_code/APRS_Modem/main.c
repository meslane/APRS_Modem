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
    //enable_DSP_timer();

    char stream_bits[32] = {0};

    struct bitstream b ={
                         .bits = stream_bits,
                         .pointer = 0
    };

    char bytes[4] = {0};

    char output[256] = {0};
    char stuffed[256] = {0};
    char packet[350] = {0};
    unsigned int len;
    unsigned int crc;

    unsigned int i;
    //char c = '\0';
	for(;;) {
	    /*
	    if (tx_queue_empty == 1) {
	        disable_DSP_timer();
	        push_string(&symbol_queue, "Hello");

	        tx_queue_empty = 0;
	        for (i=0; i<10000; i++){}
	        enable_DSP_timer();
	    }
	    */

	    /*
	    do {
	        c = waitchar();
	        push(&symbol_queue, c);
	        putchar(c);
	    } while (c != '\r');

	    putchars("\n\r");

	    enable_DSP_timer();

	    while (tx_queue_empty == 0); //wait till empty
	    disable_DSP_timer();

	    tx_queue_empty = 0;
	    */

	    /*
	    append_bitstring(&b, "01100010");
	    bitstream_to_bytes(&b, bytes);

	    for (i=0;i<4;i++) {
	        putchar(bytes[i]);
	    }
	    putchars("\n\r");

	    b.pointer = 0;
	    */
	    /*
	    putchars("Normal generation:\n\r");

	    len = generate_AX_25_packet_bytes(output, "APRS", "W6NXP", "\0", "Hello World!");

	    print_packet(output, len);

	    flip_bit_order(output, len);

	    crc = crc_16(output, len);

        output[len] = (crc >> 8) & 0xFF;
        len++;
        output[len] = (crc) & 0xFF;
        len++;

        print_packet(output, len);

        len = stuff_bits(stuffed, output, len);

        print_packet(stuffed, len);

        putchars("Packet Function:\n\r");
        */
        /*
        len = make_AX_25_packet(output, "APRS", "W6NXP", "\0", "Hello World!");

        print_packet(output, len);
        */

	    /*
        len = generate_AX_25_packet_bytes(packet, "APRS", "W6NXP", "\0", "Hello World!"); //make raw packet bytes

        print_packet(packet, len);

        flip_bit_order(packet, len); //flip bit order of non-CRC bytes

        print_packet(packet, len);

        crc = crc_16(packet, len); //generate CRC

        packet[len] = (crc >> 8) & 0xFF;
        len++;
        packet[len] = (crc) & 0xFF;
        len++;

        print_packet(packet, len);

        len = stuff_bits(output, packet, len);

        print_packet(output, len);
        */
	    len = make_AX_25_packet(packet, "APRS", "W6NXP", "\0", "Hello World!"); //create packet
	    print_packet(packet, len);

	    push_packet(&symbol_queue, packet, len);

	    enable_DSP_timer();
        while (tx_queue_empty == 0); //wait till empty
        disable_DSP_timer();
        tx_queue_empty = 0;

        hardware_delay(20000);
	}
	
	return 0;
}
