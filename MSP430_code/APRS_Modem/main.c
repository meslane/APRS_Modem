#include <msp430.h> 

#include <serial.h>
#include <dsp.h>
#include <math.h>

/**
 * main.c
 */
int main(void)
{
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

    char queue_array[32] = {0};

    struct data_queue q;
    q.data = queue_array;
    q.head = 0;
    q.tail = 0;
    q.MAX_SIZE = 32;

    unsigned int i;
	for(;;) {
	    push(&q, 'a');
        push(&q, 'b');
        push(&q, 'c');
        print_dec(q.tail, 3);
        putchars("\n\r");
        print_dec(q.head, 3);
        putchars("\n\r");
	    putchar(pop(&q));
	    putchar(pop(&q));
	    putchar(pop(&q));
	    putchars("\n\r");
	}
	
	return 0;
}
