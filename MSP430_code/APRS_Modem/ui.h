/*
 * ui.h
 *
 *  Created on: Apr 24, 2023
 *      Author: merri
 */

#ifndef UI_H_
#define UI_H_

#define FRAM_WRITE_DISABLE 0x00
#define FRAM_WRITE_ENABLE 0x01

#define MENU_SIZE 8
#define NUM_SYMBOLS 13

enum encoder_dir{NONE, CW, CCW, BUTTON};
enum LCD_input{DATA, COMMAND};
enum UI_state{UI_START, INFO, MENU, PERIOD, JITTER, DEST, CALL, REPEAT, MESSAGE, TX_SETTING};
enum beacon_state{BEACON_START, NO_LOCK, LOCK, START_TX, TX};
enum cursor_mode{MOVE, MODIFY};

//extern enum encoder_dir encoder_state;

void hardware_delay(unsigned int d);
void software_delay(unsigned long long d);

void init_encoder(void);
void init_LCD(void);
void write_LCD(enum LCD_input type, unsigned char data);
void set_LCD_cursor(unsigned char pos);
void hide_LCD_cursor(void);
void show_LCD_cursor(void);
void clear_LCD(void);
void LCD_print(char* str, unsigned char pos);

void enable_FRAM_write(const char enable);

enum encoder_dir get_encoder_state(void);

char modify_value(unsigned int* val, unsigned char digits, enum encoder_dir encoder_state, enum cursor_mode* mode, char* cursor_ptr);
char modify_call(char* callsign, enum encoder_dir encoder_state, enum cursor_mode* mode, char* cursor_ptr);
unsigned int get_next_jitter_period(unsigned int period, unsigned int jitter_setting);


enum UI_state UI_tick(enum UI_state state);

#endif /* UI_H_ */
