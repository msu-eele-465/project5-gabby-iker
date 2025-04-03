/*
 * EELE 465, Project 4
 * Gabby and Iker
 *
 * Target device: MSP430FR2355 Master
 */

//----------------------------------------------------------------------
// Headers
//----------------------------------------------------------------------
#include <msp430.h>
#include <stdbool.h>
#include <stdio.h>
#include "intrinsics.h"
#include "C:\Users\gabri\Documents\Spring2025\EELE465\project04\project4-gabby-iker\controller\src\master_i2c.h"
#include "C:\Users\gabri\Documents\Spring2025\EELE465\project04\project4-gabby-iker\controller\src\rgb_led.h"
#include "C:\Users\gabri\Documents\Spring2025\EELE465\project04\project4-gabby-iker\controller\src\heartbeat.h"
//--End Headers---------------------------------------------------------

//----------------------------------------------------------------------
// Definitions
//----------------------------------------------------------------------
#define PROWDIR     P6DIR  // FORMERLY P1
#define PROWREN     P6REN
#define PROWIN      P6IN
#define PROWOUT     P6OUT
#define PCOLDIR     P5DIR   // FORMERLY P5
#define PCOLOUT     P5OUT
#define RS BIT2  // P1.2 -> RS (Register Select)
#define EN BIT3  // P1.3 -> Enable
#define D4 BIT4  // P1.4 -> Data bit 4
#define D5 BIT5  // P1.5 -> Data bit 5
#define D6 BIT6  // P1.6 -> Data bit 6
#define D7 BIT7  // P1.7 -> Data bit 7
#define COL 4
#define ROW 4
#define TABLE_SIZE 4
//--End Definitions-----------------------------------------------------

//----------------------------------------------------------------------
// Variables
//----------------------------------------------------------------------
char real_code[] = {'3','9','4','D'};

char keypad[ROW][COL] = {
    {'1', '2', '3', 'A'},
    {'4', '5', '6', 'B'},
    {'7', '8', '9', 'C'},
    {'*', '0', '#', 'D'}
};

int lockState = 3;
volatile float temp_samples[3] = {0};  // Storage for 3 samples
volatile float averaged_temp = 0.0;    // The calculated moving average
volatile unsigned int sample_count = 0;
volatile unsigned int adc_result = 0;
volatile float temperature_celsius = 0.0;

//--End Variables-------------------------------------------------------

//----------------------------------------------------------------------
// Begin Debounce
//----------------------------------------------------------------------
void debounce() {
    volatile unsigned int i;
    for (i = 20000; i > 0; i--) {}
}
//--End Debounce--------------------------------------------------------

//----------------------------------------------------------------------
// Begin Initializing Keypad Ports
//----------------------------------------------------------------------
void keypad_init() 
{
    // Set rows as inputs (with pull-up)
    PROWDIR &= ~0x0F;   // P1.0, P1.1, P1.2, P1.3 as inputs
    PROWREN |= 0x0F;    // Activate pull-up
    PROWOUT |= 0x0F;    // Activar pull-up in rows
    
    // Set columns as outputs
    PCOLDIR |= BIT0 | BIT1 | BIT2 | BIT3; // Set P5.0, P5.1, P5.2 y P5.3 as outputs:
    PCOLOUT &= ~(BIT0 | BIT1 | BIT2 | BIT3);  // Set down the pins P5.0, P5.1, P5.2 y P5.3:
}
//--End Initialize Keypad-----------------------------------------------

void adc_init(void)
{
    P1SEL0 |= BIT3;  // A3
    P1SEL1 |= BIT3;

    ADCCTL0 |= ADCSHT_2 | ADCON;            // Sampling time, ADC on
    ADCCTL1 |= ADCSHP | ADCCONSEQ_0;        // Pulse sample mode, single channel
    ADCCTL2 |= ADCRES_2;                    // 12-bit resolution
    ADCMCTL0 |= ADCINCH_3;                  // A3 input
    ADCIE |= ADCIE0;                        // Enable interrupt
}
//--End Initialize ADC--------------------------------------------------

//----------------------------------------------------------------------
// Begin Initializing Timer B1
//----------------------------------------------------------------------
void timer_b1_init(void) 
{
    // Setup Timer
    TB1CTL |= TBCLR;            // Clear timers and dividers
    TB1CTL |= TBSSEL__ACLK;     // Source = ACLK
    TB1CTL |= MC__UP;           // Mode = UP
    TB1CCR0 = 16384;            // CCR0 = 16384 (0.5s overflow)

    // Setup Timer Compare IRQ
    TB1CCTL0 &= ~CCIFG;         //Clear CCR0 Flag
    TB1CCTL0 |= CCIE;           // Enable TB0 CCR0 Overflow IRQ
}
//--End Initialize TimerB0----------------------------------------------


//----------------------------------------------------------------------
// Begin Unlocking Routine
//----------------------------------------------------------------------
char keypad_unlocking(void)
{
    int row, col;

    // Go through 4 columns
    for (col = 0; col < 4; col++) {
        // Put column down (active)
        PCOLOUT &= ~(1 << col);   // P2.0, P2.1, P2.2, P2.4
         __delay_cycles(1000);  // Little stop to stabilize the signal

        // Go through 4 rows
        for (row = 0; row < 4; row++) {
            if ((PROWIN & (1 << row)) == 0) {  // We detect that the row is low
                debounce();  // Wait to filter the bouncing
                if ((PROWIN & (1 << row)) == 0) {  // Confirm that the key is pressed
                    rgb_led_continue(0);                // Set LED to yellow
                    char key = keypad[row][col];
                    // Wait until the key is released to avoid multiple readings 
                    while ((PROWIN & (1 << row)) == 0);
                    // Deactivate the column before returning
                    PCOLOUT |= (1 << col);                    
                    return key;
                }
            }
        }
        // Put the column high (desactivated)
        PCOLOUT |= (1 << col);
    }

    return 0; // No key pressed
}
//--End Unlocking-------------------------------------------------------

//----------------------------------------------------------------------
// Begin Unlocked Routine
//----------------------------------------------------------------------
char keypad_unlocked(void)
{
    char key_unlocked = '\0';

    // Continuously poll until 'D' is pressed
    while (key_unlocked != 'D') {
        int row, col;

        for (col = 0; col < 4; col++) {
            // Activate column
            PCOLOUT &= ~(1 << col);
            __delay_cycles(1000);

            // Check all rows
            for (row = 0; row < 4; row++) {
                if ((PROWIN & (1 << row)) == 0) {
                    debounce();
                    if ((PROWIN & (1 << row)) == 0) {
                        key_unlocked = keypad[row][col];
                        if (key_unlocked != 'D') {
                            master_i2c_send(key_unlocked, 0x068);
                            master_i2c_send(key_unlocked, 0x048);
                        }
                        // Wait for key release
                        while ((PROWIN & (1 << row)) == 0);
                        PCOLOUT |= (1 << col);

                        if (key_unlocked == 'D') {
                            rgb_led_continue(3);  // Set LED to red when 'D' is pressed
                            master_i2c_send('D', 0x068);
                            master_i2c_send('D', 0x048);
                            return key_unlocked;
                        }
                    }
                }
            }
            // Deactivate column
            PCOLOUT |= (1 << col);
        }
    }
    return key_unlocked;
}
//--End Unlocked--------------------------------------------------------

float calculate_temperature(unsigned int adc_val) {
    float Vout = (adc_val * 3.3) / 4095.0; // Assuming 3.3V reference, 12-bit ADC
    return (-1481.96 + sqrt(2.1962e6 + (1.8639 - Vout) / 3.88e-6)) / 1e2;
}

//----------------------------------------------------------------------
// Begin Main
//----------------------------------------------------------------------

int main(void)
{   
    
    int counter, i, equal;
    char introduced_password[TABLE_SIZE], key; 

        WDTCTL = WDTPW | WDTHOLD;   // Stop watchdog timer

    keypad_init();
    heartbeat_init();
    rgb_led_init();
    master_i2c_init();
    adc_init();
    timer_b1_init();

    __enable_interrupt();  // Enable global interrupts
      
    while(true)
    {
        counter = 0;
        key = 0;

        while (counter < TABLE_SIZE)
        {
            key = keypad_unlocking();
            if(key!=0)
            {
                introduced_password [counter] = key;
                counter++;
            }        
        }

        //Compare the introduced code with the real code   
        equal = 1;   
        for (i = 0; i < TABLE_SIZE; i++) {
            if (introduced_password[i] != real_code[i]) 
            {
                equal = 0;
                break;
            }
        }

        // Verify the code
        if (equal==1) 
        {
            printf("Correct Code!\n");
            counter = 0;
            rgb_led_continue(1);            // Set LED to blue
            for (i = 0; i < TABLE_SIZE; i++) 
            {
                introduced_password[i] = 0;        
            }
            keypad_unlocked();  // This now handles polling until 'D' is pressed


        } 
        else 
        {
            printf("Incorrect code. Try again.\n");
            counter = 0;  // Reinitiate counter to try again
            rgb_led_continue(3);            // Set LED to red
            master_i2c_send('\0', 0x068);
            master_i2c_send('\0', 0x048);
            //led_patterns('\0');
            for (i = 0; i < TABLE_SIZE; i++) 
            {
                introduced_password[i] = 0;        
            }
        }    
    }
    return 0;
}
//--End Main------------------------------------------------------------

//------------------------------------------------------------------------------
// Begin Interrupt Service Routines
//------------------------------------------------------------------------------
#pragma vector = ADC_VECTOR
__interrupt void ADC_ISR(void)
{
    if (ADCIV == ADCIV_ADCIFG) {
        adc_result = ADCMEM0;
        temperature_celsius = calculate_temperature(adc_result);

        // Save sample
        temp_samples[sample_count] = temperature_celsius;
        sample_count++;

        if (sample_count >= 3) {
    averaged_temp = (temp_samples[0] + temp_samples[1] + temp_samples[2]) / 3.0;
    sample_count = 0;

    // Conversión manual a texto
    int temp_int = (int)averaged_temp;
    int temp_frac = (int)((averaged_temp - temp_int) * 10);  // 1 decimal

    char temp_string[16];
    temp_string[0] = 'T';
    temp_string[1] = 'e';
    temp_string[2] = 'm';
    temp_string[3] = 'p';
    temp_string[4] = ':';
    temp_string[5] = ' ';
    temp_string[6] = temp_int / 10 + '0';            // decenas
    temp_string[7] = temp_int % 10 + '0';            // unidades
    temp_string[8] = '.';
    temp_string[9] = temp_frac + '0';                // primer decimal
    temp_string[10] = 'C';
    temp_string[11] = '\0';

    int i;
    for (i = 0; temp_string[i] != '\0'; i++) {
        master_i2c_send(temp_string[i], 0x068);
    }
}
    }
}
#pragma vector = TIMER1_B0_VECTOR
__interrupt void ISR_TB1_CCR0(void)
{
    ADCCTL0 |= ADCENC | ADCSC;  // Start conversion
    TB1CCTL0 &= ~CCIFG;
}
//-- End Interrupt Service Routines --------------------------------------------
