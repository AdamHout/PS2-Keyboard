/*
 * File:   main.c
 * Author: adam
 *
 * Created on March 13, 2023, 10:32 PM
 */

// PIC24FJ64GA002 Configuration Bit Settings

// 'C' source line config statements

// CONFIG2
#pragma config POSCMOD = NONE                                                   // Primary Oscillator Select (Primary oscillator disabled)
#pragma config I2C1SEL = PRI                                                    // I2C1 Pin Location Select (Use default SCL1/SDA1 pins)
#pragma config IOL1WAY = ON                                                     // IOLOCK Protection (Once IOLOCK is set, cannot be changed)
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

/*----------------------------------*/
/* Defines                          */
/*----------------------------------*/
#define KB_FLAG_A AD1PCFGbits.PCFG12                                            //Notification pin
#define KB_FLAG_T TRISBbits.TRISB12
#define KB_FLAG_L LATBbits.LATB12

/*----------------------------------*/
/*Globals from ps2kb.c              */
/*----------------------------------*/
extern volatile int   scanFlag;                                                 //=1 when new scan code received
extern volatile int   capsFlag;                                                 //Caps lock flag
extern volatile int   capsBreak;                                                //Caps key released
extern volatile unsigned char scanCode;                                         //Last scan code recorded

/*----------------------------------*/
/* Function declarations            */
/*----------------------------------*/
int SetCapsLock(void);



/*--------------------------------------------------------------*/
/* Begin mainline processing                                    */
/*--------------------------------------------------------------*/
int main(void){
   
   unsigned char retryCnt = 0;                                                  //Used for echo
   int rtnCode;

   //Init
   KB_FLAG_A = 1;                                                               //Set flag pin to digital
   KB_FLAG_T = 0;                                                               //Set to output
   KB_FLAG_L = 0;                                                               //Set low; Active high
   
   //Initialize the keyboard interface
   KBInit(); 
   
   do{
      KBSendCmd(CMD_ECHO,NO_ARGS);                                              //Send an echo command
      while(!scanFlag);                                                         //Wait for the keyboard to reply
      scanFlag = 0;
   }while(scanCode != CMD_ECHO && retryCnt++ < 3);
   
   //if(scanFlag != CMD_ECHO)
   //   set error code
   
   /*--------------------------------------------------*/
   /*Main control loop                                 */
   /*--------------------------------------------------*/
   while(1){
//      ClrWdt();
      
//      if(kbCount && !KB_FLAG_L){
//         KB_FLAG_L = 1;
//      }
         
      if(scanFlag){                                                             //New scan code?
         scanFlag = 0;                                                          //Yes.. clear the flag
         
         if(scanCode == CAPS_S){                                                //Caps lock?         
            rtnCode =  SetCapsLock();     
         }
         else{
            KBConvertScanCode();                                                //Translate scan code and place on the buffer 
         }
         
      }
   }
   return 0;
}

/*-----------------------------------------------------*/
/* Functions / Subroutines                             */
/*-----------------------------------------------------*/
int SetCapsLock(void){
   
//   if(capsBreak){                                                      //Break sequence?
//      capsBreak = 0;                                                   //Yes.. clear the flag
      capsFlag = ~capsFlag;                                            //Toggle the caps lock flag
//   }
      
   if(capsFlag)                                                     //Turning caps lock on?
      KBSendCmd(CMD_SET_LED,ARG_CAPS);                              //Yes
   else
      KBSendCmd(CMD_SET_LED,ARG_NONE);                              //No, turn caps lock off
               
   while(!scanFlag);                                                //Wait for the KB to reply
   scanFlag = 0;
//      kbChar = KBGetChar();               
//               if(kbchar != KB_ACK)                                             //Get a good ACK?
//                  use enum list                           //No.. report it
   return 0;
}