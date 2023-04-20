/*
 * gps.h
 *
 *  Created on: Apr 19, 2023
 *      Author: merri
 */

#ifndef GPS_H_
#define GPS_H_

#include <serial.h>

//parse string and spit out location information
int parse_GPGGA_string(struct data_queue* str, char* UTC, char* coords, char* elevation);

#endif /* GPS_H_ */
