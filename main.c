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
#include <stdlib.h>        /* c standard library */

#include "system.h"        /* System funct/params, like osc/peripheral config */
#include "user.h"          /* User funct/params, such as InitApp */

/******************************************************************************/
/* User Global Variable Declaration                                           */
/******************************************************************************/

#define PRTC_ANAL    ANSELC

#define CAMTRIG         PORTCbits.RC5
#define CAMPWREN        PORTCbits.RC4
/* i.e. uint8_t <variable_name>; */

#define MODE_STANDBY    0
#define MODE_SF_TX      1
#define MODE_SF_RX      2

#define CMDNUM_AT_CHECK   0 //first command! must stay first!
#define CMDNUM_AT_TEMP    1
#define CMDNUM_AT_VOLT    2
#define CMDNUM_AT_TXFRAME 3
#define CMDNUM_NA         4 //last one, not available (out of range)

#define TO_HEX(i) (i <= 9 ? '0' + i : 'A' - 10 + i)

#define HK_TIMEOUT        900 //number of watchdog wakeups (4 seconds per wake)
/******************************************************************************/
/* Main Program                                                               */
/******************************************************************************/
void main(void)
{
    unsigned int dacval, i, timeout;
    bool thiefdetected = 0;
    
    uint8_t mode = MODE_STANDBY;
    uint8_t cmdn = 0;
    bool sleep_well = true;


    /* Configure the oscillator for the device */
    ConfigureOscillator();

    /* Initialize I/O and Peripherals for application */
    InitApp();
    
    //turn off watchdog:
    WDTCONbits.SWDTEN = 1; //check WDTE in config word1, must be 01 to enable
                            //control
    WDTCONbits.WDTPS = 0xc; //0xC=4s
    
    //configure SPI pins:
    //0 = output, 1 = input    
    TRISAbits.TRISA4 = 0;
    TRISAbits.TRISA5 = 1;

    TRISCbits.TRISC4 = 0; //UART TX
    TRISCbits.TRISC5 = 1; //UART RX
    //enable weak pull ups
    //OPTION_REGbits.nWPUEN = 0;
    //weak pull up
    //WPUAbits.WPUA5 = 1;
    
    //set output value
    PORTAbits.RA4 = 0;
    //PORTAbits.RA5 = 0;
    
    //set output value
    CAMPWREN = 0;
    CAMTRIG = 0;
    
    //enable DAC port output pin:
    DACCON0bits.DACOE1 = 1;
    //set 0 for VDD as positive source for DAC
    DACCON0bits.D1PSS = 0;
    //enable DAC
    DACCON0bits.DACEN = 1;
    ANSELAbits.ANSA4 = 0; //enable digital input for PORTA4
    dacval = 0;
    
    if (0) //ADC config
    {
        //config FVR:
        FVRCONbits.FVREN = 1; //enable
        FVRCONbits.TSEN = 1; //enable temp indicator
        FVRCONbits.TSRNG = 0; //temp low range, min Vdd 1.8V
        FVRCONbits.ADFVR = 0; //AD ref 2=2.048V, 0=off
        ADCON1bits.ADFM = 1; //right justified
        ADCON1bits.ADPREF = 0; //3=Vref to FVR (fixed voltage reference), 0=VDD
        
        ADCON0bits.CHS = 29; //channel select tem indicator
        
        ADCON1bits.ADCS = 0; //Fosc/2
        ADCON0bits.ADON = 1; //enable ADC
        //PIE1bits.ADIE = 1; //enable interrupt flag
        
        while(0)
        {
            asm("CLRWDT");
            for(i=0; i<20000; i++){} //delay
            
            ADCON0bits.GO_nDONE = 1; //start conversion

            while(!PIR1bits.ADIF){}; //wait until conversion is done

            uint16_t re1, re2, res;
            re1 = ADRESH;
            re2 = ADRESL;
            res = re1<<8 | re2;
        }
        
    }
    
    if (1) //UART config
    {        
        SPBRGH = 0;
        SPBRGL = 12; //9600baud for FOSC default 500kHz = 12
        //building EUSART communication
        BAUDCONbits.BRG16 = 1; //BRG16 = 1, BRGH = 1; => FOSC/[4 (n+1)]
        TXSTAbits.BRGH = 1;
        TXSTAbits.SYNC = 0; //asynchronous communication
        RCSTAbits.SPEN = 1; //enable UART, pins
        TXSTAbits.TXEN = 1; //TXIF is set at this moment
        RCSTAbits.CREN = 1; //enable uart reception
        uint8_t sfi = 0;
        
        char sf_at_cmd[5] = "AT\r\n";
        char sf_at_temp[8] = "AT$T?\r\n";
        char sf_at_volt[8] = "AT$V?\r\n";
        char sf_frame[20] = "AT$SF=123456789012\r\n";
        const uint8_t sf_frame_offset = 6;
        char sf_rec_chars[24];
        char * sf_tx_chars;
        bool sf_report_alarm = false;
        uint8_t sf_errorcnt = 0;
        uint16_t hk_to;
        int16_t number_i16;
        
        //enable external interrupt
        INTCONbits.INTE = 1;
        //enable interrupt on change
        INTCONbits.IOCIE = 1;
        //detect rising (P) falling (N) edge
        IOCAPbits.IOCAP5 = 1;
        IOCAFbits.IOCAF5 = 0;
        
        LATAbits.LATA4 = 0;
        
        hk_to = HK_TIMEOUT;
        while(1)
        {
            asm("CLRWDT");
            switch(mode)
            {
            case MODE_STANDBY:
                LATAbits.LATA4 = 0;
                while (sleep_well)
                {
                    IOCAFbits.IOCAF5 = 0;
                    asm("NOP");  
                    asm("SLEEP");
                    asm("NOP");
                    LATAbits.LATA4 = 1; for(i=0; i<200; i++) {} LATAbits.LATA4 = 0; //blink
                    hk_to--;
                    if(IOCAFbits.IOCAF5 || hk_to == 0)    //PORTA.5 flag - alarm
                    {
                        sleep_well = false; //cannot fall asleep as UART does not work
                        if (IOCAFbits.IOCAF5) //alarm has priority
                            sf_report_alarm = true;
                        else
                            sf_report_alarm = false;
                    }
                }
                LATAbits.LATA4 = 1;
                {                    
                    mode = MODE_SF_TX;
                    sfi = 0;
                    
                    if (cmdn==CMDNUM_AT_CHECK) {sf_tx_chars = sf_at_cmd;}
                    if (cmdn==CMDNUM_AT_TEMP) {sf_tx_chars = sf_at_temp;}
                    if (cmdn==CMDNUM_AT_VOLT) {sf_tx_chars = sf_at_volt;}
                    if (cmdn==CMDNUM_AT_TXFRAME) {sf_tx_chars = sf_frame;}
                    
                    //INTCONbits.PEIE = 1; //peripheral wakeup from sleep
                    //PIE1bits.RCIE = 1; //enable RX interrupt
                    //PIE1bits.TXIE = 1; //enable TX interrupt
                }
                break;
            case MODE_SF_TX:
                if (PIR1bits.TXIF) //UART has space for char
                {
                    TXREG = sf_tx_chars[sfi];
                    if (sf_tx_chars[sfi] == '\n')
                    {
                        mode = MODE_SF_RX;
                        sfi = 0;
                        timeout = 0;
                    }else
                    {
                        sfi++;
                    }
                }
                break;
            case MODE_SF_RX:
                if (PIR1bits.RCIF) //a char is received
                {
                    asm("CLRWDT");
                    sf_rec_chars[sfi++] = RCREG;
                    timeout = 0;
                }
                timeout++;
                //if (!STATUSbits.nTO) //watchdog timeout
                if (timeout>40000)
                {
                    //INTCONbits.PEIE = 0; //peripheral wakeup from sleep
                    //PIE1bits.RCIE = 0; //enable RX interrupt
                    //PIE1bits.TXIE = 0; //enable TX interrupt
                    mode = MODE_STANDBY;
                    if (sf_rec_chars[0] != 'E') //correctly received uart message
                    {
                        number_i16 = strtol(sf_rec_chars, NULL, 10); //convert first num to int16
                        switch (cmdn)
                        {
                        case CMDNUM_AT_TEMP:
                            sf_frame[sf_frame_offset+0] = TO_HEX(((number_i16 & 0xF000) >> 12));
                            sf_frame[sf_frame_offset+1] = TO_HEX(((number_i16 & 0x0F00) >> 8));
                            sf_frame[sf_frame_offset+2] = TO_HEX(((number_i16 & 0x00F0) >> 4));
                            sf_frame[sf_frame_offset+3] = TO_HEX((number_i16 & 0x000F));
                            break;
                        case CMDNUM_AT_VOLT:
                            sf_frame[sf_frame_offset+4] = TO_HEX(((number_i16 & 0xF000) >> 12));
                            sf_frame[sf_frame_offset+5] = TO_HEX(((number_i16 & 0x0F00) >> 8));
                            sf_frame[sf_frame_offset+6] = TO_HEX(((number_i16 & 0x00F0) >> 4));
                            sf_frame[sf_frame_offset+7] = TO_HEX((number_i16 & 0x000F));
                            sf_frame[sf_frame_offset+8] = '0';
                            if (sf_report_alarm) //alarm!
                            {
                                sf_frame[sf_frame_offset+8] += 0x1;
                            }
                            sf_frame[sf_frame_offset+9] = '0';
                            sf_frame[sf_frame_offset+10] = '\r';
                            sf_frame[sf_frame_offset+11] = '\n';
                            break;
                        default:
                            //CMDNUM_AT_TXFRAME
                            break;
                        }
                        cmdn++;
                        sf_rec_chars[0] = 'E';
                    }else
                    {
                        sf_errorcnt++;
                        sf_rec_chars[1] = 'E';
                    }
                    if (cmdn >= CMDNUM_NA || sf_errorcnt >= 10)
                    {
                        //all messages sent or 10x message error
                        sf_errorcnt = 0;
                        cmdn = CMDNUM_AT_CHECK; //first command
                        sleep_well = true;
                        hk_to = HK_TIMEOUT;
                    }
                }
                break;
            default:
                mode = MODE_STANDBY;
                break;
            }            
            
        }
    }
    while(1)
    {
        if (!thiefdetected)
        {
            //enable interrupts
            //GIE is for interrupt vectors. Else program continues just after the sleep
            //INTCONbits.GIE = 1;
            //enable external interrupt
            INTCONbits.INTE = 1;
            //enable interrupt on change
            INTCONbits.IOCIE = 1;

            //detect rising (P) falling (N) edge
            IOCAPbits.IOCAP5 = 1;
            //!! IOCAFbits.IOCAF5 should be set when interrupt arrives..
            //??OPTION_REGbits.INTEDG = 0; //falling edge interrupt
            //OPTION_REGbits
            INTCONbits.INTF = 0; //external interrupt flag
            while(1)
            {
                for(i=0; i<200; i++) {} //delay before diode is turned off
                LATAbits.LATA4 = 0;
                asm("SLEEP");
                asm("NOP");
                if (!STATUSbits.nTO) //watchdog timeout
                    LATAbits.LATA4 = 1;
                if (PCONbits.nRWDT)
                    LATAbits.LATA4 = 1; //watchdog reset has not occurred
                if(INTCONbits.IOCIF)
                    LATAbits.LATA4 = 1; //some external interrupt flag occurred
                INTCONbits.INTF = 0; //clear (watchdog) flag
                if(IOCAFbits.IOCAF5)    //PORTA.5 flag
                {
                    //IOCAFbits.IOCAF5 = 0; //moved to the very end of the cycle
                    thiefdetected = true;
                    break; //quit this loop and continue the program
                }
            }
        }
        asm("CLRWDT");
        for(i=0; i<20000; i++) //higher then 20000 values does not work - watchdog! (OLD WATCHDOG)
        {
        }
        LATAbits.LATA4 = LATAbits.LATA4 ^ 1; //heart beat

        APFCONbits.P2SEL = 0;

        dacval++;
        DACCON1bits.DACR = dacval;  //5 bits copied
        dacval = DACCON1bits.DACR;
        
        CAMPWREN = 1; //keep DC/DC on
        CAMTRIG = 0; //shutter off
        
        if (dacval == 3 || dacval == 5) //shutter trigger
        {
            CAMTRIG = 1;
        }
        
        //if (dacval > 3)  //speed up debugging
        if (dacval > 23)  //skip last 8 steps (31 steps)
        {
            dacval = 0;
            DACCON1bits.DACR = 0;
            CAMPWREN = 0;
            thiefdetected = 0; //end of camera sequence
            IOCAFbits.IOCAF5 = 0;
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

