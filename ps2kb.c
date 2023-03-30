
/*----------------------------------------------------------------------------*/
/* Interface to a PS2 keyboard via an external interrupt                      */
/*                                                                            */
/* Resources Used:                                                            */
/* -External Interrupt 0                                                      */
/* -5V tolerant IO pin                                                        */
/*                                                                            */
/* Summary:                                                                   */
/* Keyboard runs at 5V with open collector pull-up resistors. The MCU data    */            
/* and clock pins are configured as an open drain output and can still be    */
/* read as inputs. No need for direction control                              */
/*                                                                            */
/* PS2 clock line will be connected to external interrupt 0. PS2 data line    */
/* is connected to a 5V (digital) tolerant GPIO pin. Ext Int0 will trigger    */
/* on the falling edge of the clock line. Data is valid when clock is low.    */
/*                                                                            */
/* In order to send a command to the keyboard, the clock line must first be   */
/* pulled low for a minimum of 100us. Then the data line is pulled low and the*/
/* clock line is released, at which time the keyboard suspends                */
/* transmission and waits for the command. If the keyboard receives an        */
/* invalid command, it responds with a "resend" (0xFE)                        */
/*                                                                            */
/* Data sent from the device to the host is read on the falling edge of the   */ 
/* clock signal. Data sent from the host to the device is read on the rising  */
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
/* 03/2023 Adam Hout    -Original code                                        */
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
volatile unsigned char scanCode;                                                //Scan code from the keyboard

int capsLock = 0;                                                               //Caps lock status; 1 = On
int numsLock = 0;
int kbBitCnt;                                                                   //Bit counter for incoming scan codes
int kbParity;                                                                   //Compute parity

//FIFO buffer for translated output
queue_t xOutBuf, *pOutBuf; 

//Keyboard flags
kbFlags_t xFlags, *pFlags;

//Enums
ps2States_t ps2State;                                                           //Current state of operation
kbErrors_t  kbError;                                                            //Keyboard errors

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
/* Setup the keyboard                       */
/*------------------------------------------*/
int kbInitialize(void){
   
   //Init the PS2 data and clock pins
   PS2DATA_D = 1;                                                               //Set PS2 data pin to open drain
   PS2DATA_T = 0;                                                               //Set PS2 data pin to output (input can still be read)
   PS2DATA_L = 1;                                                               //Allow data line to go high
   PS2CLOCK_D = 1;                                                              //Set PS2 clock line to open drain
   PS2CLOCK_T = 0;                                                              //Set PS2 clock pin to output (input can still be read)
   PS2CLOCK_L = 1;                                                              //Allow clock line to pull up
   ps2State = PS2START;                                                         //Set the machine state
   
   //Setup circular buffer for translated characters   
   pOutBuf = &xOutBuf;                                                          //Ref character queue
   pOutBuf->head = 0;                                                           //Initialize it
   pOutBuf->tail = 0;
   pOutBuf->count = 0;
   memset(&pOutBuf->buffer,0x00,BUFSIZE);
   
   //Setup the keyboard flags structure 
   pFlags = &xFlags;
   memset(pFlags,0x00,sizeof(xFlags));
 
   //External interrupt 0 connected to PS2 KB clock pin
   INTCON2bits.INT0EP = 1;                                                      //Interrupt on falling edge
   IFS0bits.INT0IF = 0;                                                         //Clear Ext Int 0 interrupt flag
   IEC0bits.INT0IE = 1;                                                         //Enable external interrupt 0
   
   //Send an echo to the keyboard
   return kbEcho();
}

void kbCheckFlags(void){
   
   static unsigned char prevCode=0;                                             //Previous scan code
   
   if(scanCode == CAPS_S || scanCode == NUM_S)                                  //Caps or num lock?
      pFlags->skipFlag = 1;                                                     //Yes.. set it on the break
   else if(scanCode == L_SHIFT_S|| scanCode == R_SHIFT_S){                      //Shift key?
      if(pFlags->breakFlag)                                                     //Break sequence?
         pFlags->shiftFlag = 0;                                                 //Yes.. clear the shift flag
      else{
         pFlags->shiftFlag = 1;                                              
         pFlags->skipFlag = 1;                                                  //Discard this code
      }
   }
   else if(scanCode == BREAK_S){                                                //Break code?
      if(prevCode == CAPS_S)                                                    //Caps lock break sequence?
         pFlags->capsFlag = 1;                                                          //Yes.. set its flag
      else if(prevCode == NUM_S)
         pFlags->numsFlag = 1;
      else
         pFlags->breakFlag = 2;                                                         //Discard the break sequence
   }
   
   prevCode = scanCode;                                                         //Save this scan code to compare to the next one
}

void kbSetLocks(void){
   
   if(pFlags->capsFlag)                                                         //Toggle the appropriate lock
      capsLock = ~capsLock;                                                     
   else
      numsLock = ~numsLock;
      
   if(capsLock && numsLock)                                                     //Caps and Nums lock?
      kbSendCmd(CMD_SET_LED,ARG_CAP_NUM);
   else if(capsLock)                                                            //Just caps lock
      kbSendCmd(CMD_SET_LED,ARG_CAPS);                              
   else if(numsLock)                                                            //Just nums lock
      kbSendCmd(CMD_SET_LED,ARG_NUM);
   else
      kbSendCmd(CMD_SET_LED,ARG_NONE);                                          //All led's off
               
   while(!pFlags->scanFlag);                                                    //Wait for the KB to ACK
   pFlags->scanFlag = 0;
   if(scanCode != KB_ACK){
      kbError = ERR_LCK_NOACK;
      pFlags->errFlag = 1;
   }     
}

/*----------------------------------------------------*/
/*Convert incoming scan codes via the translate tables*/
/*and store them in the circular output buffer        */
/*----------------------------------------------------*/
void kbPostCode(void){
   
   //Return response bytes as is
   if(scanCode == KB_BAT || scanCode == KB_ECHO ||
      scanCode == KB_ACK || scanCode == KB_FAIL ||
      scanCode == KB_FL2 || scanCode == KB_RSND ||
      scanCode == KB_ERR)
   {
      pOutBuf->buffer[pOutBuf->head++] = scanCode;                              //No conversion
   }
   else{   
      if (pFlags->shiftFlag)                                                    //Shift key prior code sent?
         pOutBuf->buffer[pOutBuf->head++] = ShiftScanCodes[scanCode % 128];     //Yes.. use shift table
      else{									
         pOutBuf->buffer[pOutBuf->head++] = ScanCodes[scanCode % 128];          //Otherwise use standard table

         if (capsLock && pOutBuf->buffer[pOutBuf->head] >= 'a' 
            && pOutBuf->buffer[pOutBuf->head] <= 'z')
             pOutBuf->buffer[pOutBuf->head] = 
                                toupper(pOutBuf->buffer[pOutBuf->head]);        //Caps lock on, convert to upper case
      }
      
      pOutBuf->count++;
      pOutBuf->head %= BUFSIZE;
      if(pOutBuf->head == pOutBuf->tail){
         kbError = ERR_OVERFLOW;
         pFlags->errFlag =1;
      }
   }
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
void kbSendCmd(unsigned char cmd, unsigned char arg)
{
   
   IEC0bits.INT0IE = 0;                                                         //Disable external Int0 while in command mode
   kbReqToSend();
   kbWriteByte(cmd);                                                            //Send passed command to the keyboard

   if (arg != NO_ARGS){                                                         //Send the command argument. 0xFF indicates no ARG
      kbReqToSend();
      kbWriteByte(arg);
   }                                 
      
   IFS0bits.INT0IF = 0; 
   IEC0bits.INT0IE = 1;                                                         //Enable ext int 0
}

/*------------------------------------------*/
/* Initiates a write to the keyboard        */
/*------------------------------------------*/
void kbReqToSend(){
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
void kbWriteByte(unsigned char Byte)
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
int kbEcho(void){
   
   unsigned char retryCnt = 0;                                                  
   
   do{
      kbSendCmd(CMD_ECHO,NO_ARGS);                                              //Send an echo command
      while(!pFlags->scanFlag);                                                         //Wait for the keyboard to reply
      pFlags->scanFlag = 0;                                                             //Clear the flag
   }while(scanCode != CMD_ECHO && retryCnt++ < 3);                              //Up to three attempts
   
   if(scanCode != CMD_ECHO)                                                     //Success?
      return ERR_ECHO;                                                          //No, set error code
   else 
      return ERR_NONE;                                                          //Echo passed
}

/*------------------------------------------*/
/* External Interrupt 0 ISR (PS2 Clock pin) */
/* State machine to receive scan codes from */
/* a PS2 keyboard                           */
/*------------------------------------------*/
void __attribute ((interrupt, no_auto_psv)) _INT0Interrupt(void)
{
   switch (ps2State){	
      case PS2START:                                                            //Start state
         if (!PS2DATA_P){                                                       //Data pin low for the start bit  
            kbBitCnt = 8;                                                       //Init bit counter	
            kbParity = 0;                                                       //Init parity check
            ps2State = PS2BIT;                                                  //Bump to the next state
         }
         break;
         
      case PS2BIT:                                                              //Data bit state
         scanCode >>= 1;                                                        //Shift scan code bits
			
         if (PS2DATA_P)                                                         //Data line high?
            scanCode += 0x80;                                                   //Yes.. turn on most significant bit in scan code buffer

         kbParity ^= scanCode;                                                  //Update parity
			
         if (--kbBitCnt == 0)                                                   //If all scan code bits read
            ps2State = PS2PARITY;                                               //Change state to parity
         break;
      
      case PS2PARITY:                                                           //Parity state
         if (PS2DATA_P)
            kbParity ^= 0x80;					

         if (kbParity & 0x80)                                                   //Continue if parity is odd
            ps2State = PS2STOP;
         else{
            kbError = ERR_PARITY;                                               //Set parity error
            pFlags->errFlag = 1;                                                //Flag it  
            ps2State = PS2START;
         }
         break;

      case PS2STOP:                                                             //Stop state
         if (PS2DATA_P){                                                        //Stop bit?
            pFlags->scanFlag = 1;                                               //Good scan code
            ps2State = PS2START;                                                //Reset to start state
            break;  
         }
         else{                                                                  //Invalid stop bit
            kbError = ERR_STOP;
            pFlags->errFlag = 1;
         }
		
      default:
         kbError = ERR_INV_STATE;                                               //Should not get here
         pFlags->errFlag = 1;
         ps2State = PS2START;
         break;
   }

   IFS0bits.INT0IF = 0;                                                         //Reset int0 flag	
   return;
}
