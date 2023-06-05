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

struct TicTacToe {
    unsigned int P1_state; //9-bit board state
    unsigned int P2_state;

    char P1_call[10];
    char P2_call[10];

    char board[51];

    char player_turn; //1 == P1, 2 == P2
    char finished; //game finished flag
};

void print_message_packet(struct message* msg);
struct message demod_AX_25_packet_to_msg(char* NRZI_bytes, unsigned int len);
void send_message(struct message* msg);

/* stack operations */
void push_message(struct message msg);
struct message pop_message(unsigned int index);
unsigned int message_stack_size(void);
struct message peek_message_stack(unsigned int index);

/* tic-tac-toe things*/
void init_board(struct TicTacToe* game);
char process_move(struct TicTacToe* game, char* movestring, char player);
void update_game_board(struct TicTacToe* game);
char detect_end_condition(struct TicTacToe* game);

#endif /* BBS_H_ */
