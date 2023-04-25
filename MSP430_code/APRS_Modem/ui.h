/*
 * ui.h
 *
 *  Created on: Apr 24, 2023
 *      Author: merri
 */

#ifndef UI_H_
#define UI_H_

enum encoder_dir{NONE, CW, CCW, BUTTON};
enum LCD_input{DATA, COMMAND};

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

enum encoder_dir get_encoder_state(void);

#endif /* UI_H_ */
