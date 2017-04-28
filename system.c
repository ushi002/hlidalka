/******************************************************************************/
/*Files to Include                                                            */
/******************************************************************************/

#if defined(__XC)
    #include <xc.h>         /* XC8 General Include File */
#elif defined(HI_TECH_C)
    #include <htc.h>        /* HiTech General Include File */
#endif

#include <stdint.h>        /* For uint8_t definition */
#include <stdbool.h>       /* For true/false definition */

#include "system.h"

/* Refer to the device datasheet for information about available
oscillator configurations. */
void ConfigureOscillator(void)
{
    /* TODO Add clock switching code if appropriate.  */
    
    //APPLIED VALUE: default/reset value of OSCCON.IRCF is '0111' -> 500kHz
    
    //#if defined(USE_INTERNAL_OSC)
    //Make sure to turn on active clock tuning for USB full speed 
    //operation from the INTOSC
    //OSCCON = 0xFC;  //HFINTOSC @ 16MHz, 3X PLL, PLL enabled
    //ACTCON = 0x90;  //Active clock tuning enabled for USB
    //#endif

    /* Typical actions in this function are to tweak the oscillator tuning
    register, select new clock sources, and to wait until new clock sources
    are stable before resuming execution of the main project. */
}
