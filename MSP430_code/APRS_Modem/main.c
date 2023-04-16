#include <msp430.h> 

#include <serial.h>
#include <dsp.h>
#include <math.h>
#include <packet.h>

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

	    len = generate_AX_25_packet_bytes(output, "APRS", "W6NXP", "\0", "Hello World!");

	    for(i=0;i<len;i++) {
	        print_hex(output[i]);
	        putchar(' ');
	    }
	    putchars("\n\r");

	    flip_bit_order(output, len);

	    crc = crc_16(output, len);

        output[len] = (crc >> 8) & 0xFF;
        len++;
        output[len] = (crc) & 0xFF;
        len++;

        for(i=0;i<len;i++) {
            print_hex(output[i]);
            putchar(' ');
        }
        putchars("\n\r");

	    print_hex((crc >> 8) & 0xFF);
	    print_hex((crc) & 0xFF);
	    putchars("\n\r");
	}
	
	return 0;
}
