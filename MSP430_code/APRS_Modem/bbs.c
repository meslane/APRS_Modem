#include <msp430.h>
#include <bbs.h>
#include <packet.h>
#include <dsp.h>
#include <serial.h>
#include <ui.h>

#pragma PERSISTENT(message_stack)
struct message message_stack[MSG_STACK_SIZE] = {{.num_callsigns = 0,
                                                 .payload_len = 0,
                                                 .crc = CRC_FAIL}};

#pragma PERSISTENT(message_stack_ptr)
unsigned int message_stack_ptr = 0;

void print_message_packet(struct message* msg) {
    unsigned int i;
    unsigned int j;

    for (i = 0; i < msg->num_callsigns; i++) {
        for (j = 0; j < 7; j++) {
            if (msg->callsigns[i][j] >= 32 && msg->callsigns[i][j] < 128) {
                putchar(msg->callsigns[i][j]);
            }
        }
        putchar('|');
    }

    for (i = 0; i < msg->payload_len; i++) {
        if (msg->payload[i] >= 32 && msg->payload[i] < 128) {
            putchar(msg->payload[i]);
        }
    }

    putchar('|');

    if (msg->crc == CRC_PASS) {
        putchars("CRC PASSED");
    } else {
        putchars("CRC FAILED");
    }

    putchars("\n\r");
}

struct message demod_AX_25_packet_to_msg(char* NRZI_bytes, unsigned int len) {
    unsigned int i;
    unsigned int in_pointer = 0;
    unsigned int out_pointer = 0;
    unsigned char call_pointer = 0;
    char last;
    unsigned char number;

    char bits_array[400] = {0};
    char bits_array2[400] = {0};

    char temp[400];

    unsigned int crc;
    unsigned int packet_crc;
    unsigned int temp_crc;

    struct bitstream bits = {
        .bytes = bits_array,
        .bit_pointer = 7,
        .byte_pointer = 0
    };

    struct bitstream bits2 = {
        .bytes = bits_array2,
        .bit_pointer = 7,
        .byte_pointer = 0
    };

    struct message output_message = {
        .num_callsigns = 0,
        .payload_len = 0,
        .crc = CRC_FAIL
    };

    if (len < 17) { //invalid packet
        output_message.crc = CRC_FAIL;
        return output_message;
    }

    array_to_bitstream(&bits, NRZI_bytes, len); //len is in bytes

    //convert back to standard encoding
    bitstream_NRZI_to_NRZ(&bits);
    unstuff_bitstream(&bits2, &bits);

    len = bitstream_to_array(temp, &bits2);
    len--; //remove extra byte

    crc = ~crc_16(temp, len - 2); //calculate CRC from data (but not the CRC itself)

    //flip bits
    temp_crc = crc;
    crc = 0x0000;
    for(i=0;i<16;i++) {
        crc |= ((temp_crc >> (15-i)) & 0x01) << i;
    }

    flip_bit_order(temp, len);

    packet_crc = (((int)temp[len-1] << 8) & 0xFF00) | temp[len-2];

    if (crc == packet_crc) { //check against recovered CRC
        output_message.crc = CRC_PASS;
        P1OUT |= (0x01 << 1); //DEBUG
    } else {
        output_message.crc = CRC_FAIL;
        P1OUT &= ~(0x01 << 1); //DEBUG
    }

    in_pointer = 0;
    out_pointer = 0;
    call_pointer = 0;

    //extract callsigns
    last = 0;
    while (last == 0 && (in_pointer < (len - 7))) { //make sure we don't violate any array bounds
        out_pointer = 0;
        for (i=0;i<7;i++) {
            if (i < 6) {
                if ((temp[in_pointer] >> 1) > 0x20) { //append ascii
                    output_message.callsigns[call_pointer][out_pointer] = (temp[in_pointer] >> 1); //append if there is info
                    out_pointer++;
                }
            } else { //i == 6 (get number if any)
                if (temp[in_pointer] & 0x01 == 0x01) { //if last callsign
                    last = 1;
                }
                number = (temp[in_pointer] >> 1) & 0x0F;

                if (number > 0) {
                   output_message.callsigns[call_pointer][out_pointer] = '-'; //append hyphen
                   out_pointer++;

                   if (number > 9) {
                       output_message.callsigns[call_pointer][out_pointer] = '1';
                       out_pointer++;
                   }

                   output_message.callsigns[call_pointer][out_pointer] = (number % 10) + 0x30; //ascii offset
                   out_pointer++;
                }

            }
            in_pointer++;
            output_message.callsigns[call_pointer][out_pointer] = '\0';
        }

        call_pointer++;
    }
    output_message.num_callsigns = call_pointer;

    out_pointer = 0;

    in_pointer += 2; //skip control and ID

    //extract packet body
    while (in_pointer < (len - 2) && out_pointer < 63) { //get up to CRC
        output_message.payload[out_pointer] = temp[in_pointer];
        out_pointer++;
        in_pointer++;
    }
    output_message.payload[out_pointer] = '\0'; //null terminate
    out_pointer++;
    output_message.payload_len = out_pointer;

    return output_message;
}

void send_message(struct message* msg) {
    unsigned long long i;
    unsigned int pkt_len;
    char packet[400];

    if (msg->num_callsigns > 2) {
        pkt_len = make_AX_25_packet(packet, msg->callsigns[0], msg->callsigns[1], msg->callsigns[2], msg->payload, 64, 32);
    } else {
        pkt_len = make_AX_25_packet(packet, msg->callsigns[0], msg->callsigns[1], "\0", msg->payload, 64, 32);
    }

    push_packet(&symbol_queue, packet, pkt_len);

    for (i=0;i<500000;i++) {
       __no_operation(); //delay
    }

    PTT_on();
    for (i=0;i<20000;i++) {
       __no_operation(); //delay to let radio key up
    }

    enable_DSP_timer();
    while(tx_queue_empty != 1);
    disable_DSP_timer();

    for (i=0;i<20000;i++) {
       __no_operation(); //delay to let radio key down
    }
    PTT_off();

    tx_queue_empty = 0;

    for (i=0;i<240000;i++) {
       __no_operation(); //delay
    }
}


void push_message(struct message msg) {
    enable_FRAM_write(FRAM_WRITE_ENABLE);

    if (message_stack_ptr >= (MSG_STACK_SIZE - 1)) {
        pop_message(0); //delete oldest
    }

    message_stack[message_stack_ptr] = msg;
    message_stack_ptr++;

    enable_FRAM_write(FRAM_WRITE_DISABLE);
}

struct message pop_message(unsigned int index) {
    unsigned int i;
    struct message output_message = {
        .num_callsigns = 0,
        .payload_len = 0,
        .crc = CRC_FAIL
    };

    if (message_stack_ptr > index && message_stack_ptr > 0) { //make sure index is in bounds
        enable_FRAM_write(FRAM_WRITE_ENABLE);

        output_message = message_stack[index];

        for (i  = index; i < message_stack_ptr; i++) {
            message_stack[i] = message_stack[i+1]; //continually shift down
        }
        message_stack_ptr--;

        enable_FRAM_write(FRAM_WRITE_DISABLE);
    }

    return output_message;
}

unsigned int message_stack_size(void) {
    return message_stack_ptr;
}

struct message peek_message_stack(unsigned int index) {
    return message_stack[index];
}

/* tic tac toe */
void init_board(struct TicTacToe* game) {
    strcpy(game->board, "\n3 .|.|.\n  -|-|-\n2 .|.|.\n  -|-|-\n1 .|.|.\n\n  A B C\n", 51);
    game->P1_state = 0;
    game->P2_state = 0;
    game->P1_call[0] = '\0';
    game->P2_call[0] = '\0';
    game->finished = 0;
    game->player_turn = 1;
}


//do move, return 1 if valid, 0 if not
char process_move(struct TicTacToe* game, char* movestring, char player) {
    unsigned int move = 0x0000;

    //compute move bits
    switch(movestring[0]) {
    case 'A':
        move |= 0x01;
        break;
    case 'B':
        move |= 0x02;
        break;
    case 'C':
        move |= 0x04;
        break;
    default:
        break;
    }

    switch(movestring[1]) {
    case '1':
        break;
    case '2':
        move <<= 3;
        break;
    case '3':
        move <<= 6;
        break;
    default:
        break;
    }

    if (((game->P1_state & move) != move)  && ((game->P2_state & move) != move) && (game->player_turn == player)) { //if tile is not occupied and it is player's turn
        switch (player) {
        case 1:
            game->P1_state |= move;
            break;
        case 2:
            game->P2_state |= move;
            break;
        default:
            break;
        }
        return 1;
    } else {
        return 0;
    }
}

void update_game_board(struct TicTacToe* game) {
    unsigned int i;
    const unsigned char board_indices[9] = {35, 37, 39, 19, 21, 23, 3, 5, 7};

    for (i = 0; i < 9; i++) {
        if ((game->P1_state >> i) & 0x01) {
            game->board[board_indices[i]] = 'X';
        }
        else if ((game->P2_state >> i) & 0x01) {
            game->board[board_indices[i]] = 'O';
        }
    }
}

//0 = ongoing, 1 = P1 wins, 2 = P2 wins, 3 = cats game
char detect_end_condition(struct TicTacToe* game) {
    unsigned int i;
    const unsigned int win_states[8] = {0b000000111, 0b000111000, 0b111000000, 0b001001001, 0b010010010, 0b100100100, 0b100010001, 0b001010100};

    for (i = 0; i < 8; i++) {
        if ((game->P1_state & win_states[i]) == win_states[i]) { //P1 wins
            return 1;
        }
        else if ((game->P2_state & win_states[i]) == win_states[i]) { //P2 wins
            return 2;
        }
    }

    if ((game->P1_state | game->P2_state) == 0x1FF) { //if board is full
        return 3;
    }

    return 0; //TODO
}
