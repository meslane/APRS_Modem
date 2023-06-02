#include <msp430.h> 

#include <serial.h>
#include <dsp.h>
#include <math.h>
#include <packet.h>
#include <gps.h>
#include <ui.h>
#include <bbs.h>

char message[400];
char output[400];

struct message msg;

char* BBS_CALL = "BBS-1";

enum RX_state{RX_START, RX_UNLOCKED, RX_START_FLAG, RX_MESSAGE, RX_END_FLAG, RX_DECODE, RX_RESET};
enum RX_state RX_tick(enum RX_state state) {
    static long flag_queue = 0xFFFFFFFF;
    static unsigned char bit_index = 0;
    static char byte = 0x01; //data byte grabbed from routine
    static unsigned int message_index = 0;

    /* transitions */
    switch(state) {
    case RX_START:
        state = RX_UNLOCKED;
        break;
    case RX_UNLOCKED:
        if (flag_queue == 0x01010101) { //consistent start flag
            //flag_queue = 0xFFFFFFFF; //reset for next cycle
            state = RX_START_FLAG;
            putchars("LOCK\n\r");
        }
        break;
    case RX_START_FLAG:
        if (bit_index == 0 && byte != 0x01) { //if no longer getting start flags
            state = RX_MESSAGE;
            putchars("MSG\n\r");
        }
        break;
    case RX_MESSAGE:
        if ((flag_queue & 0x000000FF) == 0x01 || (flag_queue & 0x000000FF) == 0x7F) { //detect end flag
            if (bit_index == 0 && (flag_queue & 0x000000FF) == 0x7F) {
                message[message_index] = 0x00;
                message_index++; //add extra byte
            }

            state = RX_END_FLAG;
            putchars("END\n\r");
        } else if (message_index >= 400 || (bit_index == 0 && byte == 0x00)) { //if overflow message buffer or bad packet data
            state = RX_RESET;
            putchars("FAILED\n\r");
        }
        break;
    case RX_END_FLAG:
        if (num_ones(flag_queue, 8) != 1 && num_ones(flag_queue, 8) != 7) { //if end flags are over
            state = RX_DECODE; //go to decode now that end flags are done
            putchars("UNLOCK\n\r");
        }
        break;
    case RX_DECODE:
        state = RX_RESET;
        break;
    case RX_RESET:
        state = RX_UNLOCKED;
        break;
    default:
        state = RX_START;
        break;
    }

    /* actions */
    switch(state) {
    case RX_UNLOCKED:
        if (rx_ready == 1) { //if PLL has triggered a bit sample
            rx_ready = 0;

            flag_queue = ((flag_queue << 1) & 0xFFFFFFFE) | rx_bit; //shift flag queue and add RX bit to end
        }
        break;
    case RX_START_FLAG:
    case RX_MESSAGE:
    case RX_END_FLAG:
        if (rx_ready == 1) { //if PLL has triggered a bit sample
            rx_ready = 0;

            if (bit_index == 0) {
                if (state == RX_MESSAGE) {
                    message[message_index] = byte;
                    message_index++;
                }

                byte = 0;
            }

            byte |= (rx_bit << (7 - bit_index));

            bit_index = (bit_index < 7) ? bit_index + 1 : 0; //increment if less than 7, otherwise reset

            flag_queue = ((flag_queue << 1) & 0xFFFFFFFE) | rx_bit; //do flag queue
        }
        break;
    case RX_DECODE:
        msg = demod_AX_25_packet_to_msg(message, message_index);
        print_message_packet(&msg);
        break;
    case RX_RESET:
        message_index = 0; //reset for next pass
        bit_index = 0;
        byte = 0x01; //so it doesn't immediately go to message
        break;
    default:
        break;
    }

    return state;
}

enum BBS_state{BBS_START, BBS_RX, BBS_PROCESS, BBS_TX};
enum BBS_state BBS_tick(enum BBS_state state) {
    static enum RX_state rx_state = RX_START;

    unsigned int pkt_len;
    unsigned long long i;
    char packet[400];

    /* transitions */
    switch (state) {
    case BBS_START:
        init_DSP_timer(DSP_RX);
        enable_DSP_timer();
        state = BBS_RX;
        break;
    case BBS_RX:
        if (rx_state == RX_DECODE) { //if a message has been gotten, process it
            disable_DSP_timer(); //pause DSP
            state = BBS_PROCESS;
        }
        break;
    case BBS_PROCESS:
        init_DSP_timer(DSP_TX);
        state = BBS_TX; //TEMP, change later
        break;
    case BBS_TX:
        init_DSP_timer(DSP_RX);
        enable_DSP_timer();
        state = BBS_RX;
        break;
    default:
        state = BBS_START;
        break;
    }

    /* actions */
    switch (state) {
    case BBS_START:
        break;
    case BBS_RX:
        rx_state = RX_tick(rx_state); //increment RX state machine in RX mode
        break;
    case BBS_PROCESS:
        break;
    case BBS_TX:
        pkt_len = make_AX_25_packet(packet, msg.callsigns[1], BBS_CALL, msg.callsigns[2], msg.payload, 64, 32);
        push_packet(&symbol_queue, packet, pkt_len);
        PTT_on();

        for (i=0;i<20000;i++) {
           __no_operation(); //delay to let radio key up
        }

        enable_DSP_timer();

        while(tx_queue_empty != 1);

        disable_DSP_timer();

        for (i=0;i<20000;i++) {
           __no_operation(); //delay to let radio key up
        }
        PTT_off();

        tx_queue_empty = 0;

        for (i=0;i<240000;i++) {
           __no_operation(); //delay
        }
        break;
    default:
        break;
    }

    return state;
}

int main(void) {
    WDTCTL = WDTPW | WDTHOLD;	//stop watchdog timer
    PM5CTL0 &= ~LOCKLPM5; //disable high-impedance GPIO mode
    __bis_SR_register(GIE); //enable interrupts

    Software_Trim();
    init_clock();
    init_UART_UCA1(115200); //terminal output
    //init_UART_UCA0(9600); //GPS module

    //set P1.5 to ADC input
    P1SEL0 |= (1 << 5);
    P1SEL1 |= (1 << 5);
    init_ADC(5); //init for channel #5

    P1DIR |= (0x01 << 4);
    P1DIR |= (0x01 << 1);
    P1DIR |= (0x01 << 3);

    enum BBS_state bbs_state = BBS_START;

    unsigned long long tick = 0;

    init_resistor_DAC();
    init_PTT();

    putchars("\n\r");
    for(;;) {

        bbs_state = BBS_tick(bbs_state);

        tick++;
    }

    return 0;
}
