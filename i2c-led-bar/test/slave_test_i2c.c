#include "msp430fr2310.h"
#include <msp430.h>

#define SLAVE_ADDR  0x68                    // Slave I2C Address
volatile unsigned char receivedData = 0;    // Recieved data

void I2C_Slave_Init(void)
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

int main(void)
{
    I2C_Slave_Init();                   // Initialize the slave for I2C
    __bis_SR_register(LPM0_bits + GIE); // Enter LPM0, enable interrupts
    return 0;
}



// I2C ISR
#pragma vector = USCI_B0_VECTOR
__interrupt void USCI_B0_ISR(void)
{
    switch (__even_in_range(UCB0IV, USCI_I2C_UCTXIFG0))
    {
        case USCI_I2C_UCRXIFG0:         // Receive Interrupt
            receivedData = UCB0RXBUF;   // Read received data
            break;
        default: 
            break;
    }
}

