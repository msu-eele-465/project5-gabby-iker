#include <msp430.h>

//------------------------------------------------------------------------------
// Definitions
//------------------------------------------------------------------------------
// Port 2
#define RS BIT0     // P2.0
#define EN BIT6     // P2.6

// Port 1
#define D4  BIT4     // P1.4
#define D5 BIT5     // P1.5
#define D6 BIT6     // P1.6
#define D7 BIT7     // P1.7

#define SLAVE_ADDR  0x48                    // Slave I2C Address

//------------------------------------------------------------------------------
// Variables
//------------------------------------------------------------------------------
volatile unsigned char receivedData = 0;    // Received data
char key_unlocked;                          // Store key press
unsigned char cursorState = 0;              // 0 = OFF, 1 = ON
unsigned char cursorBlinkState = 0;         // Store cursor blink state

//------------------------------------------------------------------------------
// Begin Slave I2C Initialization
//------------------------------------------------------------------------------
void slave_i2c_init(void)
{
    WDTCTL = WDTPW | WDTHOLD;  // Stop Watchdog Timer

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
//--End Slave I2C Init----------------------------------------------------------

//------------------------------------------------------------------------------
// Begin Pulse Enable
//------------------------------------------------------------------------------
void pulse_enable() {
    P2OUT |= EN;             // Set Enable to 1
    __delay_cycles(1000);    // Delay
    P2OUT &= ~EN;            // Set Enable to 0
    __delay_cycles(1000);    // Delay
}
//--End Pulse Enable------------------------------------------------------------

//------------------------------------------------------------------------------
// Begin Send Nibble
//------------------------------------------------------------------------------
void send_nibble(unsigned char nibble) {
    P1OUT &= ~(D4 | D5 | D6 | D7);      // Clear data bits
    P1OUT |= ((nibble & 0x0F) << 4);    // Load nibble into corresponding bits (P1.4 to P1.7)
    pulse_enable();                     // Pulse Enable to send data
}
//--End Send Nibble-------------------------------------------------------------

//------------------------------------------------------------------------------
// Begin Send Data
//------------------------------------------------------------------------------
void send_data(unsigned char data) {
    P2OUT |= RS;                // Data mode
    send_nibble(data >> 4);     // Send most significant 4 bits
    send_nibble(data & 0x0F);   // Send least significant 4 bits (corrected)
    __delay_cycles(4000);       // Delay to process data
}
//--End Send Data---------------------------------------------------------------

//------------------------------------------------------------------------------
// Begin Send Command
//------------------------------------------------------------------------------
void send_command(unsigned char cmd) {
    P2OUT &= ~RS;               // Command mode
    send_nibble(cmd >> 4);      // Send most significant 4 bits
    send_nibble(cmd);           // Send least significant 4 bits
    __delay_cycles(4000);       // Delay to ensure command is processed
}
//--End Send Command------------------------------------------------------------

//------------------------------------------------------------------------------
// Begin Toggle Cursor
//------------------------------------------------------------------------------
void toggle_cursor() {
    
    cursorState ^= 1;           // Toggle between 0 and 1 using XOR

    if (cursorState) {
        send_command(0x0E);     // Display ON, Cursor ON
    } else {
        send_command(0x0C);     // Display ON, Cursor OFF
    }
}
//--End Toggle Cursor-----------------------------------------------------------

//------------------------------------------------------------------------------
// Begin Toggle Blink Cursor
//------------------------------------------------------------------------------
void toggle_blink_cursor() {
    cursorBlinkState ^= 1;      // Toggle between 0 and 1 using XOR

    if (cursorBlinkState) {
        send_command(0x0F);     // Cursor ON with blink
    } else {
        send_command(0x0E);     // Cursor ON without blink
    }
}
//-- End Toggle Blink Cursor----------------------------------------------------

//------------------------------------------------------------------------------
// Begin LCD Initialization
//------------------------------------------------------------------------------
void lcd_init() {
    // Configure pins as output
    P1DIR |= D4 | D5 | D6 | D7;
    P2DIR |= RS | EN;

    // Clear outputs
    P1OUT &= ~(D4 | D5 | D6 | D7);
    P2OUT &= ~(RS | EN);
    __delay_cycles(50000);  // Startup delay
    send_nibble(0x03);      // LCD initialization
    __delay_cycles(5000);   // Delay
    send_nibble(0x03);      // Repeat initialization
    __delay_cycles(200);    // Delay
    send_nibble(0x03);      // Repeat initialization
    send_nibble(0x02);      // Set 4-bit mode

    send_command(0x28);     // Configure LCD: 4 bits, 2 lines, 5x8
    send_command(0x0C);     // Display ON, cursor OFF
    send_command(0x06);     // Automatic write mode
    send_command(0x01);     // Clear screen
    __delay_cycles(2000);   // Wait for screen to clear
}
//-- End LCD Init---------------------------------------------------------------

//------------------------------------------------------------------------------
// Begin LCD Set Cursor
//------------------------------------------------------------------------------
void lcd_set_cursor(unsigned char position) {
    send_command(0x80 | position);  // Set cursor address in DDRAM
}
//-- End LCD Set Cursor---------------------------------------------------------

//------------------------------------------------------------------------------
// Begin LCD Print
//------------------------------------------------------------------------------
void lcd_print(const char* str, unsigned char startPos) {
    lcd_set_cursor(startPos);
    while (*str) {
        send_data(*str++);
        startPos++;
        if (startPos == 0x10) startPos = 0x40;  // Auto jump to second line
    }
}
//-- End LCD Print--------------------------------------------------------------

//------------------------------------------------------------------------------
// Begin Display Output
//------------------------------------------------------------------------------
void display_output(char input)
{
    switch (input) 
    { 
        case 'C':
            toggle_cursor();
            break;
        case '9':
            toggle_blink_cursor();
            break;
        case '0':
            send_command(0x01);
            __delay_cycles(2000);
            lcd_print("STATIC", 0x00);
            lcd_set_cursor(0x4F);
            send_data('0');
            lcd_set_cursor(0x06);
            break;
        case '1':
            send_command(0x01);
            __delay_cycles(2000);
            lcd_print("TOGGLE", 0x00);
            lcd_set_cursor(0x4F);
            send_data('1');
            lcd_set_cursor(0x06);
            break;
        case '2':
            send_command(0x01);
            __delay_cycles(2000);
            lcd_print("UP COUNTER", 0x00);
            lcd_set_cursor(0x4F);
            send_data('2');
            lcd_set_cursor(0x0A);
            break;
        case '3':
            send_command(0x01);
            __delay_cycles(2000);
            lcd_print("IN AND OUT", 0x00);
            lcd_set_cursor(0x4F);
            send_data('3');
            lcd_set_cursor(0x0A);
            break;
        case '4':
            send_command(0x01);
            __delay_cycles(2000);
            lcd_print("DOWN COUNTER", 0x00);
            lcd_set_cursor(0x4F);
            send_data('4');
            lcd_set_cursor(0x0C);
            break;
        case '5':
            send_command(0x01);
            __delay_cycles(2000);
            lcd_print("ROTATE 1 LEFT", 0x00);
            lcd_set_cursor(0x4F);
            send_data('5');
            lcd_set_cursor(0x0D);
            break;
        case '6':
            send_command(0x01);
            __delay_cycles(2000);
            lcd_print("ROTATE 7 RIGHT", 0x00);
            lcd_set_cursor(0x4F);
            send_data('6');
            lcd_set_cursor(0x0E);
            break;
        case '7':
            send_command(0x01);
            __delay_cycles(2000);
            lcd_print("FILL LEFT", 0x00);
            lcd_set_cursor(0x4F);
            send_data('7');
            lcd_set_cursor(0x09);
            break;
        case 'D':
            send_command(0x01);
            break;

    }
}
//-- End Display Output---------------------------------------------------------

//------------------------------------------------------------------------------
// Begin Main
//------------------------------------------------------------------------------
int main(void) {
    lcd_init();                         // Initialize LCD
    slave_i2c_init();                   // Initialize the slave for I2C
    __bis_SR_register(LPM0_bits + GIE); // Enter LPM0, enable interrupts
    return 0;
        
    
}
//-- End Main-------------------------------------------------------------------

//------------------------------------------------------------------------------
// Begin Interrupt Service Routines
//------------------------------------------------------------------------------
#pragma vector = USCI_B0_VECTOR
__interrupt void USCI_B0_ISR(void)
{
    switch (__even_in_range(UCB0IV, USCI_I2C_UCTXIFG0))
    {
        case USCI_I2C_UCRXIFG0:         // Receive Interrupt
            key_unlocked = UCB0RXBUF;   // Read received data
            display_output(UCB0RXBUF);
            break;
        default: 
            break;
    }
}
//-- End Interrupt Service Routines --------------------------------------------
