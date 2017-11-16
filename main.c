#include <stdio.h>
#include <stdint.h>
#include <string.h>

// Based on:
// https://en.wikipedia.org/wiki/Bit_stuffing
// http://docwiki.cisco.com/wiki/Synchronous_Data_Link_Control_and_Derivatives

#define START_BYTE 0x80
#define FLAG_BYTE 0x7E
//#define END_BYTE 0xFF

#define T_FLAG0 0
#define T_START 1
#define T_DATA  2
#define T_END   3
#define T_IDLE  4

#define R_WAITING_FOR_START 0
#define R_GOT_FIRST_FLAG    1
#define R_READING_DATA      2

int gpio_pin;

// messages should be:
// FLAG_BYTE START_BYTE (DATA...) FLAG_BYTE

const char *msg = ">this is ??>>?>a pretty long mes><<>sage <";

int t_state;
int t_pos;
int t_consecutive_ones;
int t_msg_idx;

void tx() {
    const int msg_len = strlen(msg);
    if (t_state == T_FLAG0) {
        gpio_pin = (FLAG_BYTE >> (7 - t_pos++)) & 0x1; 
        if (t_pos >= 8) {
            t_pos = 0;
            t_state = T_START;
        }
    }
    else if (t_state == T_START) {
        gpio_pin = (START_BYTE >> (7 - t_pos++)) & 0x1; 
        if (t_pos >= 8) {
            t_pos = 0;
            t_state = T_DATA;
            t_consecutive_ones = 0;
            t_msg_idx = 0;
        }
    }
    else if (t_state == T_DATA) {
        if (t_consecutive_ones >= 5) {
            gpio_pin = 0; // bit stuffing
            t_consecutive_ones = 0;
            return;
        }
        uint8_t bit = (msg[t_msg_idx] >> (7 - t_pos++)) & 0x1;
        if (bit) {
            t_consecutive_ones++;
        }
        else {
            t_consecutive_ones = 0;
        }
        gpio_pin = bit;//(msg[0] >> (7 - t_pos++)) & 0x1;
        if (t_pos >= 8) {
            t_pos = 0;
            if (t_msg_idx >= msg_len - 1) {
                t_state = T_END;
            }
            else {
                t_msg_idx++;
            }
        }
    }
    else if (t_state == T_END) {
        gpio_pin = (FLAG_BYTE >> (7 - t_pos++)) & 0x1;
        if (t_pos >= 8) {
            t_pos = 0;
            t_state = T_IDLE;
        }
    }
    else if (t_state == T_IDLE) {
        gpio_pin = 0;//(FLAG_BYTE >> (7 - t_pos++)) & 0x1;
        t_pos++;
        if (t_pos >= 13) {
            t_pos = 0;
            t_state = T_FLAG0;
        }
    }
}

uint8_t r_buf;
int r_state;
int r_pos;
int r_consecutive_ones;

void rx() {
    // handle bit stuffing
    if (gpio_pin) {
        r_consecutive_ones++;
    }
    else {
        if (r_consecutive_ones == 5) {
            // ignore 0 after 5 consecutive high bits
            r_consecutive_ones = 0;
            return;
        }
        r_consecutive_ones = 0;
    }
    r_buf <<= 1;
    r_buf |= gpio_pin;
    if (r_state == R_WAITING_FOR_START) {
        if (r_buf == FLAG_BYTE) {
            r_pos = 0;
            r_state = R_GOT_FIRST_FLAG;
            printf("got 1st flag\n");
        }
    }
    else if (r_state == R_GOT_FIRST_FLAG) {
        r_pos++;
        if (r_pos >= 8) {
            if (r_buf == FLAG_BYTE) {
                // got flag byte twice -- wait for start byte
                r_pos = 0;
                //r_state = R_GOT_FIRST_FLAG;
            }
            else if (r_buf == START_BYTE) {
                // read data in next byte(s)
                r_pos = 0;
                r_state = R_READING_DATA;
                printf("data:"); 
            }
            else {
                // unexpected byte -- we may be misaligned
                r_pos = 0;
                r_state = R_WAITING_FOR_START;
            }
        }
    }
    else if (r_state == R_READING_DATA) {
        r_pos++;
        if (r_pos >= 8) {
            if (r_buf == FLAG_BYTE) {
                // packet has ended
                r_pos = 0;
                r_state = R_WAITING_FOR_START;
                printf("\nended packet\n");
            }
            else {
                // got some data
                r_pos = 0;
                printf("%c", r_buf); 
            }
        }
    }
}

int main(void) {
    t_state = T_START;
    t_pos = 0;
    t_consecutive_ones = 0;

    r_state = R_WAITING_FOR_START;
    r_buf = 0;
    r_pos = 0;
    r_consecutive_ones = 0;

    while (1) {
        tx();
        rx();
    }
}
