/*
 * gps.h
 *
 *  Created on: Apr 19, 2023
 *      Author: merri
 */

#ifndef GPS_H_
#define GPS_H_

#include <serial.h>

int parse_GPGGA_string(struct data_queue* str, char* UTC, char* coords, char* elevation);
unsigned long UTC_seconds(char* UTC_str);
unsigned int coords_to_APRS_payload(char* payload, char* coords, char* elev, char symbol);
void coords_to_display(char* coords, char* lat, char* lon);

#endif /* GPS_H_ */
