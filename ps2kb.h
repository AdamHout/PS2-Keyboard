/* 
 * File:   ps2kb.h
 * Author: adam
 *
 * Created on January 25, 2023, 11:08 PM
 */

#ifndef PS2KB_H
#define	PS2KB_H

/*----------------------------------------------------*/
/* Defines                                            */
/*----------------------------------------------------*/
#define PS2DATA_D   ODCBbits.ODB6                                               //Open drain control for PS2 data line
#define PS2DATA_T   TRISBbits.TRISB6                                            //Tris control for the data line
#define PS2DATA_L   LATBbits.LATB6                                              //Lat control for writing to the data line (command mode only)
#define PS2DATA_P   PORTBbits.RB6                                               //Port control for reading the data line

#define PS2CLOCK_D	ODCBbits.ODB7	                                            //Open drain control for PS2 clock line
#define PS2CLOCK_T	TRISBbits.TRISB7                                            //Tris register control for PS2 clock line
#define PS2CLOCK_L	LATBbits.LATB7                                              //Lat control for controling the clock line (command mode only)
#define PS2CLOCK_P	PORTBbits.RB7                                               //External interrupt 0 for PS2 clock line

//#define PS2START  	0                                                           //Start bit state
//#define PS2BIT		1                                                           //Shift bit state
//#define PS2PARITY	2                                                           //Parity bit state
//#define PS2STOP		3                                                           //Stop bit state
#define PS2TO       6000                                                        //Timeout period. 1.5ms @ FCY = 

#define BUFSIZE     512                                                         //FIFO/circular buffer size in bytes

//ASCII values for look-up table constants
#define BKSP        0x08                                                        //Backspace
#define TAB			0x09                                                        //Tab key						
#define ENTER		0x0D                                                        //Enter key - Carriage return
#define CAPS		0x11                                                        //Caps lock key - Using DC1
#define ESC			0x1B                                                        //Escape key
#define L_SHIFT		0x12                                                        //Left shift key
#define R_SHIFT		0x59                                                        //Right shift key
#define L_CTRL		0x00                                                        //Left control key (not defined at this time)
#define NUMLOCK		0x00                                                        //Number lock key (not defined at this time)
#define F1			0x00                                                        //F1 function key (not defined at this time)
#define F2			0x00                                                        //F2 function key (not defined at this time)
#define F3			0x00                                                        //F3 function key (not defined at this time)
#define F4			0x00                                                        //F4 function key (not defined at this time)
#define F5			0x00                                                        //F5 function key (not defined at this time)
#define F6			0x00                                                        //F6 function key (not defined at this time)
#define F7			0x00                                                        //F7 function key (not defined at this time)
#define F8			0x00                                                        //F8 function key (not defined at this time)
#define F9			0x00                                                        //F9 function key (not defined at this time)
#define F10			0x00                                                        //F10 function key (not defined at this time)
#define F11			0x00                                                        //F11 function key (not defined at this time)
#define F12			0x00                                                        //F12 function key (not defined at this time)

//PS2 scan code constants
#define BREAK_S     0XF0                                                        //Break code
#define TAB_S       0x0D                                                        //Tab key						
#define BKSP_S      0x66                                                        //Backspance key
#define ENTER_S     0x5A                                                        //Enter key
#define ESC_S       0x76                                                        //Escape key
#define L_SHIFT_S   0x12                                                        //Left shift key
#define R_SHIFT_S   0x59                                                        //Right shift key
#define CAPS_S      0x58                                                        //Caps lock key

//Keyboard commands
#define CMD_ECHO     0xEE                                                       //Keyboard responds with echo (0xEE)
#define CMD_DEVID    0xF2                                                       //Read device ID. Keyboard responds with 2 byte ID
#define CMD_RESET    0xFF                                                       //Reset. Keyboard acks back with 0xFA and enteres BAT
#define CMD_RESEND   0xFE                                                       //Resend. Keyboard resends last byte sent to the host
#define CMD_CODE_SET 0xF0                                                       //Requests scan code set. Keyboard responds with ack (0xFA) the waits for a 1 byte
                                                                                //argument of 0x01, 0x02, 0x03 to set the scan code table used. If 0x00 is passed
                                                                                //the keyboard responds with ack followed by the current scan code sent
#define CMD_SET_LED  0xED                                                       //Followed with a 1 byte argument that defines the state of the keyboard LED's. 
                                                                                //Always Always Always Always Always Caps  Num  Scroll 
                                                                                //  0      0      0      0      0    Lock  Lock Lock

//Keyboard command arguments
#define ARG_NONE    0x00                                                        //All LED's off
#define ARG_SCROLL  0x01                                                        //Only Scroll lock LED on
#define ARG_NUM     0x02                                                        //Only Num lock LED on
#define ARG_NUM_SCR 0x03                                                        //Nums and scroll lock LED's on
#define ARG_CAPS    0x04                                                        //Only Caps lock LED on
#define ARG_CAP_SCR 0x05                                                        //Caps and scroll lock LED's on
#define ARG_CAP_NUM 0x06                                                        //Caps and num lock LED's on
#define ARG_ALL     0x07                                                        //All LED's on
#define NO_ARGS     0xFF                                                        //Passing no arguments to the command function

//Keyboard responses
#define KB_BAT  0xAA                                                            //Sent after sucessful basic assurance test
#define KB_ECHO 0xEE                                                            //Echo reply from keyboard
#define KB_ACK  0xFA                                                            //Command acknowledge
#define KB_FAIL 0xFC                                                            //Keyboard BAT failed
#define KB_FL2  0xFD                                                            //Keyboard BAT failed (not sure why there are two codes)
#define KB_RSND 0xFE                                                            //Resend (keyboard wants controller to repeat last command it sent)
#define KB_ERR  0xFF                                                            //Key detection error or internal buffer overrun

/*----------------------------------------------------*/
/* Typedefs                                           */
/*----------------------------------------------------*/
typedef enum {PS2START,                                                         //Start bit
              PS2BIT,                                                           //Data bit(s)
              PS2PARITY,                                                        //Parity bit
              PS2STOP} PS2STATES_t;                                             //Stop bit 
                     
typedef struct{                                                                 //FIFO queue typedef
    int head;                                                                   //Head subscript
    int tail;                                                                   //Tail subscript
    int count;                                                                  //queued item count
    unsigned char buffer[BUFSIZE]; 
}queue_t;

/*----------------------------------------------------*/
/* Function declarations                              */
/*----------------------------------------------------*/
void			KBInit(void);                                                   //Init INT0 and I/O
unsigned char   KBGetChar(void);                                                //Pulls the next scan code from the buffer and converts to ASCII
unsigned char   KBPeek(void);                                                   //Returns last scan code received w/o altering the buffer
void            KBConvertScanCode(void);                                        //Translate scan codes via the lookup table
void            KBSendCmd(unsigned char, unsigned char);                        //Send command to KB
void            KBReqToSend(void);                                              //Generates request to send (start bit)to the keyboard
void 		   	KBWriteByte(unsigned char);                           

void KBCheckFlags(void);

#endif	/* PS2KB_H */

