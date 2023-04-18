/*
 * packet.c
 *
 *  Created on: Apr 14, 2023
 *      Author: merri
 */
#include <msp430.h>
#include <packet.h>
#include <serial.h>
#include <math.h>

const unsigned int crc16_LUT[] = {
        0x0000, 0x1021, 0x2042, 0x3063, 0x4084, 0x50A5, 0x60C6, 0x70E7, 0x8108, 0x9129, 0xA14A, 0xB16B, 0xC18C, 0xD1AD, 0xE1CE, 0xF1EF,
        0x1231, 0x0210, 0x3273, 0x2252, 0x52B5, 0x4294, 0x72F7, 0x62D6, 0x9339, 0x8318, 0xB37B, 0xA35A, 0xD3BD, 0xC39C, 0xF3FF, 0xE3DE,
        0x2462, 0x3443, 0x0420, 0x1401, 0x64E6, 0x74C7, 0x44A4, 0x5485, 0xA56A, 0xB54B, 0x8528, 0x9509, 0xE5EE, 0xF5CF, 0xC5AC, 0xD58D,
        0x3653, 0x2672, 0x1611, 0x0630, 0x76D7, 0x66F6, 0x5695, 0x46B4, 0xB75B, 0xA77A, 0x9719, 0x8738, 0xF7DF, 0xE7FE, 0xD79D, 0xC7BC,
        0x48C4, 0x58E5, 0x6886, 0x78A7, 0x0840, 0x1861, 0x2802, 0x3823, 0xC9CC, 0xD9ED, 0xE98E, 0xF9AF, 0x8948, 0x9969, 0xA90A, 0xB92B,
        0x5AF5, 0x4AD4, 0x7AB7, 0x6A96, 0x1A71, 0x0A50, 0x3A33, 0x2A12, 0xDBFD, 0xCBDC, 0xFBBF, 0xEB9E, 0x9B79, 0x8B58, 0xBB3B, 0xAB1A,
        0x6CA6, 0x7C87, 0x4CE4, 0x5CC5, 0x2C22, 0x3C03, 0x0C60, 0x1C41, 0xEDAE, 0xFD8F, 0xCDEC, 0xDDCD, 0xAD2A, 0xBD0B, 0x8D68, 0x9D49,
        0x7E97, 0x6EB6, 0x5ED5, 0x4EF4, 0x3E13, 0x2E32, 0x1E51, 0x0E70, 0xFF9F, 0xEFBE, 0xDFDD, 0xCFFC, 0xBF1B, 0xAF3A, 0x9F59, 0x8F78,
        0x9188, 0x81A9, 0xB1CA, 0xA1EB, 0xD10C, 0xC12D, 0xF14E, 0xE16F, 0x1080, 0x00A1, 0x30C2, 0x20E3, 0x5004, 0x4025, 0x7046, 0x6067,
        0x83B9, 0x9398, 0xA3FB, 0xB3DA, 0xC33D, 0xD31C, 0xE37F, 0xF35E, 0x02B1, 0x1290, 0x22F3, 0x32D2, 0x4235, 0x5214, 0x6277, 0x7256,
        0xB5EA, 0xA5CB, 0x95A8, 0x8589, 0xF56E, 0xE54F, 0xD52C, 0xC50D, 0x34E2, 0x24C3, 0x14A0, 0x0481, 0x7466, 0x6447, 0x5424, 0x4405,
        0xA7DB, 0xB7FA, 0x8799, 0x97B8, 0xE75F, 0xF77E, 0xC71D, 0xD73C, 0x26D3, 0x36F2, 0x0691, 0x16B0, 0x6657, 0x7676, 0x4615, 0x5634,
        0xD94C, 0xC96D, 0xF90E, 0xE92F, 0x99C8, 0x89E9, 0xB98A, 0xA9AB, 0x5844, 0x4865, 0x7806, 0x6827, 0x18C0, 0x08E1, 0x3882, 0x28A3,
        0xCB7D, 0xDB5C, 0xEB3F, 0xFB1E, 0x8BF9, 0x9BD8, 0xABBB, 0xBB9A, 0x4A75, 0x5A54, 0x6A37, 0x7A16, 0x0AF1, 0x1AD0, 0x2AB3, 0x3A92,
        0xFD2E, 0xED0F, 0xDD6C, 0xCD4D, 0xBDAA, 0xAD8B, 0x9DE8, 0x8DC9, 0x7C26, 0x6C07, 0x5C64, 0x4C45, 0x3CA2, 0x2C83, 0x1CE0, 0x0CC1,
        0xEF1F, 0xFF3E, 0xCF5D, 0xDF7C, 0xAF9B, 0xBFBA, 0x8FD9, 0x9FF8, 0x6E17, 0x7E36, 0x4E55, 0x5E74, 0x2E93, 0x3EB2, 0x0ED1, 0x1EF0
};

void push_bit(struct bitstream* s, char bit) {
    if ((bit & 0x01) == 1) {
        s->bytes[s->byte_pointer] |= (0x01 << s->bit_pointer);
    } else {
        s->bytes[s->byte_pointer] &= ~(0x01 << s->bit_pointer);
    }

    if (s->bit_pointer == 0) { //move to next byte if at the end of this one
        s->bit_pointer = 7;
        s->byte_pointer++;
    } else {
        s->bit_pointer--;
    }
}

void push_byte(struct bitstream* s, char byte) {
    int index;

    for(index=7;index>=0;index--) {
        push_bit(s, (byte >> index) & 0x01);
    }
}

char pop_bit(struct bitstream* s) {
    if (s->bit_pointer == 7) { //move to prior byte if at the start of this one
        s->bit_pointer = 0;
        s->byte_pointer--;
    } else {
        s->bit_pointer++;
    }

    return (s->bytes[s->byte_pointer] >> s->bit_pointer) & 0x01;
}

char peek_bit(struct bitstream* s, unsigned int index) {
    return (s->bytes[index/8] >> (7 - (index % 8))) & 0x01;
}

void set_stream_bit(struct bitstream* s, unsigned int index, char bit) {
    if ((bit & 0x01) == 1) {
        s->bytes[index/8] |= (0x01 << (7 -(index % 8)));
    } else {
        s->bytes[index/8] &= ~(0x01 << (7 -(index % 8)));
    }
}

void clear_bitstream(struct bitstream* s) {
    s->byte_pointer = 0;
    s->bit_pointer = 7;
}

void print_bitstream_bytes(struct bitstream* s) {
    unsigned int i;
    for(i=0;i<=s->byte_pointer;i++) {
        print_hex(s->bytes[i]);
        putchar(' ');
    }

    putchars("\n\r");
}

//return number of bits in stream
unsigned int get_len(struct bitstream* s) {
    return (s->byte_pointer * 8) + (7 - s->bit_pointer);
}

void array_to_bitstream(struct bitstream* output, char* input, unsigned int len) {
    unsigned int i;

    clear_bitstream(output);

    for (i=0;i<len;i++) {
        push_byte(output, input[i]);
    }
}

unsigned int bitstream_to_array(char* output, struct bitstream* input) {
    unsigned int i = 0;

    while (i<=input->byte_pointer) {
        output[i] = input->bytes[i];
        i++;
    }

    return i;
}

//====
char is_digit(char c) {
    if (c >= 0x30 && c <= 0x39) {
        return 1;
    } else {
        return 0;
    }
}

char ascii_to_num(char c) {
    if (c >= 0x30 && c <= 0x39) {
        return (c - 0x30);
    } else {
        return 0;
    }
}

void print_packet(char* packet, unsigned int len) {
    unsigned int i;

    for(i=0;i<len;i++) {
        print_hex(packet[i]);
        putchar(' ');
    }
    putchars("\n\r");
}


//for testing
void print_packet_bits(char* packet, unsigned int len) {
    unsigned int i;
    unsigned int j;

   for(i=0;i<len;i++) {
       for(j=0;j<8;j++) {
           putchar(((packet[i] >> (7-j)) & 0x01) + 0x30);
           putchar(' ');
       }
   }
   putchars("\n\r");
}

//convert an address string in the style of aprs.fi to the proper format for sending in a packet
void str_to_AX_25_addr(char* output, char* str) {
    char c;
    char d[2];

    unsigned char i = 0;
    unsigned char j;

    unsigned char SSID;

    c = str[i];
    while (c != '\0' && c != '-' && i < 6) { //check for dash or end of string if no dash
        output[i] = (c << 1); //must left shift by one
        i++;
        c = str[i];
    }

    for (j=i;j<6;j++) {
        output[j] = 0x40; //pad with spaces
    }

    if (c == '-') { //if there is an SSID
        i++;
        d[0] = str[i];
        i++;
        d[1] = str[i];

        if (is_digit(d[1]) && d[0] == '1') { //if there are two digits
            SSID = 10 + ascii_to_num(d[1]);
        } else {
            SSID = ascii_to_num(d[0]);
        }
    } else {
        SSID = 0;
    }

    output[6] = 0x60 | ((SSID & 0x0F) << 1);
}

void insert_bytes(char* dest, char* src, unsigned int index, unsigned int len) {
    unsigned int i;

    for(i=0;i<len;i++) {
        dest[index + i] = src[i];
    }
}

unsigned int generate_AX_25_packet_bytes(char* output, char* dest, char* src, char* digipeaters, char* payload) {
    unsigned int index = 0;
    unsigned int i;
    unsigned int j;
    unsigned char k;
    char temp[7];
    char repeater_string[8] = {'\0'};

    //insert destination
    str_to_AX_25_addr(temp, dest);
    insert_bytes(output, temp, 0, 7);
    index += 7;

    //insert source
    str_to_AX_25_addr(temp, src);
    insert_bytes(output, temp, index, 7);
    index += 7;

    i = 0;
    while(digipeaters[i] != '\0') { //append addresses until end of string
        j = 0;
        while(digipeaters[i] != ',' && digipeaters[i] != '\0') {
            repeater_string[j] = digipeaters[i];
            i++;
            j++;
        }

        str_to_AX_25_addr(temp, repeater_string);
        insert_bytes(output, temp, index, 7);
        index += 7;

        if (digipeaters[i] == ',') { //advance pointer and clear repeater string if not end of string
            i++;

            for (k=0;k<8;k++) {
                repeater_string[k] = '\0';
            }

        }
    }

    output[index - 1] |= 0x01; //insert last address bit to indicate end of payload

    //append control and PID
    output[index] = 0x03;
    index++;
    output[index] = 0xF0;
    index++;

    //append payload
    i=0;
    while(payload[i] != '\0') {
        output[index] = payload[i];
        index++;
        i++;
    }

    return index;
}

//invert bit order per octet as is expected by AX.25
void flip_bit_order(char* data, unsigned int len) {
    unsigned int i;
    unsigned char j;
    char temp;

    for(i=0;i<len;i++) {
        temp = data[i];
        data[i] = 0x00;
        for(j=0;j<8;j++) {
            data[i] |= ((temp >> (7-j)) & 0x01) << j;
        }
    }
}

//calculate the 16 bit CRC of a given sequence
//adapted from: https://stackoverflow.com/a/25256043
unsigned int crc_16(char *buf, unsigned int len) {
    const unsigned int poly = 0x1021;
    unsigned int crc = 0xFFFF;

    while (len--) {
        crc ^= *buf++ << 8;
        crc = crc & 0x8000 ? (crc << 1) ^ poly : crc << 1;
        crc = crc & 0x8000 ? (crc << 1) ^ poly : crc << 1;
        crc = crc & 0x8000 ? (crc << 1) ^ poly : crc << 1;
        crc = crc & 0x8000 ? (crc << 1) ^ poly : crc << 1;
        crc = crc & 0x8000 ? (crc << 1) ^ poly : crc << 1;
        crc = crc & 0x8000 ? (crc << 1) ^ poly : crc << 1;
        crc = crc & 0x8000 ? (crc << 1) ^ poly : crc << 1;
        crc = crc & 0x8000 ? (crc << 1) ^ poly : crc << 1;
    }

    return crc & 0xFFFF;
}

unsigned int crc_16_alt(char* data, unsigned int len) {
    unsigned int crc = 0xFFFF;
    unsigned int table_index;

    unsigned int i;

    for (i=0;i<len;i++) {
        table_index = (data[i] ^ (crc >> 8)) & 0x00FF;
        crc = crc16_LUT[table_index] ^ (crc << 8);
    }

    crc = ((crc ^ 0xFFFF) & 0xFFFF);

    return (crc & 0xFFFF);
}


//add a zero after every fifth bit
void stuff_bitstream(struct bitstream* output, struct bitstream* input) {
    unsigned int i;
    unsigned char ones = 0;
    char temp;

    clear_bitstream(output);

    for (i=0;i<get_len(input);i++) {
        temp = peek_bit(input, i);
        if (temp == 1) {
            ones++;
        } else {
            ones = 0;
        }

        push_bit(output, temp);

        if (ones == 5) {
            push_bit(output, 0); //pad with zero if there are 5 consecutive ones
            ones = 0;
        }
    }
}

//TODO: change to return the bit index of the last bit
//use this data to determine where the end flag should start
unsigned int stuff_bits(char* output, char* input, unsigned int len) {
    unsigned int input_byte_index = 0;
    unsigned char input_bit_index = 7;

    unsigned int output_byte_index = 0;
    unsigned char output_bit_index = 7;

    unsigned char ones = 0;

    char bit;

    while (input_byte_index < len) { //stuff data
        bit = ((input[input_byte_index] >> input_bit_index) & 0x01);
        output[output_byte_index] |= (bit << output_bit_index);

        if (bit == 1) {
            ones++;
        } else  {
            ones = 0;
        }

        if (ones == 5) {
            if (output_bit_index == 0) {
                output_bit_index = 7;
                output_byte_index++;
            } else {
                output_bit_index--;
            }
        }

        if (input_bit_index == 0) {
            input_bit_index = 7;
            input_byte_index++;
        } else {
            input_bit_index--;
        }

        if (output_bit_index == 0) {
            output_bit_index = 7;
            output_byte_index++;
        } else {
            output_bit_index--;
        }
    }

    return (output_byte_index * 8) + (7 - output_bit_index); //return number of bits after stuffing
}

unsigned char NRZ_to_NRZI(char* packet, unsigned int len) {
    unsigned char current = 0x01;

    unsigned int i;
    unsigned char j;

    char bit;

    for (i=0;i<len;i++) {
        for (j=0;j<8;j++) {
            bit = (packet[i] >> (7-j)) & 0x01;

            packet[i] &= ~(0x01 << (7-j)); //clear bit first

            if (bit == 1) {
                packet[i] |= (current << (7-j));
            } else {
                packet[i] |= ((current ^ 0x01) << (7-j));
                current ^= 0x01;
            }
        }
    }

    return current;
}

void bitstream_NRZ_to_NRZI(struct bitstream* s) {
    char current = 0x01; //starting level of 1

    unsigned int i;

    for (i=0;i<get_len(s);i++) {
        if (peek_bit(s, i) == 1) { //1 = no level change
            set_stream_bit(s, i, current);
        } else { //0 = level change
            current ^= 1; //flip current
            set_stream_bit(s, i, current);
        }
    }
}

unsigned int add_flags(char* output, char* input, unsigned int input_len, unsigned int num_start_flags, unsigned int num_end_flags, unsigned char NRZI_bit, unsigned char end_bit_index) {
    unsigned int output_ptr = 0;
    unsigned int i;
    unsigned char flag_ptr = 7;


    for (i=0;i<num_start_flags;i++) { //append start flags
        output[output_ptr] = 0x01;
        output_ptr++;
    }

    for (i=0;i<input_len;i++) { //copy data
        output[output_ptr] = input[i];
        output_ptr++;
    }

    if (end_bit_index != 7) {
        output_ptr--;
    }

    //if the last bit of the sequence is a zero, end flag should be 0x7F
    //if the last bit is a one, the end flag should be 0x01
    //currently this causes demodulation to fail for some packets because the end flag is not always appended correctly
    for (i=0;i<(num_end_flags * 8);i++) {
        /*
        if ((NRZI_bit & 0x01) == 1) {
            output[output_ptr] = 0x01;
        } else {
            output[output_ptr] = 0xFE;
        }
        */
        //output[output_ptr] = 0x01;
        //output_ptr++;

        if ((NRZI_bit & 0x01) == 1) {
            if (flag_ptr == 0) {
                output[output_ptr] |= (0x01 << end_bit_index);
            }
        } else {
            if (flag_ptr != 0) {
                output[output_ptr] |= (0x01 << end_bit_index);
            }
        }

        output[output_ptr] |= (0x01 << end_bit_index);

        if (end_bit_index == 0) {
            end_bit_index = 7;
            output_ptr++;
            output[output_ptr] = 0;
        } else {
            end_bit_index--;
        }

        flag_ptr = (flag_ptr > 0) ? (flag_ptr - 1) : 7;

    }

    return output_ptr;
}

void bitstream_add_flags(struct bitstream* output, struct bitstream* input, unsigned char num_start_flags, unsigned char num_end_flags) {
    unsigned int i;
    char end_flag;

    clear_bitstream(output);

    for (i=0;i<num_start_flags;i++) {
        push_byte(output, 0x01); //prepend start flags
    }

    for (i=0;i<get_len(input);i++) {
        push_bit(output, peek_bit(input, i)); //add data from original packet
    }

    //append end flags
    if (peek_bit(input, get_len(input)-1) == 1) { //0x01 if last bit is 1
        end_flag = 0x01;
    } else { //0xFE if last bit is 0
        end_flag = 0xFE;
    }

    for (i=0;i<num_end_flags;i++) {
        push_byte(output, end_flag);
    }
}

unsigned int make_AX_25_packet(char* output, char* dest, char* src, char* digipeaters, char* payload, unsigned int num_start_flags, unsigned int num_end_flags) {
    unsigned int len;
    unsigned int crc;

    char bits_array[400] = {0};
    char bits_array2[400] = {0};

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

    len = generate_AX_25_packet_bytes(output, dest, src, digipeaters, payload); //make raw packet bytes

    //print_packet(temp_packet, len);

    flip_bit_order(output, len); //flip bit order of non-CRC bytes

    crc = ~crc_16(output, len); //generate CRC

    output[len] = (crc >> 8) & 0xFF;
    len++;
    output[len] = (crc) & 0xFF;
    len++;

    array_to_bitstream(&bits, output, len);

    stuff_bitstream(&bits2, &bits);
    bitstream_NRZ_to_NRZI(&bits2);

    bitstream_add_flags(&bits, &bits2, num_start_flags, num_end_flags);

    len = bitstream_to_array(output, &bits);

    /*
    num_bits = stuff_bits(output, temp_packet, len);
    len = (num_bits / 8) + 1;
    bit_index = 7 - (num_bits % 8); //get number of left over bits after stuffing

    print_hex(bit_index);
    putchars("\n\r");

    NRZI_bit = NRZ_to_NRZI(output, len);

    len = add_flags(temp_packet, output, len, num_start_flags, num_end_flags, NRZI_bit, bit_index);

    insert_bytes(output, temp_packet, 0, len);
    */

    return len;
}
