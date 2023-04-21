#include <msp430.h> 

#include <serial.h>
#include <dsp.h>
#include <math.h>
#include <packet.h>
#include <gps.h>

#define BEACON_INTERVAL 30

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

enum beacon_state{START, NO_LOCK, LOCK, START_TX, TX};

/* state machine function */
enum beacon_state beacon_tick(enum beacon_state state) {
    static int GPS_lock = 0;
    static int TX_ongoing = 0;
    static unsigned long long curr_UTC_secs = 0;
    static unsigned long long prev_UTC_secs = 0;

    char c;
    static char GPS_str[256];
    static struct data_queue GPS_queue = {.data = GPS_str,
                                  .head = 0,
                                  .tail = 0,
                                  .MAX_SIZE = 256};

    char packet[400] = {0};
    unsigned int pkt_len;
    unsigned int i;

    static char UTC_str[16];
    static char coord_str[32];
    static char elev_str[16];
    static char payload[128];
    static int fix_status;

    //transitions
    switch(state) {
    case START:
        state = NO_LOCK;
        break;
    case NO_LOCK:
        if (GPS_lock > 0) {
            state = LOCK;
            putchars("GPS LOCK ACQUIRED\n\r");
        }
        break;
    case LOCK:
        if (GPS_lock < 1) {
            state = NO_LOCK;
            putchars("GPS LOCK LOST\n\r");
        }
        else if ((curr_UTC_secs - prev_UTC_secs) >= BEACON_INTERVAL || (prev_UTC_secs > curr_UTC_secs)) {
            state = START_TX;
            TX_ongoing = 1;
        }
        break;
    case START_TX:
        state = TX;
        break;
    case TX:
        if (TX_ongoing == 0) {
            state = LOCK;
        }
        break;
    }

    //actions
    switch(state) {
    case LOCK:
    case NO_LOCK:
        while(queue_len(&PORTA0_RX_queue) > 0) { //grab data from GPS
            c = pop(&PORTA0_RX_queue);
            push(&GPS_queue, c);

            if (c == '\n') { //end of data string
                fix_status = parse_GPGGA_string(&GPS_queue, UTC_str, coord_str, elev_str);
                clear_queue(&GPS_queue);
                if (fix_status != -1) { //if correct string
                    GPS_lock = fix_status;
                    curr_UTC_secs = UTC_seconds(UTC_str);

                    putchars("UTC: ");
                    putchars(UTC_str);
                    putchars(" ");
                    putchars("Coords: ");
                    putchars(coord_str);
                    putchars(" ");
                    putchars("Elevation: ");
                    putchars(elev_str);
                    putchars("\n\r");
                }
            }
        }
        break;
    case START_TX:
        prev_UTC_secs = curr_UTC_secs;

        for (i=0;i<400;i++) { //clear packet
            packet[i] = 0x00;
        }
        coords_to_APRS_payload(payload, coord_str, elev_str, 'K');

        putchars("Transmitting payload: ");
        putchars(payload);
        putchars("\n\r");

        pkt_len = make_AX_25_packet(packet, "APRS", "W6NXP", "WIDE1-1,WIDE2-1", payload, 63, 63);
        push_packet(&symbol_queue, packet, pkt_len);
        PTT_on(); //key up
        enable_DSP_timer(); //start transmission
        break;
    case TX:
        if (tx_queue_empty == 1) { //wait till empty
            disable_DSP_timer();
            tx_queue_empty = 0;
            PTT_off();
            TX_ongoing = 0;

            putchars("Transmission Complete\n\r");
        }
        break;
    default:
        break;
    }

    return state;
}


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

    enum beacon_state state = START;
	for(;;) {
	    state = beacon_tick(state);
	}
	
	return 0;
}
