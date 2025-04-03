#include "intrinsics.h"
#include <msp430.h>
#include <stdbool.h>

//------------------------------------------------------------------------------
// Definitions
//------------------------------------------------------------------------------
#define ledOn 0xFF
#define ledOff 0
#define ledPattern01_init 0b10101010
#define ledPattern02_init 0
#define ledPattern03_init 0b00011000
#define ledPattern04_init 0xFF
#define ledPattern05_init 0b00000001
#define ledPattern06_init 0b01111111
#define ledPattern07_init 0b00000001
#define SLAVE_ADDR  0x68                    // Slave I2C Address

//------------------------------------------------------------------------------
// Variables
//------------------------------------------------------------------------------
bool new_input_bool = true;                 // Initialize new input to true
bool pattern3_out = true;                   // Start in/out pattern going out
char key_cur;                               // Store key press
char key_prev = '\0';                       // Initialize keypress to null
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
bool bool_set_led   = false;
unsigned int timing_base = 32768;           // 1 second
int timing_adj;                             // + or - 0.5 seconds
unsigned char ledPattern_state;             // Store LED pattern
volatile unsigned char receivedData = 0;    // Recieved data

//------------------------------------------------------------------------------
// Begin I2C initialization
//------------------------------------------------------------------------------
void slave_i2c_init(void)
{
    WDTCTL = WDTPW | WDTHOLD;           // Stop Watchdog Timer

    // Configure P1.2 as SDA and P1.3 as SCL
    P1SEL1 &= ~(BIT2 | BIT3);
    P1SEL0 |= BIT2 | BIT3;

    // Configure USCI_B0 as I2C Slave
    UCB0CTLW0 |= UCSWRST;               // Put eUSCI_B0 into software reset
    UCB0CTLW0 |= UCMODE_3;              // Select I2C slave mode
    UCB0I2COA0 = SLAVE_ADDR + UCOAEN;   // Set and enable first own address
    UCB0CTLW0 |= UCTXACK;               // Send ACKs

    PM5CTL0 &= ~LOCKLPM5;               // Disable low-power inhibit mode

    UCB0CTLW0 &= ~UCSWRST;              // Pull eUSCI_B0 out of software reset
    UCB0IE |= UCSTTIE + UCRXIE;         // Enable Start and RX interrupts

    __enable_interrupt();               // Enable Maskable IRQs
}
//--End I2C initializatoin------------------------------------------------------

//------------------------------------------------------------------------------
// Begin LED initialization
//------------------------------------------------------------------------------
void init_led_bar()
{
    WDTCTL = WDTPW | WDTHOLD;  // Stop Watchdog Timer
    
    // Setup Ports
    P1OUT &= ~0xFF;             // Clear  Pin 3 to start
    P1DIR |= 0xFF;              // Config Pin 3 as output
    P2OUT &= ~0xFF;             // Clear  Pin 4 to start
    P2DIR |= 0xFF;              // Config Pin 4 as output

    // Setup Timer
    TB1CTL |= TBCLR;            // Clear timers and dividers
    TB1CTL |= TBSSEL__ACLK;     // Source = ACLK
    TB1CTL |= MC__UP;           // Mode = UP
    TB1CCR0 = timing_base;      // CCR0 = default 1 second

    // Setup Timer Compare IRQ
    TB1CCTL0 &= ~CCIFG;         //Clear CCR0 Flag
    TB1CCTL0 |= CCIE;           // Enable TB0 CCR0 Overflow IRQ
    __enable_interrupt();       // Enable Maskable IRQ
    PM5CTL0 &= ~LOCKLPM5;
}
//--End LED Initialization------------------------------------------------------

//------------------------------------------------------------------------------
// Begin status indicator initialization
//------------------------------------------------------------------------------
void init_status_indicator()
{
    // Setup Timer
    TB0CTL |= TBCLR;            // Clear timers and dividers
    TB0CTL |= TBSSEL__ACLK;     // Source = ACLK
    TB0CTL |= MC__UP;           // Mode = UP
    TB0CCR0 = 32768;            // CCR0 = 32768 (1s overflow)
    __enable_interrupt();       // Enable Maskable IRQ
}
//--End Stat Ind Initialization-------------------------------------------------

//------------------------------------------------------------------------------
// Begin Display LED Patterns
//------------------------------------------------------------------------------
void display_led_pattern()
{
    P1OUT = ledPattern_state;
    P2OUT = (P2OUT & 0x3F) | ((ledPattern_state & 0x0C) << 4);
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
            TB1CCR0 = timing_base >> 1;
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
            TB1CCR0 = timing_base >> 1;
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
            TB1CCR0 = timing_base >> 2;
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
            TB1CCR0 = timing_base + (timing_base >> 1);
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
            TB1CCR0 = timing_base >> 1;
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
//--End LED Patterns------------------------------------------------------------

//------------------------------------------------------------------------------
// Begin Set LED Bar
//------------------------------------------------------------------------------
void set_led_bar(char key_input)
{
    if (key_input == 'C') {
        bool_set_led = true;
    } else if (key_input == 'A' || key_input == 'B') {
        bool_set_led = false;
    }
    if ((bool_set_led == true && (key_input >= '0' && key_input <= '6')) || key_input == 'D') {
        key_prev = key_cur;
        key_cur = key_input;
        new_input_bool = true;
        led_patterns(key_cur);
        bool_set_led = false;
    }
}
//--End Set LED Bar-------------------------------------------------------------

//------------------------------------------------------------------------------
// Begin Main
//------------------------------------------------------------------------------
int main(void)
{
    init_led_bar();
    init_status_indicator();
    slave_i2c_init();                   // Initialize the slave for I2C
    __bis_SR_register(LPM0_bits + GIE); // Enter LPM0, enable interrupts
    return 0;
}
//--End Main--------------------------------------------------------------------

//------------------------------------------------------------------------------
// Begin Interrupt Service Routines
//------------------------------------------------------------------------------
// Timer for pattern
#pragma vector = TIMER1_B0_VECTOR
__interrupt void ISR_TB1_CCR0(void)
{
    led_patterns(key_cur);
    TB1CCTL0 &= ~CCIFG;
}
//------------------------------------------------------------------------------
// Interrupt for Status LED (currently not working)
// ToDo: Fix status LED
#pragma vector = TIMER0_B0_VECTOR
__interrupt void ISR_TB0_CCR0(void)
{
    P2OUT &= ~BIT0;              // Turn off status indicator
    TB0CCTL0 &= ~CCIFG;
    TB0CCTL0 &= ~CCIE;          // Disable TB0 interrupt until next I2C receive
}
//------------------------------------------------------------------------------
// I2C Receive key press and set pattern
#pragma vector = USCI_B0_VECTOR
__interrupt void USCI_B0_ISR(void)
{
    new_input_bool = true;
    switch (__even_in_range(UCB0IV, USCI_I2C_UCTXIFG0))
    {
        case USCI_I2C_UCRXIFG0:         // Receive Interrupt
            set_led_bar(UCB0RXBUF);    // Read received data
            P2OUT |= BIT0;              // Turn on status indicator
            TB0CCTL0 &= ~CCIFG;         //Clear CCR0 Flag
            TB0CCTL0 |= CCIE;           // Enable TB0 interrupt
            break;
        default: 
            break;
    }
}
//-- End Interrupt Service Routines --------------------------------------------
