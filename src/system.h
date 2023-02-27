/****************************************************************************************
*   @file system.h
*   
*   @author Omkar Jadhav (omjadha@pdx.edu)  Supreet Gulavani (sg7@pdx.edu)
*   @copyright Omkar Jadhav, Supreet Gulavani, 2023
*
*   Modified from the original file by Prof. Roy Kravitz at Portland State University.
*   
*   @note This file has all function prototypes   
*
*******************************************************************************************/
#ifndef __SYSTEM_H__
#define __SYSTEM_H__

/******************Header files***************************/
#include <stdio.h>
#include <stdlib.h>

#include <platform.h>

#include "xil_printf.h"
#include "xparameters.h"
#include "xstatus.h"
#include "microblaze_sleep.h"
#include "xtmrctr.h"
#include "xintc.h"
#include "xwdttb.h"

#include "nexys4io.h"
#include "xgpio.h"



/*********** Peripheral-related constants **********/
// Clock frequencies
#define CPU_CLOCK_FREQ_HZ		XPAR_CPU_CORE_CLOCK_FREQ_HZ
#define AXI_CLOCK_FREQ_HZ		XPAR_CPU_M_AXI_DP_FREQ_HZ

// Definitions for peripheral NEXYS4IO
#define N4IO_DEVICE_ID		    XPAR_NEXYS4IO_0_DEVICE_ID
#define N4IO_BASEADDR		    XPAR_NEXYS4IO_0_S00_AXI_BASEADDR
#define N4IO_HIGHADDR		    XPAR_NEXYS4IO_0_S00_AXI_HIGHADDR

// Definitions for PMOD Encoder Peripheral
#define PMODENC_DEVICE_ID       XPAR_PMODENC_0_DEVICE_ID
#define PMODENC_BASEADDR        XPAR_PMODENC_0_AXI_GPIO_BASEADDR
#define PMODENC_HIGHADDR        XPAR_PMODENC_0_AXI_GPIO_HIGHADDR

// Definition for PWM Detect Address
//#define PWM_DETECT_ADDR         XPAR_AXI_GPIO_1_BASEADDR

// Definition for peripheral PMODHB3
#define PMODHB3_DEVICE_ID		XPAR_PMODHB3IP_0_DEVICE_ID
#define PMODHB3_BASEADDR		XPAR_PMODHB3IP_0_S00_AXI_BASEADDR
#define PMODHB3_HIGHADDR		XPAR_PMODHB3IP_0_S00_AXI_HIGHADDR

// PWM Tachometer Address
#define PWM_TACHO_ADDR			XPAR_PWM_TACHO_0_S00_AXI_BASEADDR

// Fixed Interval timer - 100 MHz input clock, 40KHz output clock
// FIT_COUNT_1MSEC = FIT_CLOCK_FREQ_HZ * .001
#define FIT_IN_CLOCK_FREQ_HZ	CPU_CLOCK_FREQ_HZ
#define FIT_CLOCK_FREQ_HZ		40000
#define FIT_COUNT				(FIT_IN_CLOCK_FREQ_HZ / FIT_CLOCK_FREQ_HZ)
#define FIT_COUNT_1MSEC			40

// Application Specific 
#define NBTNS   5
#define SPEEDUP1   1
#define SPEEDUP5  5
#define SPEEDUP10((x),(pos)) ({1 & ((x)  >> (pos))})   
#define SET_MODE    0
#define RUN_MODE    1
#define CRASH_MODE  2

// Peripheral Instances
extern PmodENC pmodENC_inst;
extern XIntc   IntCtlrInst;             // Interrupt Controller instance
extern XTmrCtr  AXITimerInst;         // Timer Instance
extern XWdtTb   WDTTimerInst;           // Watchdog timer instance
extern static XUartLite uart;       // UARTlite instance

extern volatile uint8_t kpid[3] = {0};
extern volatile uint16_t stptRPM;
// Force Crash flag
extern volatile uint8_t force_crash;

/**************Funtion Prototypes*****************/
extern void do_init(void);      // Initialize system
extern int AXI_Timer_initialize(void);
extern void WDT_Handler(void);

#endif