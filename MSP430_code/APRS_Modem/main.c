#include <msp430.h> 

#include <serial.h>
#include <dsp.h>
#include <math.h>
#include <packet.h>
#include <gps.h>
#include <ui.h>

#define FRAM_WRITE_DISABLE 0x00
#define FRAM_WRITE_ENABLE 0x01

#define MENU_SIZE 7
#define NUM_SYMBOLS 13

/* enable/disable writing to FRAM */
void enable_FRAM_write(const char enable) {
    switch(enable) {
    case FRAM_WRITE_ENABLE:
        SYSCFG0 = FRWPPW | DFWP; //enable FRAM write
        break;
    case FRAM_WRITE_DISABLE:
        SYSCFG0 = FRWPPW | PFWP | DFWP; //disable FRAM write
        break;
    default:
        break;
    }
}

enum beacon_state{BEACON_START, NO_LOCK, LOCK, START_TX, TX};
enum UI_state{UI_START, INFO, MENU, PERIOD, JITTER, DEST, CALL, REPEAT, MESSAGE};

enum cursor_mode{MOVE, MODIFY};

int GPS_lock = 0;
int TX_ongoing = 0;
char update_display = 0;

char UTC_str[16];
char coord_str[32];
char elev_str[16];

char lat[16];
char lon[16];

const char digipeater_settings[4][16] = {
    "WIDE2-1",
    "WIDE2-2",
    "WIDE1-1,WIDE2-1",
    "WIDE1-1,WIDE2-2"
};

char* digi;

const char map_symbol_strings[NUM_SYMBOLS][16] = {
    "School",
    "Person",
    "Bicycle",
    "Motorcycle",
    "Canoe",
    "Boat",
    "Car",
    "Truck",
    "Train",
    "Small Plane",
    "Large Plane",
    "Helicopter",
    "Balloon"
};

const char map_symbols[NUM_SYMBOLS] = {
    'K',
    '[',
    'b',
    '<',
    'C',
    'Y',
    '>',
    'u',
    '=',
    '\'',
    '^',
    'X',
    'O'
};

unsigned int current_interval;

/* save during power cycles */
#pragma PERSISTENT(BEACON_INTERVAL);
unsigned int BEACON_INTERVAL = 30;

#pragma PERSISTENT(JITTER_INTERVAL);
unsigned int JITTER_INTERVAL = 0;

#pragma PERSISTENT(digi_index)
unsigned char digi_index = 2;

#pragma PERSISTENT(symbol_index)
unsigned char symbol_index = 0;

#pragma PERSISTENT(call)
char call[10] = "W6NXP    ";

#pragma PERSISTENT(dest)
char dest[10] = "APRS     ";

char modify_value(unsigned int* val, unsigned char digits, enum encoder_dir encoder_state, enum cursor_mode* mode, char* cursor_ptr) {
    char update = 0;

    if (encoder_state == CW) {
        if (*mode == MOVE && *cursor_ptr < 15) {
            *cursor_ptr += 1;
            set_LCD_cursor(*cursor_ptr);
        } else if (*mode == MODIFY) { //modify
            enable_FRAM_write(FRAM_WRITE_ENABLE);
            *val += pow(10, (digits-1)-*cursor_ptr);
            enable_FRAM_write(FRAM_WRITE_DISABLE);
            update = 1;
        }
    }
    else if (encoder_state == CCW) {
        if (*mode == MOVE && *cursor_ptr > 0) {
            *cursor_ptr -= 1;
            set_LCD_cursor(*cursor_ptr);
        } else if (*mode == MODIFY) {
            enable_FRAM_write(FRAM_WRITE_ENABLE);
            *val -= pow(10, (digits-1)-*cursor_ptr);
            enable_FRAM_write(FRAM_WRITE_DISABLE);
            update = 1;
        }
    }
    else if (encoder_state == BUTTON && *cursor_ptr < digits) {
        if (*mode == MODIFY) {
            *mode = MOVE;
        } else if (*mode == MOVE) {
            *mode = MODIFY;
        }
        update = 1;
    }

    return update;
}

char modify_call(char* callsign, enum encoder_dir encoder_state, enum cursor_mode* mode, char* cursor_ptr) {
    char update = 0;

    if (encoder_state == CW) {
        if (*mode == MOVE && *cursor_ptr < 15) {
            *cursor_ptr += 1;
            set_LCD_cursor(*cursor_ptr);
        } else if (*mode == MODIFY) { //modify
            if (callsign[*cursor_ptr] < 0x5A) { //must be valid char
                enable_FRAM_write(FRAM_WRITE_ENABLE);
                callsign[*cursor_ptr]++;
                enable_FRAM_write(FRAM_WRITE_DISABLE);
            }
            update = 1;
        }
    }
    else if (encoder_state == CCW) {
        if (*mode == MOVE && *cursor_ptr > 0) {
            *cursor_ptr -= 1;
            set_LCD_cursor(*cursor_ptr);
        } else if (*mode == MODIFY) {
            if (callsign[*cursor_ptr] > 0x20) { //must be valid char
                enable_FRAM_write(FRAM_WRITE_ENABLE);
                callsign[*cursor_ptr]--;
                enable_FRAM_write(FRAM_WRITE_DISABLE);
            }
            update = 1;
        }
    }
    else if (encoder_state == BUTTON && *cursor_ptr < 9) {
        if (*mode == MODIFY) {
            *mode = MOVE;
        } else if (*mode == MOVE) {
            *mode = MODIFY;
        }
        update = 1;
    }

    return update;
}

//use ADC noise to add jitter to signal
unsigned int get_next_jitter_period(unsigned int period, unsigned int jitter_setting) {
    long long ADC_sum = 0;
    int sign = 0;
    unsigned int i;
    int jitter;

    for(i=0;i<10;i++) {
        ADC_sum += get_ADC_result();
    }

    for (i=0;i<4;i++) {
        sign += get_ADC_result();
    }

    if (sign % 2 == 0) { //determine if positive or negative
        sign = 1;
    } else {
        sign = -1;
    }

    jitter = sign * (ADC_sum % (jitter_setting + 1));

    if (jitter_setting < period) {
        return (period + jitter);
    } else {
        return period;
    }
}

/* state machine function */
enum beacon_state beacon_tick(enum beacon_state state, enum UI_state ui_state) {
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

    static char payload[128];
    static int fix_status;

    //transitions
    switch(state) {
    case BEACON_START:
        state = NO_LOCK;
        current_interval = BEACON_INTERVAL;
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
        else if (((curr_UTC_secs - prev_UTC_secs) >= current_interval || (prev_UTC_secs > curr_UTC_secs)) && ui_state == INFO) { //don't transmit if in menu
            current_interval = get_next_jitter_period(BEACON_INTERVAL,JITTER_INTERVAL);
            print_dec(current_interval, 4);
            putchars("\n\r");
            state = START_TX;
            TX_ongoing = 1;
        }
        break;
    case START_TX:
        state = TX;
        update_display = 1;
        break;
    case TX:
        if (TX_ongoing == 0) {
            state = LOCK;
            update_display = 1;
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

                    /*
                    putchars("UTC: ");
                    putchars(UTC_str);
                    putchars(" ");
                    putchars("Coords: ");
                    putchars(coord_str);
                    putchars(" ");
                    putchars("Elevation: ");
                    putchars(elev_str);
                    putchars("\n\r");
                    */

                    update_display = 1;
                }
            }
        }
        break;
    case START_TX:
        disable_UART_interrupt(PORTA0); //prevent GPS UART from interrupting sampling
        clear_queue(&GPS_queue);

        prev_UTC_secs = curr_UTC_secs;

        for (i=0;i<400;i++) { //clear packet
            packet[i] = 0x00;
        }
        coords_to_APRS_payload(payload, coord_str, elev_str, map_symbols[symbol_index]);

        putchars("Transmitting message: ");
        putchars(dest);
        putchars(" ");
        putchars(call);
        putchars(" ");
        putchars(digi);
        putchars(" ");
        putchars(payload);
        putchars("\n\r");

        //pkt_len = make_AX_25_packet(packet, "APRS", "W6NXP", "WIDE1-1,WIDE2-1", payload, 63, 63);
        pkt_len = make_AX_25_packet(packet, dest, call, digi, payload, 63, 63);
        push_packet(&symbol_queue, packet, pkt_len);
        PTT_on(); //key up
        enable_DSP_timer(); //start transmission
        break;
    case TX:
        if (tx_queue_empty == 1) { //wait till empty
            disable_DSP_timer();
            PTT_off();
            enable_UART_interrupt(PORTA0);
            tx_queue_empty = 0;
            TX_ongoing = 0;

            putchars("Transmission Complete\n\r");
        }
        break;
    default:
        break;
    }

    return state;
}

enum UI_state UI_tick(enum UI_state state) {
    static char menu_ptr = 0;
    static char cursor_ptr = 0;
    static enum cursor_mode mode = MOVE;

    char menu_place[4] = {0};
    enum encoder_dir encoder_state = get_encoder_state();
    char temp[16] = {0};
    int temp_int;

    //transitions
    switch (state) {
    case UI_START:
        digi = (char*)digipeater_settings[digi_index];
        state = INFO;
        break;
    case INFO:
        if (encoder_state == BUTTON) {
            state = MENU;
            menu_ptr = 0;
            update_display = 1;
        }
        break;
    case MENU:
        if (encoder_state == BUTTON) {
            switch(menu_ptr) {
            case 0:
                state = PERIOD;
                show_LCD_cursor();
                break;
            case 1:
                state = JITTER;
                show_LCD_cursor();
                break;
            case 2:
                state = DEST;
                show_LCD_cursor();
                break;
            case 3:
                state = CALL;
                show_LCD_cursor();
                break;
            case 4:
                state = REPEAT;
                //mode = MODIFY;
                cursor_ptr = 15;
                hide_LCD_cursor();
                break;
            case 5:
                state = MESSAGE;
                hide_LCD_cursor();
                break;
            case 6:
                state = INFO;
                hide_LCD_cursor();
                break;
            default:
                break;
            }
            update_display = 1;
        }
        break;
    case PERIOD:
    case JITTER:
    case DEST:
    case CALL:
    case REPEAT:
    case MESSAGE:
        if (encoder_state == BUTTON && cursor_ptr == 15) { //temp
               state = MENU;
               //menu_ptr = 0;
               update_display = 1;
               hide_LCD_cursor();
        }
        break;
    default:
        state = UI_START;
        break;
    }

    //actions
    switch (state) {
    case INFO:
        if (update_display == 1) {
            clear_LCD();

            if (GPS_lock == 0) {
                LCD_print("NO GPS LOCK", 0);
            } else {
                coords_to_display(coord_str, lat, lon);
                LCD_print(elev_str, 9);
                LCD_print(lat, 0);
                LCD_print(lon, 16);

                if (TX_ongoing == 1) {
                    LCD_print("TX", 30);
                }
            }
        }
        break;
    case MENU:
        if (encoder_state == CW && menu_ptr < (MENU_SIZE - 1)) {
            menu_ptr++;
            update_display = 1;
        }
        else if (encoder_state == CCW && menu_ptr > 0) {
            menu_ptr--;
            update_display = 1;
        }

        if (update_display == 1) {
            clear_LCD();

            switch (menu_ptr) {
            case 0:
                LCD_print("TX Period", 0);
                break;
            case 1:
                LCD_print("TX Jitter", 0);
                break;
            case 2:
                LCD_print("Dest Call", 0);
                break;
            case 3:
                LCD_print("Src Call", 0);
                break;
            case 4:
                LCD_print("Repeaters", 0);
                break;
            case 5:
                LCD_print("Map Symbol", 0);
                break;
            case 6:
                LCD_print("Go Back", 0);
                break;
            }

            LCD_print("Menu", 16);

            menu_place[0] = (menu_ptr + 1) + 0x30;
            menu_place[1] = '/';
            menu_place[2] = MENU_SIZE + 0x30;
            LCD_print(menu_place, 21);
        }
        break;
    case PERIOD:
        temp_int = modify_value(&BEACON_INTERVAL, 5, encoder_state, &mode, &cursor_ptr);
        update_display += temp_int;

        if (temp_int != 0) { //if modified
            current_interval = get_next_jitter_period(BEACON_INTERVAL,JITTER_INTERVAL);
        }

        if (update_display > 0) {
            clear_LCD();
            int_to_str(temp, BEACON_INTERVAL, 5);
            LCD_print(temp, 0);
            LCD_print("^", 15);
            set_LCD_cursor(cursor_ptr);
        }
        break;
    case JITTER:
        temp_int += modify_value(&JITTER_INTERVAL, 5, encoder_state, &mode, &cursor_ptr);
        update_display += temp_int;

        if (temp_int != 0) {
            current_interval = get_next_jitter_period(BEACON_INTERVAL,JITTER_INTERVAL);
        }

        if (update_display > 0) { //if modified
            clear_LCD();
            int_to_str(temp, JITTER_INTERVAL, 5);
            LCD_print(temp, 0);
            LCD_print("^", 15);
            set_LCD_cursor(cursor_ptr);
        }
        break;
    case DEST:
        update_display += modify_call(dest, encoder_state, &mode, &cursor_ptr);

        if (update_display > 0) {
            clear_LCD();
            LCD_print(dest, 0);
            LCD_print("^", 15);
            set_LCD_cursor(cursor_ptr);
        }
        break;
    case CALL:
        update_display += modify_call(call, encoder_state, &mode, &cursor_ptr);

        if (update_display > 0) {
            clear_LCD();
            LCD_print(call, 0);
            LCD_print("^", 15);
            set_LCD_cursor(cursor_ptr);
        }
        break;
    case REPEAT:
        cursor_ptr=15;
        if (encoder_state == CW) {
            if (digi_index < 3) {
                enable_FRAM_write(FRAM_WRITE_ENABLE);
                digi = (char*)digipeater_settings[++digi_index];
                enable_FRAM_write(FRAM_WRITE_DISABLE);
                update_display = 1;
            }
        }
        else if (encoder_state == CCW) {
            if (digi_index > 0) {
                enable_FRAM_write(FRAM_WRITE_ENABLE);
                digi = (char*)digipeater_settings[--digi_index];
                enable_FRAM_write(FRAM_WRITE_DISABLE);
                update_display = 1;
            }
        }

        if (update_display > 0) {
            clear_LCD();
            LCD_print(digi, 0);
            //LCD_print("^", 15);
            set_LCD_cursor(cursor_ptr);
        }
        break;
    case MESSAGE:
        cursor_ptr=15;

        cursor_ptr=15;
        if (encoder_state == CW) {
            if (symbol_index < (NUM_SYMBOLS - 1)) {
                enable_FRAM_write(FRAM_WRITE_ENABLE);
                symbol_index++;
                enable_FRAM_write(FRAM_WRITE_DISABLE);
                update_display = 1;
            }
        }
        else if (encoder_state == CCW) {
            if (symbol_index > 0) {
                enable_FRAM_write(FRAM_WRITE_ENABLE);
                symbol_index--;
                enable_FRAM_write(FRAM_WRITE_DISABLE);
                update_display = 1;
            }
        }

        if (update_display > 0) {
            clear_LCD();
            LCD_print((char*)map_symbol_strings[symbol_index], 0);
            //LCD_print("^", 15);
            set_LCD_cursor(cursor_ptr);
        }

        break;
    default:
        state = UI_START;
        hide_LCD_cursor();
        break;
    }

    update_display = 0;
    return state;
}

int main(void) {
    WDTCTL = WDTPW | WDTHOLD;	//stop watchdog timer
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
    init_encoder();
    init_LCD();

    hide_LCD_cursor();

    enum beacon_state gps_state = BEACON_START;
    enum UI_state ui_state = UI_START;
    unsigned long long tick = 0;
    for(;;) {
        gps_state = beacon_tick(gps_state, ui_state);
        ui_state = UI_tick(ui_state);

        tick++;
    }

    return 0;
}
