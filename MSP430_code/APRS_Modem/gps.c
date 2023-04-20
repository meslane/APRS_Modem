/*
 * gps.c
 *
 *  Created on: Apr 19, 2023
 *      Author: merri
 */

#include <gps.h>
#include <serial.h>

int parse_GPGGA_string(struct data_queue* s, char* UTC, char* coords, char* elevation) {
    int position_fix = -1; //return -1 of not a GPGGA string
    char c;
    unsigned int i;
    unsigned char commas = 0;
    char temp_str[6];

    for(i=0;i<6;i++) {
        temp_str[i] = pop(s);
    }

    if (streq(temp_str, "$GNGGA", 6) == 0) { //exit if not expected data
        return -1;
    }

    /* UTC */
    pop(s); //get rid of comma

    c = '\0';
    i = 0;
    while (c != ',') { //append UTC
        c = pop(s);
        UTC[i] = c;
        i++;
    }
    UTC[i - 1] = '\0'; //null-terminate

    /* coords */
    commas = 0;
    i = 0;
    while (commas < 4) {
        c = pop(s);

        if (c == ',') {
            commas++;
        } else {
            coords[i] = c;
            i++;
        }
    }
    coords[i] = '\0';

    /* position fix */
    position_fix = pop(s) - 0x30; //convert position fix to int

    commas = 0;
    while (commas < 3) {
        c = pop(s);
        if (c == ',') {
            commas++;
        }
    }

    /* elevation*/
    i = 0;
    c = '\0';
    while (c != ',') {
        c = pop(s);
        elevation[i] = c;
        i++;
    }
    elevation[i - 1] = '\0';

    return position_fix;
}
