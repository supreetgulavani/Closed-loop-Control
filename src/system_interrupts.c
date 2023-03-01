/**
* @file system_interrupts.c
*
* @author Omkar Jadhav (omjadhav@pdx.edu) Supreet Gulavani (sg7@pdx.edu@pdx.edu)
* @copyright Supreet Gulavani, 2023
*
*@note File contains System Interrupt Handlers
*******************************************************************************/

/************************Header Files****************************************/
#include "platform.h"
#include "nexys4io.h"

/************************************Variables*******************************/
extern volatile bool newbtnsSw; // true if the FIT handler updated global buttons and switch values
extern volatile uint16_t sw;	// switches - set in the FIT handler
extern volatile uint8_t btns;	// buttons - set in the FIT handler

/********** Interrupt Handlers **********/

/****************************************************************************/
/**
* Fixed interval timer interrupt handler
*
* Reads the switches and sets the handshaking signal if any changes.
* Reads the button and sets the handshaking signal if any changes.
* Checks/sets the global newbtnsSw which is the handshake between the interrupt handler and main
*
* @note:  pushbutton mapping: {0 0 0 btnC btnU btnD btnL btnR}
*
* @note  This handler should be called about twice per second
*****************************************************************************/
void FIT_Handler(void)
{
	static bool isInitialized = false;	// true if the function has run at least once
	static uint8_t prevBtns;			// previous value of button register
	static uint16_t prevSw;				// previous value of the switch register
//	static bool dpOn;					// true if decimal point 0 is on

	// initialize the static variables the first time the function is called
	if (!isInitialized) {
		prevBtns = 0x8F;	// invert btns to get everything started
		prevSw = 0xFFFF;	// invert switches to get everything started
		//dpOn = true;
		isInitialized = true;
	}

	// return if main() hasn't handled the last button and switch changes
	if (newbtnsSw) {
		return;
	}

	// get the value of the switches.  Will be written to the LEDs in main()
	sw =  NX4IO_getSwitches();
	if (prevSw != sw) {
		newbtnsSw = true;
		prevSw = sw;
	}

	// get the value of the buttons.  Will be written to the 7-segment decimal points in main()
	btns = NX4IO_getBtns ();
	if (prevBtns != btns) {
		newbtnsSw = true;
		prevBtns = btns;
	}
}

/* In WDT interrupt handler
void wdt_handler(void *pvUnused)
{
	//if system running flag is set, restart the watch dog timer
	if(sys_flag == 1)
	{
		XWdtTb_RestartWdt(&WDTTimerInst);
	}
}*/
