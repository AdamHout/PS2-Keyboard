/*-------------------------------------------------------------------------------*/
/* Function to set unused pins on a PIC24FJ64GA002 as outputs and drive them low */
/*-------------------------------------------------------------------------------*/
#include "xc.h"
#include "sup.h"


void SetUnusedPins(void)
{
											      //Pin 1 - Reset
	
//	AD1PCFGbits.PCFG0 = 1;					                //Pin 2 - AN0/VREF+/CN2/RA0 (configure as digital)
//	TRISAbits.TRISA0 = 0;					                //Set to output
//	LATAbits.LATA0 = 0;						        //Drive pin low

//	AD1PCFGbits.PCFG1 = 1;					                //Pin 3 - AN1/VREF-/CN3/RA1
//	TRISAbits.TRISA1 = 0;					
//	LATAbits.LATA1 = 0;
	
//	AD1PCFGbits.PCFG2 = 1;					                //Pin 4 - PGD1/EMUD1/AN2/C2IN-/RP0/CN4/RB0
//	TRISBbits.TRISB0 = 0;
//	LATBbits.LATB0 = 0;		

//	AD1PCFGbits.PCFG3 = 1;					                //Pin 5 - PGC1/EMUC1/AN3/C2IN+/RP1/CN5/RB1
//	TRISBbits.TRISB1 = 0;
//	LATBbits.LATB1 = 0;

	AD1PCFGbits.PCFG4 = 1;					                //Pin 6 - AN4/C1IN-/RP2/SDA2/CN6/RB2
	TRISBbits.TRISB2 = 0;
	LATBbits.LATB2 = 0;

	AD1PCFGbits.PCFG5 = 1;					                //Pin 7 - AN5/C1IN+/RP3/SCL2/CN7/RB3
	TRISBbits.TRISB3 = 0;
	LATBbits.LATB5 = 0;		
                                                                                //Pin 8 - VSS

//	TRISAbits.TRISA2 = 0;					                //Pin 9 - OSCI/CLKI/CN30/RA2
//	LATAbits.LATA = 0;

//	TRISAbits.TRISA3 = 0;					                //Pin 10 - OSCO/CLKO/CN29/PMA0/RA3
//	LATAbits.LATA3 = 0;

	TRISBbits.TRISB4 = 0;					                //Pin 11 - SOSCI/RP4/PMBE/CN1/RB4
	LATBbits.LATB4 = 0;

	TRISAbits.TRISA4 = 0;					                //Pin 12 - SOSCO/T1CK/CN0/PMA1/RA4
	LATAbits.LATA4 = 0;			
                                                                                //Pin 13 - VDD

	TRISBbits.TRISB5 = 0;					                //Pin 14 - PGD3/EMUD3/RP5/SDA1(2)/CN27/PMD7/RB5
	LATBbits.LATB5 = 0;

//	TRISBbits.TRISB6 = 0;					                //Pin 15 - PGC3/EMUC3/RP6/SCL1(2)/CN24/PMD6/RB6
//	LATBbits.LATB6 = 0;			

//	TRISBbits.TRISB7 = 0;					                //Pin 16 - RP7/INT0/CN23/PMD5/RB7
//	LATBbits.LATB7 = 0;

	TRISBbits.TRISB8 = 0;					                //Pin 17 - TCK/RP8/SCL1/CN22/PMD4/RB8
	LATBbits.LATB8 = 0;

	TRISBbits.TRISB9 = 0;					                //Pin 18 - TDO/RP9/SDA1/CN21/PMD3/RB9
	LATBbits.LATB9 = 0;				
                                                                                //Pin 19 - DISVREG
                                                                                //Pin 20 - Vcap / VDDCore

//	TRISBbits.TRISB10 = 0;					                //Pin 21 - PGD2/EMUD2/TDI/RP10/CN16/PMD2/RB10
//	LATBbits.LATB10 = 0;

//	TRISBbits.TRISB11 = 0;					                //Pin 22 - PGC2/EMUC2/TMS/RP11/CN15/PMD1/RB11
//	LATBbits.LATB11 = 0;
//
//	AD1PCFGbits.PCFG12 = 1;					                //Pin 23 - AN12/RP12/CN14/PMD0/RB12
//	TRISBbits.TRISB12 = 0;
//	LATBbits.LATB12 = 0;

	AD1PCFGbits.PCFG11 = 1;					                //Pin 24 - AN11/RP13/CN13/PMRD/RB13
	TRISBbits.TRISB13 = 0;
	LATBbits.LATB13 = 0;

	AD1PCFGbits.PCFG10 = 1;					                //Pin 25 - AN10/CVREF/RTCC/RP14/CN12/PMWR/RB14
	TRISBbits.TRISB14 = 0;
	LATBbits.LATB14 = 0;

	AD1PCFGbits.PCFG9 = 1;					                //Pin 26 - AN9/RP15/CN11/PMCS1/RB15
	TRISBbits.TRISB15 = 0;
	LATBbits.LATB15 = 0;
                                                                                //Pin 27 - VSS
                                                                                //Pin 28 - VDD
	return;
}

