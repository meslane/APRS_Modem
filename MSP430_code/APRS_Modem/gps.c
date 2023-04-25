/*
 * gps.c
 *
 *  Created on: Apr 19, 2023
 *      Author: merri
 */

#include <gps.h>
#include <serial.h>
#include <math.h>

//parse string and spit out location information
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


//take a UTC timestamp string and return seconds elapsed
unsigned long UTC_seconds(char* UTC_str) {
    unsigned long secs = 0;
    unsigned long temp;

    temp = str_to_int(UTC_str); //returns in hhmmss format, must do some math on it to get what we want

    secs += (temp / 10000) * 3600; //hours -> secs
    secs += ((temp / 100) % 100) * 60; //mins -> secs
    secs += (temp % 100); //secs

    return secs;
}

//convert coords and elevation into a payload string
unsigned int coords_to_APRS_payload(char* payload, char* coords, char* elev, char symbol) {
    int i = 0;
    int j = 0;
    unsigned long long elevation;

    payload[i] = '=';
    i++;
    while (coords[j] != '\0') {
        if (coords[j] >= 0x40) { //move output pointer back two spaces if letter (to get rid of last two digits)
            i -= 3;
        }

        payload[i] = coords[j];

        if (coords[j] == 'N' || coords[j] == 'S') { //add spacer
            i++;
            payload[i] = '/';
        }

        j++;
        i++;
    }

    //add symbol
    payload[i] = symbol;
    i++;

    //add altitude
    payload[i] = '>';
    i++;
    payload[i] = '/';
    i++;
    payload[i] = 'A';
    i++;
    payload[i] = '=';
    i++;

    //APRS network expects elevation to be in feet, GPS gives it in meters
    elevation = (str_to_int(elev) * 3281)/1000;

    //convert back to string
    for (j = (6-1); j>=0; j--) { //APRS expects 6 digits
        payload[i] = (((elevation / pow(10,j)) % 10) + 48);
        i++;
    }

    payload[i] = '\0';
    i++;

    return i;
}

void coords_to_display(char* coords, char* lat, char* lon) {
    int i = 0;
    int j = 0;

    char* current_str = lat;

    while (coords[j] != '\0') {
        if (coords[j] >= 0x40) { //move output pointer back two spaces if letter (to get rid of last two digits)
            i -= 3;
        }

        current_str[i] = coords[j];

        if (coords[j] == 'N' || coords[j] == 'S') { //add spacer
            current_str[++i] = '\0';
            current_str = lon;
            i=0;
        }
        else if (coords[j] == 'W' || coords[j] == 'E'){
            current_str[++i] = '\0';
            break;
        }
        else {
            i++;
        }

        j++;
    }
}
