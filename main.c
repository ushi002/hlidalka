/******************************************************************************/
/* Files to Include                                                           */
/******************************************************************************/

#if defined(__XC)
    #include <xc.h>         /* XC8 General Include File */
#elif defined(HI_TECH_C)
    #include <htc.h>        /* HiTech General Include File */
#endif

#include <stdint.h>        /* For uint8_t definition */
#include <stdbool.h>       /* For true/false definition */

#include "system.h"        /* System funct/params, like osc/peripheral config */
#include "user.h"          /* User funct/params, such as InitApp */

/******************************************************************************/
/* User Global Variable Declaration                                           */
/******************************************************************************/

#define PRTC_ANAL    ANSELC

/* i.e. uint8_t <variable_name>; */

/******************************************************************************/
/* Main Program                                                               */
/******************************************************************************/
void main(void)
{
    unsigned int dacval, i;
    
    /* Configure the oscillator for the device */
    ConfigureOscillator();

    /* Initialize I/O and Peripherals for application */
    InitApp();
    
    //turn off watchdog:
    WDTCONbits.SWDTEN = 0; //check WDTE in config word1, must be 01 to enable 
                            //control
    //WDTCONbits.WDTPS = 0xf; //default 2s
    
    //configure SPI pins:
    //0 = output, 1 = input    
    TRISAbits.TRISA4 = 0;
    TRISAbits.TRISA5 = 1;
    TRISCbits.TRISC4 = 0;
    TRISCbits.TRISC5 = 0;
    //enable weak pull ups
    OPTION_REGbits.nWPUEN = 0;
    //weak pull up
    WPUAbits.WPUA5 = 1;
    
    //set output value
    PORTAbits.RA4 = 0;
    
    //set output value
    PORTCbits.RC4 = 0;
    PORTCbits.RC5 = 0;
    
    //enable DAC port output pin:
    DACCON0bits.DACOE1 = 1;
    //set 0 for VDD as positive source for DAC
    DACCON0bits.D1PSS = 0;
    //enable DAC
    DACCON0bits.DACEN = 1;

    dacval = 0;
    while(1)
    {
        asm("CLRWDT");
        for(i=0; i<20000; i++) //higher then 20000 values does not work - watchdog!
        {            
        }
        PORTAbits.RA4 = LATAbits.LATA4 ^ 1;

        dacval++;
        DACCON1bits.DACR = dacval;  //5 bits copied
        dacval = DACCON1bits.DACR;
        
        PORTCbits.RC4 = 1; //keep DC/DC on
        PORTCbits.RC5 = 0; //shutter off
        
        if (dacval == 3 || dacval == 5) //shutter trigger
        {
            PORTCbits.RC5 = 1;            
        }
        
        if (dacval > 23)  //skip last 8 steps (31 steps)
        {
            dacval = 0;
            PORTCbits.RC4 = 0;            
        }        
    }
    
    //enable interrupts
    //GIE is for interrupt vector. Else program continues just after sleep
    //INTCONbits.GIE = 1;
    //enable external interrupt
    INTCONbits.INTE = 1;
    //enable interrupt on change
    INTCONbits.IOCIE = 1;
    
    //detect falling edge
    IOCANbits.IOCAN5 = 1;
    //!! IOCAFbits.IOCAF5 should be set when interrupt arrives..
    //??OPTION_REGbits.INTEDG = 0; //falling edge interrupt
    //OPTION_REGbits
    asm("SLEEP");
    asm("NOP");    
}

