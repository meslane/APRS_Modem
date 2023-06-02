#include <bbs.h>
#include <packet.h>
#include <dsp.h>
#include <serial.h>

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
    } else {
        output_message.crc = CRC_FAIL;
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

    pkt_len = make_AX_25_packet(packet, msg->callsigns[0], msg->callsigns[1], msg->callsigns[2], msg->payload, 64, 32);
    push_packet(&symbol_queue, packet, pkt_len);

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
