#ifndef BBS_H_
#define BBS_H_

#include <msp430.h>

#define CRC_PASS 0
#define CRC_FAIL 1

#define MAX_PAYLOAD 128
#define MSG_STACK_SIZE 16

struct message {
    char callsigns[8][10];
    unsigned char num_callsigns;

    char payload[MAX_PAYLOAD];
    unsigned char payload_len;

    char crc;
};


void print_message_packet(struct message* msg);
struct message demod_AX_25_packet_to_msg(char* NRZI_bytes, unsigned int len);
void send_message(struct message* msg);

/* stack operations */
void push_message(struct message msg);
struct message pop_message(unsigned int index);
unsigned int message_stack_size(void);
struct message peek_message_stack(unsigned int index);

#endif /* BBS_H_ */
