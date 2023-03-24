
/*----------------------------------------------------------------------------*/
/* Interface to a PS2 keyboard via an input capture module                    */
/*                                                                            */
/* Resources Used:                                                            */
/* -External Interrupt 0                                                      */
/* -Timer 4                                                                   */
/* -Two 5V tolerant IO pins                                                   */
/*                                                                            */
/* Summary:                                                                   */
/* Keyboard runs at 5V with open collector pull-up resistors.                 */            
/*                                                                            */
/* PS2 clock line will be connected to external interrupt 0. PS2 data line    */
/* is connected to a 5V (digital) tolerant GPIO pin. Ext Int0 will trigger    */
/* on the falling edge of the clock line. Data is valid when clock is low.    */
/*                                                                            */
/* Timer 4 is used for a timeout period and will reset the state back to      */
/* START whenever it's ISR is entered                                         */
/*                                                                            */
/* In order to send a command to the keyboard, the clock line must first be   */
/* pulled low for a minimum of 100us. Then the data line is pulled low and the*/
/* clock line is released, at which time the keyboard suspends                */
/* transmission and waits for the command. If the keyboard receives an        */
/* invalid command, it responds with a "resend" (0xFE)                        */
/*                                                                            */
/* Data sent from the device to the host is read on the falling edge of the   */ 
/* clock signal; data sent from the host to the device is read on the rising  */
/* edge                                                                       */
/*                                                                            */
/* At power on or after a software reset, the keyboard runs it's Basic        */
/* Assurance Test (BAT). Upon completion of BAT, the keyboard will send 0xAA  */
/* for success, or 0xFC for an error                                          */
/*                                                                            */
/*                     BUS STATES                                             */
/* Data		   Clock		   State                              */
/* --------------------------------------------------------                   */
/* low		   low		   Communication stalled                      */
/* low	   	   high		   Host request to send                       */
/* high		   low		   Communication stalled                      */
/* high		   high		   Idle                                       */
/*----------------------------------------------------------------------------*/
/* 02/2023 Adam Hout    -Original code                                        */
/*----------------------------------------------------------------------------*/
#include "xc.h"
#include "ps2kb.h"
#include "sys.h"
#include <ctype.h>                                                              //For toupper()
#include <string.h>                                                             //For memset()
#include <libpic30.h>                                                           //For delay_us()
#include <stdlib.h>                                                             //For malloc())

/*------------------------------------------*/
/* Global variables                         */
/*------------------------------------------*/
volatile int           scanFlag = 0;                                            //=1 when a new scan code is received
volatile int           capsFlag = 0;                                            //Caps lock flag
volatile int           capsBreak = 0;                                           //Caps key released
volatile int           shiftFlag = 0;                                           //Left or right shift key flag
volatile int           breakCode = 0;                                           //Break code (0xF0) flag
volatile unsigned char scanCode;                                                //Scan code from the keyboard

int kbBitCnt;                                                                   //Bit counter for incoming scan codes
int kbParity;                                                                   //Compute parity

//Enums
PS2STATES_t PS2State;                                                           //Current state of operation

//FIFO buffer for translated output
queue_t xOutBuf, *pOutBuf;                                                     


//PS2 scan code lookup tables
const char ScanCodes[128] = {0,F9,0,F5,F1,F3,F2,F12,
                             0,F10,F8,F6,F4,TAB,'`',0,
                             0,0,L_SHIFT,0,L_CTRL,'q','1',0,
                             0,0,'z','s','a','w','2',0,
                             0,'c','x','d','e','4','3',0,
                             0,' ','v','f','t','r','5',0,
                             0,'n','b','h','g','y','6',0,
                             0,0,'m','j','u','7','8',0,
                             0,',','k','i','o','0','9',0,
                             0,'.','/','l',';','p','-',0,
                             0,0,'\'',0,'[','=',0,0,
                             CAPS,R_SHIFT,ENTER,']',0,0x5C,0,0,
                             0,0,0,0,0,0,BKSP,0,
                             0,'1',0,'4','7',0,0,0,
                             0,'.','2','5','6','8',ESC,NUMLOCK,
                             F11,'+','3','-','*','9',0,0};


const char ShiftScanCodes[128] = {0,F9,0,F5,F1,F3,F2,F12,
                                  0,F10,F8,F6,F4,TAB,'~',0,
                                  0,0,L_SHIFT,0,L_CTRL,'Q','!',0,
                                  0,0,'Z','S','A','W','@',0,
                                  0,'C','X','D','E','$','#',0,
                                  0,' ','V','F','T','R','%',0,
                                  0,'N','B','H','G','Y','^',0,
                                  0,0,'M','J','U','&','*',0,
                                  0,'<','K','I','O',')','(',0,
                                  0,'>','?','L',':','P','_',0,
                                  0,0,'|',0,'{','+',0,0,
                                  CAPS,R_SHIFT,ENTER,'}',0,'|',0,0,
                                  0,0,0,0,0,0,BKSP,0,
                                  0,'1',0,'4','7',0,0,0,
                                  0,'.','2','5','6','8',ESC,NUMLOCK,
                                  F11,'+','3','-','*','9',0,0};


/*------------------------------------------*/
/* Initialize input capture for the keyboard*/
/*------------------------------------------*/
void KBInit(void)
{
   //Init the PS2 data and clock pins
   PS2DATA_D = 1;                                                               //Set PS2 data pin to open drain
   PS2DATA_T = 0;                                                               //Set PS2 data pin to output (input can still be read)
   PS2DATA_L = 1;                                                               //Allow clock line to go high
   PS2CLOCK_D = 1;                                                              //Set PS2 clock line to open drain
   PS2CLOCK_T = 0;                                                              //Set PS2 clock pin to output (input can still be read)
   PS2CLOCK_L = 1;                                                              //Allow clock line to pull up
   PS2State = PS2START;                                                         //Set the machine state
   
   //Setup circular buffer for translated characters   
   pOutBuf = &xOutBuf;                                                          //Ref character queue
   pOutBuf->head = 0;                                                           //Initialize it
   pOutBuf->tail = 0;
   pOutBuf->count = 0;
   memset(&pOutBuf->buffer,0x00,BUFSIZE);
 
   //External interrupt 0 connected to PS2 KB clock pin
   INTCON2bits.INT0EP = 1;                                                      //Interrupt on falling edge
   IFS0bits.INT0IF = 0;                                                         //Clear Ext Int 0 interrupt flag
   IEC0bits.INT0IE = 1;                                                         //Enable external interrupt 0
}

void KBCheckFlags(void){
   
   static unsigned char prevCode;   
   
   
   switch(scanCode){
      case CAPS_S:
         break;
      case L_SHIFT_S:
         break;
      case R_SHIFT_S:
         break;
      case BREAK_S:
         if(prevCode == CAPS_S)                                           //Was this a caps lock break code?
            capsBreak = 1;                                                //Yes.. flag it
         else
            breakCode = 1;                                                //Treat as regular break code      
         break;
   }
   
   
   if(scanCode == CAPS_S){                                                      //Caps lock?                
   }
   if(scanCode == BREAK_S){                                           //Don't want break codes
      if(prevCode == CAPS_S)                                           //Was this a caps lock break code?
         capsBreak = 1;                                                //Yes.. flag it
      else
         breakCode = 1;                                                //Treat as regular break code 
   }
   else if(scanCode == L_SHIFT_S || scanCode == R_SHIFT_S){            //Capture shift keys
      if(breakCode){                                                   //Shift key released?
         breakCode = 0;                                                //Yes.. clear the break flag
         shiftFlag = 0;                                                //Clear the shift flag
      }
      else{
         shiftFlag = 1;                                                //Set the flag
      }
   }
   else if(breakCode){                                                //Don't want the byte following the break code either
      breakCode = 0;
   }
   
   prevCode = scanCode;    
}

/*----------------------------------------------------*/
/*Convert incoming scan codes via the translate tables*/
/*and store them in the circular output buffer        */
/*----------------------------------------------------*/
void KBConvertScanCode(){
   
   
   //Return response bytes as is
   if(scanCode == KB_BAT || scanCode == KB_ECHO ||
      scanCode == KB_ACK || scanCode == KB_FAIL ||
      scanCode == KB_FL2 || scanCode == KB_RSND ||
      scanCode == KB_ERR)
   {
      pOutBuf->buffer[pOutBuf->head++] = scanCode;                              //No conversion
   }
   else{   
      if (shiftFlag)                                                            //Shift key prior code sent?
         pOutBuf->buffer[pOutBuf->head++] = ShiftScanCodes[scanCode % 128];     //Yes.. use shift table
      else{									
         pOutBuf->buffer[pOutBuf->head++] = ScanCodes[scanCode % 128];          //Otherwise use standard table

         if (capsFlag && pOutBuf->buffer[pOutBuf->head] >= 'a' 
            && pOutBuf->buffer[pOutBuf->head] <= 'z')
             pOutBuf->buffer[pOutBuf->head] = 
                                toupper(pOutBuf->buffer[pOutBuf->head]);        //Caps lock on, convert to upper case
      }
      
      pOutBuf->head %= BUFSIZE;
      pOutBuf->count++;
   }
}

/*----------------------------------------------------*/
/*----------------------------------------------------*/
unsigned char KBGetChar(void){
   
   unsigned char rtnChr;
   
   rtnChr = 'x';
   
//   if (kbBufTail == kbBufHead)                                                  //Anything to return?
//      return 0;

//   ScanCode = kbScanBuf[kbBufTail++];                                            //Copy out the first scan code
//   kbBufTail %= BUFSIZE;                                                        //Wrap around if needed
   
   
   return rtnChr;   
   
}

/*-------------------------------------------------------------------*/
/*Return the last keyboard char received without altering the buffer */
/*-------------------------------------------------------------------*/
unsigned char KBPeek(){
   
   unsigned char rtnChr;
   
   
   rtnChr = 'x';
   
//   if (kbBufTail == kbBufHead)                                                  //Anything to return?
//      return 0;
//
//   if(kbBufHead > 0)                                                            //Check for first array element
//      ScanCode = kbScanBuf[kbBufHead-1];                                         //Beyond.. can back up one
//   else                                                                         //Head = element 0
//      ScanCode = kbScanBuf[BUFSIZE-1];                                           //Copy out the last array element
//   
//   //Return response bytes as is
//   if(ScanCode == KB_BAT || ScanCode == KB_ECHO ||
//      ScanCode == KB_ACK || ScanCode == KB_FAIL ||
//      ScanCode == KB_FL2 || ScanCode == KB_RSND ||
//      ScanCode == KB_ERR)
//   {
//      return ScanCode;                                                          //No conversion
//   }
//      
//   if (ShiftFlag)                                                               //Shift key prior code sent?
//      RtnChr = ShiftScanCodes[ScanCode % 128];                                  //Yes.. use shift table
//   else{									
//      RtnChr = ScanCodes[ScanCode % 128];                                       //Otherwise use standard table
//
//      if (CapsFlag && RtnChr >= 'a' && RtnChr <= 'z')
//         RtnChr = toupper(RtnChr);                                              //Caps lock on, convert to upper case
//   }
   
   return rtnChr;
}
/*---------------------------------------------------------------------*/
//         Send passed command to the keyboard     
//1)   Bring the Clock line low for at least 100 microseconds.
//2)   Bring the Data line low.
//3)   Release the Clock line.
//4)   Wait for the device to bring the Clock line low.
//5)   Set/reset the Data line to send the first data bit
//6)   Wait for the device to bring Clock high.
//7)   Wait for the device to bring Clock low.
//8)   Repeat steps 5-7 for the other seven data bits and the parity bit
//9)   Release the Data line.
//10) Wait for the device to bring Data low.
//11) Wait for the device to bring Clock  low.
//12) Wait for the device to release Data and Clock
/*---------------------------------------------------------------------*/
void KBSendCmd(unsigned char cmd, unsigned char arg)
{
   
   IEC0bits.INT0IE = 0;                                                         //Disable external Int0 while in command mode
   KBReqToSend();
   KBWriteByte(cmd);                                                            //Send passed command to the keyboard

   if (arg != NO_ARGS){                                                         //Send the command argument if one was passed. 0xFF indicates no ARG
      KBReqToSend();
      KBWriteByte(arg);
   }                                 
      
   IFS0bits.INT0IF = 0; 
   IEC0bits.INT0IE = 1;                                                         //Enable ext int 0
}

/*------------------------------------------*/
/* Initiates a write to the keyboard        */
/*------------------------------------------*/
void KBReqToSend(){
   PS2CLOCK_L = 0;                                                              //Clock line needs pulled low for a minimum of 100us 
   __delay_us(100);                                                             
   PS2DATA_L = 0;                                                               //Pull data line low (start bit)
   __delay_us(20);                                                              //Hold it for 20us
   PS2CLOCK_L = 1;                                                              //Allow clock line to go high again
}

/*------------------------------------------*/
/* Send a byte to the keyboard (only used   */
/* by command function                      */
/*------------------------------------------*/
void KBWriteByte(unsigned char Byte)
{
   int ctr;
   int parity=0;                                             

   /*----------------------------------*/
   /* Shift in the passed command. The */
   /* keyboard will control the clock. */
   /* Pass data when clock line is low */
   /*----------------------------------*/
   for (ctr=0x01; ctr<=0x80; ctr*=2){
      while(PS2CLOCK_P);                                                        //Wait for keyboard to pull clock line back low

      if (ctr & Byte){                                                           //CMD bit high?
         PS2DATA_L = 1;                                                         //Yes.. set data line high
         parity++;
      }
      else
         PS2DATA_L = 0;                                                         //Otherwise set data line low

      while(!PS2CLOCK_P);                                                       //Wait for keyboard to release clock line high
   }

   /*----------------------------------*/
   /* PS2 uses odd parity. If even # of*/
   /* 1's in command byte, then send 1 */
   /* for parity, otherwise send a 0   */
   /*----------------------------------*/
   while(PS2CLOCK_P);                                                           //Wait for keyboard to pull clock line back low

   if ((parity % 2) == 0)                                                       //Remainder from division?
      PS2DATA_L = 1;                                                            //No.. even nbr of 1's, set parity to 1
   else 
      PS2DATA_L = 0;                                                            //Yes.. odd nbr of 1's, set parity to 0

   /*----------------------------------*/
   /* Send the stop bit (always 1)     */
   /*----------------------------------*/
   while(!PS2CLOCK_P);                                                          //Wait for keyboard to clock in the parity bit
   while(PS2CLOCK_P);                                                           //Wait for keyboard to pull clock line back low
   PS2DATA_L = 1;                                                               //Stop bit
   while(!PS2CLOCK_P);                                                          //Wait for keyboard to clock in the stop bit

   /*----------------------------------*/
   /* Get the ACK from the keyboard    */
   /* ACK is sent when the clock line  */
   /* is high                          */
   /*----------------------------------*/
   while (PS2DATA_P);                                                           //Wait for keyboard to pull data line low (ACK bit)
   while (PS2CLOCK_P);                                                          //Wait for keyboard to pull clock line low
   while (!PS2CLOCK_P);                                                         //Wait for keyboard to release clock line high
   while (!PS2DATA_P);                                                          //Wait for the keyboard to release the data line high
   return;
}

/*------------------------------------------*/
/* External Interrupt 0 ISR (PS2 Clock pin) */
/* State machine to receive scan codes from */
/* a PS2 keyboard                           */
/*------------------------------------------*/
void __attribute ((interrupt, no_auto_psv)) _INT0Interrupt(void)
{
   switch (PS2State){	
      case PS2START:                                                            //Start state
         if (!PS2DATA_P){                                                       //Data pin low for the start bit  
            kbBitCnt = 8;                                                       //Init bit counter	
            kbParity = 0;                                                       //Init parity check
            PS2State = PS2BIT;                                                  //Bump to the next state
         }
         break;
         
      case PS2BIT:                                                              //Data bit state
         scanCode >>= 1;                                                        //Shift scan code bits
			
         if (PS2DATA_P)                                                         //Data line high?
            scanCode += 0x80;                                                   //Yes.. turn on most significant bit in scan code buffer

         kbParity ^= scanCode;                                                  //Update parity
			
         if (--kbBitCnt == 0)                                                   //If all scan code bits read
            PS2State = PS2PARITY;                                               //Change state to parity
         break;
      
      case PS2PARITY:                                                           //Parity state
         if (PS2DATA_P)
            kbParity ^= 0x80;					

         if (kbParity & 0x80)                                                   //Continue if parity is odd
            PS2State = PS2STOP;
         else
            //set error
            PS2State = PS2START;
         break;

      case PS2STOP:                                                             //Stop state
         if (PS2DATA_P){
            scanFlag = 1;
            PS2State = PS2START;                                                   //Reset to start state
            break;  
         }
         //else
            //set error
		
      default:
         //set error
         PS2State = PS2START;
         break;
   }

   IFS0bits.INT0IF = 0;                                                         //Reset int0 flag	
   return;
}
