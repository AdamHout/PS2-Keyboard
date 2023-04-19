/*----------------------------------------------------------------------------*/
/* Program Overview:                                                          */
/* Act as an interface between a PS2 keyboard and a master controller         */
/*----------------------------------------------------------------------------*/  
/* MCU: PIC24FJ64GA002 FOSC = 32MHz FCY = 16MHz                               */
/*----------------------------------------------------------------------------*/  
/* Peripherals Used:                                                          */
/* External interrupt 0 - PS2 clock line - interrupt on falling edge          */
/* SPI1 - Connection to the host                                              */
/*----------------------------------------------------------------------------*/  
/* External Devices:                                                          */
/* PS2 keyboard - Rosewill F21SG                                              */
/*----------------------------------------------------------------------------*/  
/* Pointers:                                                                  */
/*        */
/*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*-*--*-*-*/  
/* Revisions:                                                                 */
/*                                                                            */
/* Date                         Comments                                      */
/* 03/2023  Adam Hout           Original development                          */
/*----------------------------------------------------------------------------*/  

// PIC24FJ64GA002 Configuration Bit Settings
// CONFIG2
#pragma config POSCMOD = NONE                                                   // Primary Oscillator Select (Primary oscillator disabled)
#pragma config I2C1SEL = PRI                                                    // I2C1 Pin Location Select (Use default SCL1/SDA1 pins)
#pragma config IOL1WAY = OFF                                                    // IOLOCK Protection (IOLOCK may be changed via unlocking seq)
#pragma config OSCIOFNC = ON                                                    // Primary Oscillator Output Function (OSC2/CLKO/RC15 functions as port I/O (RC15))
#pragma config FCKSM = CSDCMD                                                   // Clock Switching and Monitor (Clock switching and Fail-Safe Clock Monitor are disabled)
#pragma config FNOSC = FRCPLL                                                   // Oscillator Select (Fast RC Oscillator with PLL module)
#pragma config SOSCSEL = SOSC                                                   // Sec Oscillator Select (Default Secondary Oscillator (SOSC))
#pragma config WUTSEL = LEG                                                     // Wake-up timer Select (Legacy Wake-up Timer)
#pragma config IESO = OFF                                                       // Internal External Switch Over Mode (IESO mode (Two-Speed Start-up) disabled)

// CONFIG1
// WDT: TO = ( FWPSA ) × ( WDTPS ) × ( T LPRC )
//      TO = 128 * 256 * (1/31000) = 1.06 seconds
#pragma config WDTPS = PS256                                                    // Watchdog Timer Postscaler (1:256)
#pragma config FWPSA = PR128                                                    // WDT Prescaler (Prescaler ratio of 1:128) 
#pragma config WINDIS = ON                                                      // Watchdog Timer Window (Standard Watchdog Timer enabled,(Windowed-mode is disabled))
#pragma config FWDTEN = OFF              // Watchdog Timer Enable (Watchdog Timer is enabled)
#pragma config ICS = PGx1                                                       // Comm Channel Select (Emulator EMUC1/EMUD1 pins are shared with PGC2/PGD2)
#pragma config GWRP = OFF                                                       // General Code Segment Write Protect (Writes to program memory are allowed)
#pragma config GCP = OFF                                                        // General Code Segment Code Protect (Code protection is disabled)
#pragma config JTAGEN = OFF                                                     // JTAG Port Enable (JTAG port is disabled)

/*----------------------------------*/
/* Includes                         */
/*----------------------------------*/
#include "xc.h"
#include "ps2kb.h"
#include "sys.h"
#include "sup.h"

/*----------------------------------*/
/* Defines                          */
/*----------------------------------*/
#define KB_FLAG_A AD1PCFGbits.PCFG12                                            //Notification pin
#define KB_FLAG_T TRISBbits.TRISB12
#define KB_FLAG_L LATBbits.LATB12

/*----------------------------------*/
/*Globals from ps2kb.c              */
/*----------------------------------*/
extern kbFlags_t xFlags, *pFlags;

/*--------------------------------------------------------------*/
/* Begin mainline processing                                    */
/*--------------------------------------------------------------*/
int main(void){
   
   //Init notification pin
   KB_FLAG_A = 1;                                                               //Set flag pin to digital
   KB_FLAG_T = 0;                                                               //Set to output
   KB_FLAG_L = 0;                                                               //Set low; Active high
   
   //Initialize the keyboard interface
   int16_t rtnCode = kbInitialize(); 
   
   if(rtnCode)
      pFlags->errFlag = 1;
   
   SetUnusedPins();                                                             //Make digital and drive low                                                                       
   
   /*--------------------------------------------------*/
   /*Main control loop                                 */
   /*--------------------------------------------------*/
   while(1){
      
//      ClrWdt();
         
      //Process scan codes from the keyboard
      if(pFlags->scanFlag){                                                     //New scan code?
         pFlags->scanFlag = 0;                                                  //Clear the scan code flag 
         kbCheckFlags();                                                        //Check for special conditions
   
         if(pFlags->breakFlag)                                                  //Discard break sequences                                                            
            pFlags->breakFlag--;
         else if(pFlags->skipFlag)                                              //Flagged to discard
            pFlags->skipFlag = 0;
         else if(pFlags->capsFlag || pFlags->numsFlag){                         //Caps or num lock sequence?
            kbSetLocks();                                                       //Yes.. set/clear the lock
            pFlags->capsFlag = 0;
            pFlags->numsFlag = 0;
         }
         else{   
            kbPostCode();                                                       //Translate scan code and add to the buffer
            if(!KB_FLAG_L)                                                      //Notify the host
               KB_FLAG_L = 1;
         }              
      }
   }
   return 0;
}