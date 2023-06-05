#include <msp430.h> 

#include <serial.h>
#include <dsp.h>
#include <math.h>
#include <packet.h>
#include <gps.h>
#include <ui.h>
#include <bbs.h>

char message[MAX_PACKET];

struct message msg;
struct message outgoing_message;

#pragma PERSISTENT(temp_msg) //need this to avoid memory fuckery
struct message temp_msg = {.num_callsigns = 0,
                           .payload_len = 0,
                           .crc = CRC_FAIL};

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
        } else if (message_index >= MAX_PACKET || (bit_index == 0 && byte == 0x00)) { //if overflow message buffer or bad packet data
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
        P1OUT &= ~(0x01 << 2); //DEBUG

        if (rx_ready == 1) { //if PLL has triggered a bit sample
            rx_ready = 0;

            flag_queue = ((flag_queue << 1) & 0xFFFFFFFE) | rx_bit; //shift flag queue and add RX bit to end
        }
        break;
    case RX_START_FLAG:
    case RX_MESSAGE:
    case RX_END_FLAG:
        P1OUT |= (0x01 << 2); //DEBUG

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
        P1OUT |= (0x01 << 2); //DEBUG

        msg = demod_AX_25_packet_to_msg(message, message_index);
        print_message_packet(&msg);
        break;
    case RX_RESET:
        P1OUT |= (0x01 << 2); //DEBUG

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

    static char temp[128];
    unsigned int i;
    unsigned int j;
    static char num_temp[4];
    unsigned int stack_size;
    unsigned int msg_id;

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
        if (streq(msg.callsigns[0], "BBS-1", 5) || streq(msg.callsigns[2], "BBS-1", 5)) { //if message is made out to BBS or BBS is a repeater
            init_DSP_timer(DSP_TX);
            state = BBS_TX;
        } else {
            init_DSP_timer(DSP_RX);
            enable_DSP_timer();
            state = BBS_RX;
        }
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
        /* MAIN BBS work code */
        if (streq(msg.callsigns[0], "BBS-1", 5)) { //direct commands to the sever
            strcpy(outgoing_message.callsigns[0], msg.callsigns[1], 10); //move sender to recipient
            strcpy(outgoing_message.callsigns[1], BBS_CALL, 10);
            strcpy(outgoing_message.callsigns[2], msg.callsigns[2], 10);
            outgoing_message.num_callsigns = 3;

            if (streq(msg.payload, "!ping", 5)) {
                strcpy(outgoing_message.payload, "pong!", MAX_PAYLOAD);
            }
            else if (streq(msg.payload, "!help", 5)) {
                strcpy(outgoing_message.payload, "Commands: !ping, !help, !users, !messages, !message, !delete (!about [command] for more info)", MAX_PAYLOAD);
            }
            else if (streq(msg.payload, "!about ping", 11)) {
                strcpy(outgoing_message.payload, "!ping: pings the server, server responds with '!pong' if message was received", MAX_PAYLOAD);
            }
            else if (streq(msg.payload, "!about help", 11)) {
                strcpy(outgoing_message.payload, "!help: lists all commands", MAX_PAYLOAD);
            }
            else if (streq(msg.payload, "!about users", 12)) {
                strcpy(outgoing_message.payload, "!users: lists all users whose messages are currently stored on the server", MAX_PAYLOAD);
            }
            else if (streq(msg.payload, "!about messages", 15)) {
                strcpy(outgoing_message.payload, "!messages: returns the number of stored messages addressed to the user", MAX_PAYLOAD);
            }
            else if (streq(msg.payload, "!about message", 14)) {
                strcpy(outgoing_message.payload, "!message [number]: returns the requested message addressed to the user (if it exists)", MAX_PAYLOAD);
            }
            else if (streq(msg.payload, "!about delete", 13)) {
                strcpy(outgoing_message.payload, "!delete [number]: deletes the given message addressed to the user (do this after reading, oldest messages are auto-deleted)", MAX_PAYLOAD);
            }
            else if (streq(msg.payload, "!users", 6)) {
                temp[0] = '\0';
                strcat(temp, "Users:");

                for (i = 0; i < message_stack_size(); i++) {
                    if (instr(temp, peek_message_stack(i).callsigns[1]) == 0) {
                        strcat(temp, peek_message_stack(i).callsigns[1]); //append sender
                        strcat(temp, " "); //append space
                    }
                }

                strcpy(outgoing_message.payload, temp, MAX_PAYLOAD);
            }
            else if (streq(msg.payload, "!messages", 9)) {
                temp[0] = '\0';
                strcat(temp, "Message IDs stored for you = ");

                stack_size = message_stack_size();
                for (i = 0; i < stack_size; i++) {
                    enable_FRAM_write(FRAM_WRITE_ENABLE);
                    temp_msg = peek_message_stack(i);
                    enable_FRAM_write(FRAM_WRITE_DISABLE);

                    if (streq(temp_msg.callsigns[0], msg.callsigns[1], 9)) { //if addressed to user
                        int_to_str(num_temp, i, 2);
                        strcat(temp, num_temp);
                        strcat(temp, " ");
                    }
                }

                strcpy(outgoing_message.payload, temp, MAX_PAYLOAD);
            }
            else if (streq(msg.payload, "!message", 8)) {
                temp[0] = '\0';
                msg_id = str_to_int(&msg.payload[9]);

                stack_size = message_stack_size();
                if (msg_id >= stack_size) {
                    strcat(temp, "ERROR: message ID out of range");
                } else {
                    enable_FRAM_write(FRAM_WRITE_ENABLE);
                    temp_msg = peek_message_stack(i);
                    enable_FRAM_write(FRAM_WRITE_DISABLE);

                    if (streq(temp_msg.callsigns[0], msg.callsigns[1], 9)) {
                        strcat(temp, temp_msg.payload);

                        for (j = 0; j < temp_msg.num_callsigns; j++) {
                            strcpy(outgoing_message.callsigns[j], temp_msg.callsigns[j], 10);
                        }
                        outgoing_message.num_callsigns = temp_msg.num_callsigns;
                    } else {
                        strcat(temp, "ERROR: message is not addressed to you");
                    }
                }
                strcpy(outgoing_message.payload, temp, MAX_PAYLOAD);
            }
            else if (streq(msg.payload, "!delete", 7)) {
                temp[0] = '\0';
                msg_id = str_to_int(&msg.payload[8]);

                stack_size = message_stack_size();
                if (msg_id >= stack_size) {
                    strcat(temp, "ERROR: message ID out of range");
                } else {
                    enable_FRAM_write(FRAM_WRITE_ENABLE);
                    temp_msg = peek_message_stack(i);
                    enable_FRAM_write(FRAM_WRITE_DISABLE);

                    if (streq(temp_msg.callsigns[0], msg.callsigns[1], 9)) {
                        pop_message(msg_id);
                        strcat(temp, "Deleted message");
                    } else {
                        strcat(temp, "ERROR: message is not addressed to you");
                    }
                }
                strcpy(outgoing_message.payload, temp, MAX_PAYLOAD);
            }
            else {
                strcpy(outgoing_message.payload, "Invalid command (send '!help' for list)", MAX_PAYLOAD);
            }
        }
        else if (streq(msg.callsigns[2], "BBS-1", 5)) { //forwards to other users
            push_message(msg);

            strcpy(outgoing_message.callsigns[0], msg.callsigns[1], 10); //move sender to recipient
            strcpy(outgoing_message.callsigns[1], BBS_CALL, 10);
            strcpy(outgoing_message.callsigns[2], "\0", 1); //no repeaters
            outgoing_message.num_callsigns = 2;

            strcpy(outgoing_message.payload, "Saved message to be forwarded", MAX_PAYLOAD);
        }
        break;
    case BBS_TX:
        send_message(&outgoing_message);
        break;
    default:
        break;
    }

    return state;
}

int main(void) {
    WDTCTL = WDTPW | WDTHOLD;   //stop watchdog timer
    PM5CTL0 &= ~LOCKLPM5; //disable high-impedance GPIO mode
    __bis_SR_register(GIE); //enable interrupts

    Software_Trim();
    init_clock();
    init_UART_UCA1(115200); //terminal output

    //set P1.5 to ADC input
    P1SEL0 |= (1 << 5);
    P1SEL1 |= (1 << 5);
    init_ADC(5); //init for channel #5

    P1DIR |= (0x01 << 4);
    P1DIR |= (0x01 << 1);
    P1DIR |= (0x01 << 3);
    P1DIR |= (0x01 << 2);

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
