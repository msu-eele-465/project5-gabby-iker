#include <msp430.h>
#include "msp430fr2355.h"
#include "intrinsics.h"
#include <stdbool.h>
#include <stdio.h>
#include "rgb_led.h"
//#include "led_bar.h"
#include "heartbeat.h"

//KEYPAD I/O DECLARATION
#define PROWDIR     P6DIR  // FORMERLY P1
#define PROWREN     P6REN
#define PROWIN      P6IN
#define PROWOUT     P6OUT
#define PCOLDIR     P5DIR   // FORMERLY P5
#define PCOLOUT     P5OUT

//CONSTANTS DECLARATION
#define COL 4
#define ROW 4
#define TABLE_SIZE 4

//END CONSTANTS DECLARATION

void debounce() {
    volatile unsigned int i;
    for (i = 20000; i > 0; i--) {}
}

char real_code[] = {'3','9','4','D'};

char keypad[ROW][COL] = {
    {'1', '2', '3', 'A'},
    {'4', '5', '6', 'B'},
    {'7', '8', '9', 'C'},
    {'*', '0', '#', 'D'}
};

int lockState = 3;

// Function ton initialaize the ports
void keypad_init() 
{
    // Set rows as inputs (with pull-up)
    PROWDIR &= ~0x0F;   // P1.0, P1.1, P1.2, P1.3 as inputs
    PROWREN |= 0x0F;    // Activate pull-up
    P1OUT |= 0x0F;    // Activar pull-up in rows
    
    // Set columns as outputs
    PCOLDIR |= BIT0 | BIT1 | BIT2 | BIT3; // Set P5.0, P5.1, P5.2 y P5.3 as outputs:
    PCOLOUT &= ~(BIT0 | BIT1 | BIT2 | BIT3);  // Set down the pins P5.0, P5.1, P5.2 y P5.3:
}

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
                    continue_rgb_led(0);                // Set LED to yellow
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
                            //set_led_bar(key_unlocked);
                        }
                        // Wait for key release
                        while ((PROWIN & (1 << row)) == 0);
                        PCOLOUT |= (1 << col);

                        if (key_unlocked == 'D') {
                            continue_rgb_led(3);  // Set LED to red when 'D' is pressed
                            //set_led_bar('D');
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



int keypad_main(void)
{
    // Stop watchdog timer
    WDTCTL = WDTPW | WDTHOLD; // Stop watchdog timer
    
    // Disable the GPIO power-on default high-impedance mdoe to activate
    // previously configure port settings
    PM5CTL0 &= ~LOCKLPM5;
    
    int counter, i, equal;
    char introduced_password[TABLE_SIZE], key, save_digit, key_unlocked; 

    keypad_init();
    init_heartbeat();
    init_rgb_led();
    //init_led_bar();
      
    while(true)
    {   

     counter = 0;
     key = 0;
    
     printf("Introduce the code\n");

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
            continue_rgb_led(1);            // Set LED to blue
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
            continue_rgb_led(3);            // Set LED to red
            //led_patterns('\0');
            for (i = 0; i < TABLE_SIZE; i++) 
            {
                introduced_password[i] = 0;        
            }
        }    
    }
}
