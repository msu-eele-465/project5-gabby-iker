#include "intrinsics.h"
#include <msp430.h>
#include <stdbool.h>

#define ledOn 0xFF
#define ledOff 0
#define ledPattern01_init 0b10101010
#define ledPattern02_init 0
#define ledPattern03_init 0b00011000
#define ledPattern04_init 0xFF
#define ledPattern05_init 0b00000001
#define ledPattern06_init 0b01111111
#define ledPattern07_init 0b00000001

bool new_input_bool = true;
bool pattern3_out = true;
char key_cur;
char key_prev = '\0';
int pattern1_cur;
int pattern2_cur;
int pattern3_cur;
int pattern4_cur;
int pattern5_cur;
int pattern6_cur;
bool pattern1_start = false;
bool pattern2_start = false;
bool pattern3_start = false;
bool pattern4_start = false;
bool pattern5_start = false;
bool pattern6_start = false;
unsigned int timing_base = 32768;
int timing_adj;
unsigned char ledPattern_state;


//------------------------------------------------------------------------------
// Begin led initialization
//------------------------------------------------------------------------------
void init_led_bar()
{
    WDTCTL = WDTPW | WDTHOLD;  // Stop Watchdog Timer

    // Stop watchdog timer
    WDTCTL = WDTPW | WDTHOLD;   // Stop watchdog timer
    
    // Setup Ports
    P1OUT &= ~0xFF;             // Clear  Pin 3 to start
    P1DIR |= 0xFF;              // Config Pin 3 as output
    P2OUT &= ~0xFF;             // Clear  Pin 4 to start
    P2DIR |= 0xFF;              // Config Pin 4 as output

    // Setup Timer
    TB1CTL |= TBCLR;            // Clear timers and dividers
    TB1CTL |= TBSSEL__ACLK;     // Source = ACLK
    TB1CTL |= MC__UP;           // Mode = UP
    TB1CCR0 = timing_base;      // CCR0 = 32768 (1s overflow)
    // 8192 = 0.25 seconds

    // Setup Timer Compare IRQ
    TB1CCTL0 &= ~CCIFG;         //Clear CCR0 Flag
    TB1CCTL0 |= CCIE;           // Enable TB0 CCR0 Overflow IRQ
    __enable_interrupt();       // Enable Maskable IRQ

    // Disable the GPIO power-on default high-impedance mode to activate
    // previously configure port settings
    PM5CTL0 &= ~LOCKLPM5;
}
//--End LED Initialization------------------------------------------------------

//------------------------------------------------------------------------------
// Begin Display LED Patterns
//------------------------------------------------------------------------------
void display_led_pattern()
{
    P1OUT = ledPattern_state;
    P2OUT = ledPattern_state << 4 | ledPattern_state >> 7;
}
//--End Display LED Patterns----------------------------------------------------

//------------------------------------------------------------------------------
// Begin LED Patterns
//------------------------------------------------------------------------------
void led_patterns(char key_cur) 
{
    switch(key_cur)
    {
        case '0':           // Static state
            ledPattern_state = ledPattern01_init;
            break;
        case '1':           // Toggle
            TB1CCR0 = timing_base;
            if (new_input_bool) {
                if ((key_cur == key_prev | pattern1_start == false))
                {
                    ledPattern_state = ledPattern01_init;
                    pattern1_start = true;
                }
                else
                    ledPattern_state = pattern1_cur;
                new_input_bool = false;
            } else
                ledPattern_state = pattern1_cur = (ledPattern_state ^= 0xFF);
            break;
        case '2':             // Up counter
            TB1CCR0 = 0.5 * timing_base;
            if (new_input_bool) {
                if (key_cur == key_prev | pattern2_start == false)
                {
                    ledPattern_state = ledPattern02_init;
                    pattern2_start = true;
                }
                else
                    ledPattern_state = pattern2_cur;
                new_input_bool = false;
            } else
                ledPattern_state = pattern2_cur = (++ledPattern_state);
            break;
        case '3':             // in and out
            TB1CCR0 = 0.5 * timing_base;
            if (new_input_bool) {
                if (key_cur == key_prev | pattern3_start == false)
                {
                    ledPattern_state = ledPattern03_init;
                    pattern3_start = true;
                }
                else
                    ledPattern_state = pattern3_cur;
                new_input_bool = false;
            }
            else if ((pattern3_out == true & ledPattern_state != 0b10000001) | (pattern3_out == false & ledPattern_state == 0b00011000))  // out
            {
                ledPattern_state = pattern3_cur = (ledPattern_state = ~ledPattern_state & ((0xF0 & ledPattern_state << 1) | (0xF & ledPattern_state >> 1) | ledPattern_state << 7 | ledPattern_state >> 7));
                pattern3_out = true;
            }
            else if ((pattern3_out == false & ledPattern_state != 0b00011000) | (pattern3_out == true & ledPattern_state == 0b10000001))  // in
            {
                ledPattern_state = pattern3_cur = (ledPattern_state = ~ledPattern_state & ((0xF & ledPattern_state << 1) | (0xF0 & ledPattern_state >> 1) | ledPattern_state << 7 | ledPattern_state >> 7));
                pattern3_out = false;
            }
            break;
        case '4':             // down counter, extra credit
            TB1CCR0 = 0.25 * timing_base;
            if (new_input_bool) {
                if (key_cur == key_prev | pattern4_start == false)
                {
                    ledPattern_state = ledPattern04_init;
                    pattern4_start = true;
                }
                else
                    ledPattern_state = pattern4_cur;
                new_input_bool = false;
            } else
                ledPattern_state = pattern4_cur = (--ledPattern_state);
            break;
        case '5':             // rotate one left, extra credit
            TB1CCR0 = 1.5 * timing_base;
        if (new_input_bool) {
            if (key_cur == key_prev | pattern5_start == false)
                {
                    ledPattern_state = ledPattern05_init;
                    pattern5_start = true;
                }
            else
                ledPattern_state = pattern5_cur;
                new_input_bool = false;
            } else
                ledPattern_state = pattern5_cur = (ledPattern_state = ledPattern_state << 1 | ledPattern_state >> 7);
            break;
        case '6':             // rotate 7 right, extra credit
            TB1CCR0 = 0.5 * timing_base;
            if (new_input_bool) {
                if (key_cur == key_prev | pattern6_start == false)
                {
                    ledPattern_state = ledPattern06_init;
                    pattern6_start = true;
                }
                else
                    ledPattern_state = pattern6_cur;
                new_input_bool = false;
            } else
                ledPattern_state = pattern6_cur = (ledPattern_state = ledPattern_state >> 1 | ledPattern_state << 7);
            break;
        default:
            ledPattern_state = ledOff;
            break;
    }
    display_led_pattern();
}

void set_led_bar(char key_input)
{
    if (key_input == 'A') {
        if (timing_base - 8192 >= 8192) {
            TB1CCR0 = (timing_base -= 8192);
        }
    } else if (key_input == 'B') {
        if (timing_base + 8192 <= 0xFFFF - 8192) {
            TB1CCR0 = (timing_base += 8192);
        }
    } else {
        key_prev = key_cur;
        key_cur = key_input;
        new_input_bool = true;
        led_patterns(key_cur);
    }
}

void main(void)
{
    init_led_bar();
    while(true){}
}


//------------------------------------------------------------------------------
// Begin Interrupt Service Routines
//------------------------------------------------------------------------------
#pragma vector = TIMER1_B0_VECTOR
__interrupt void ISR_TB1_CCR0(void)
{
    led_patterns('5');
    TB1CCTL0 &= ~CCIFG;
}
//-- End Interrupt Service Routines --------------------------------------------
