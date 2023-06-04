#ifndef BBS_H_
#define BBS_H_

#define CRC_PASS 0
#define CRC_FAIL 1

#define MAX_PAYLOAD 128

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

#endif /* BBS_H_ */
