/****************************************************************************************
*   @file system.c
*   
*   @author  Supreet Gulavani (sg7@pdx.edu)
*   @copyright Supreet Gulavani, 2023
*
*   Modified from the original file by Prof. Roy Kravitz at Portland State University.
*      
*
*******************************************************************************************/

/****************Header Files*****************/
#include "platform.h"
#include "system.h"
#include "nexys4io.h"
#include "PmodENC.h"
#include "PmodOLEDrgb.h"

#include "xparameters.h"
#include "xintc.h"
#include "xtmrctr.h"
#include "xgpio.h"

// Peripheral Instances
PmodENC 	pmodENC_inst;
XIntc 		IntrptCtlrInst;				
XWdtTb      WDTTimerInst;
XTmrCtr     AXITimerInst;

volatile uint8_t force_crash = 0;

/****************************************************************************/
/**
* initialize the system
*
* This function is executed once at start-up and after resets.  It initializes
* the peripherals and registers the interrupt handler(s)
*****************************************************************************/

int do_init(void)
{
	uint32_t status;				// status from Xilinx Lib calls

	// initialize the Nexys4 driver and (some of)the devices
	status = (uint32_t) NX4IO_initialize(NX4IO_BASEADDR);
	if (status != XST_SUCCESS)
	{
		return XST_FAILURE;
	}

	// set all of the display digits to blanks and turn off
	// the decimal points using the "raw" set functions.
	// These registers are formatted according to the spec
	// and should remain unchanged when written to Nexys4IO...
	// something else to check w/ the debugger when we bring the
	// drivers up for the first time
	NX4IO_SSEG_setSSEG_DATA(SSEGHI, 0x0058E30E);
	NX4IO_SSEG_setSSEG_DATA(SSEGLO, 0x00144116);

	// Initialize the pmodENC and hardware
	ENC_begin(&pmodENC_inst, PMODENC_BASEADDR);

	status = AXI_Timer_initialize();
	if (status != XST_SUCCESS)
	{
		return XST_FAILURE;
	}

	// initialize the interrupt controller
	status = XIntc_Initialize(&IntrptCtlrInst, INTC_DEVICE_ID);
	if (status != XST_SUCCESS)
	{
	   return XST_FAILURE;
	}
    
    
    // connect the WDT handler to the interrupt
	status = XIntc_Connect(&IntrptCtlrInst, WDT_INTERRUPT_ID,
						   (XInterruptHandler)WDT_Handler,
						   (void *)0);
	if (status != XST_SUCCESS)
	{
		return XST_FAILURE;

	}

    /* Initialize watchdog timer */
	xil_printf("Initializing watchdog timer\r\n");
	xStatus = XWdtTb_Initialize(&WDTTimerInst, XPAR_WDTTB_0_DEVICE_ID);
	if (xStatus == XST_FAILURE)
	{
		xil_printf("Failed to initialize watchdog timer\r\n");
	}

    // Start the WDT
	XWdtTb_Start(&WDTTimerInst); 

    // Install the handler defined in this task for the button input.
	xPortInstallInterruptHandler( WDT_INTR_NUM, wdt_handler, NULL );
	vPortEnableInterrupt( WDT_INTR_NUM );
    
/*
	// connect the fixed interval timer (FIT) handler to the interrupt
	status = XIntc_Connect(&IntrptCtlrInst, FIT_INTERRUPT_ID,
						   (XInterruptHandler)FIT_Handler,
						   (void *)0);
	if (status != XST_SUCCESS)
	{
		return XST_FAILURE;

	}

	// start the interrupt controller such that interrupts are enabled for
	// all devices that cause interrupts.
	status = XIntc_Start(&IntrptCtlrInst, XIN_REAL_MODE);
	if (status != XST_SUCCESS)
	{
		return XST_FAILURE;
	}
    */

	// enable the WDT interrupt
	XIntc_Enable(&IntrptCtlrInst, WDT_INTERRUPT_ID);
	return XST_SUCCESS;
}

int AXI_Timer_initialize(void){

	uint32_t status;				// status from Xilinx Lib calls
	uint32_t		ctlsts;		// control/status register or mask

	status = XTmrCtr_Initialize(&AXITimerInst,AXI_TIMER_DEVICE_ID);
		if (status != XST_SUCCESS) {
			return XST_FAILURE;
		}
	status = XTmrCtr_SelfTest(&AXITimerInst, TMR_CTR_NUM);
		if (status != XST_SUCCESS) {
			return XST_FAILURE;
		}
	ctlsts = XTC_CSR_AUTO_RELOAD_MASK | XTC_CSR_EXT_GENERATE_MASK | XTC_CSR_LOAD_MASK |XTC_CSR_DOWN_COUNT_MASK ;
	XTmrCtr_SetControlStatusReg(AXI_TIMER_BASEADDR, TMR_CTR_NUM,ctlsts);

	//Set the value that is loaded into the timer counter and cause it to be loaded into the timer counter
	XTmrCtr_SetLoadReg(AXI_TIMER_BASEADDR, TMR_CTR_NUM, 24998);
	XTmrCtr_LoadTimerCounterReg(AXI_TIMER_BASEADDR, TMR_CTR_NUM);
	ctlsts = XTmrCtr_GetControlStatusReg(AXI_TIMER_BASEADDR, TMR_CTR_NUM);
	ctlsts &= (~XTC_CSR_LOAD_MASK);
	XTmrCtr_SetControlStatusReg(AXI_TIMER_BASEADDR, TMR_CTR_NUM, ctlsts);

	ctlsts = XTmrCtr_GetControlStatusReg(AXI_TIMER_BASEADDR, TMR_CTR_NUM);
	ctlsts |= XTC_CSR_ENABLE_TMR_MASK;
	XTmrCtr_SetControlStatusReg(AXI_TIMER_BASEADDR, TMR_CTR_NUM, ctlsts);

	XTmrCtr_Enable(AXI_TIMER_BASEADDR, TMR_CTR_NUM);
	return XST_SUCCESS;

}


