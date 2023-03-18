	/****************************************************************************************
	*   @file project2.c
	*
	*   @author Supreet Gulavani (sg7@pdx.edu)
	*   @copyright Supreet Gulavani, 2023
	*
	*   Modified from the original file by Prof. Roy Kravitz at Portland State University.
	*
	*
	*******************************************************************************************/

	/***************************** Header Files ***********************************/
	#include <stdio.h>
	#include <stdint.h>
	#include <stdlib.h>
	#include <math.h>
	#include "platform.h"
	#include "system.h"
	#include "xparameters.h"
	#include "xstatus.h"
	#include "PmodENC544.h"
	#include "nexys4io.h"
	#include "xuartlite.h"
	#include "xwdttb.h"


	/********** Global Variables **********/

	u16 kpid[3] 				 = {0};
	u8 pwm 					 = 0;
	u16 switch_val 			 = 0;
	u16 last_switch_val 		 = 0;
	u8 mode 					 = SET_MODE;
	u8 sel_k_params 			 = 0;
	u8 k_param_change 		 = 1;
	u16 set_pt_mod 			 = 1;
	volatile u16 stptRPM 	 = 0;
	volatile u16 stptRPM_temp = 0;
	bool newbtnsSw 			 = true;

	XIntc 			INTC_Inst;		// Interrupt Controller instance

	// Buttons and Switches Temp Variables
	volatile u8 btn_temp;				// new value of button register
	volatile u16 sw_temp;				// new value of the switch register
	volatile u8 encBtn_temp;				// new value of the encoder button
	volatile u8 encSW_temp;				// new value of the encoder switch

	// Buttons and Switches Variables
	volatile u8 btn;					// updated value of button register
	volatile u16 sw;					// updated value of the switch register
	volatile u8 encBtn;				// updated value of the encoder button
	volatile u8 encSW;				// updated value of the encoder switch

	u8 direction;
	u8 copyData;
	int32_t integralVal;
	int32_t error;
	u16 rpm_new;
	volatile int16_t rotaryCount;
	XUartLite uart;
	XWdtTb WDT_Inst;
	u8 rpm = 1;

	void input_task();
	void update_btnsw_val();
	void set_task();
	void run_task();
	void crash_task();
	void mode_task(void);
	void pid(u8 kp_Sel, u8 ki_Sel, u8 kd_Sel, u8 pwmVal);


	/***********Main Program***********/
	int main()
	{
	   xil_printf("ECE 544 Nexys4IO Project-2 Application\r\n");
	   xil_printf("By Omkar Jadhav, Supreet Gulavani\r\n");

	   init_platform();
	   u32 sts = do_init();

		if (XST_SUCCESS != sts) {
			xil_printf("FATAL(main): System initialization failed\r\n");

			return 1;
		}

		microblaze_disable_interrupts();

		// main loop
		while (1)
		{
		   input_task();

		   if (XWdtTb_IsWdtExpired(&WDT_Inst))
		   {
			   if (encSW_temp == 0)
			   {
					XWdtTb_RestartWdt(&WDT_Inst);
			   }
		   }

		   if (newbtnsSw)
		   {
			   update_btnsw_val();
			   newbtnsSw = false;
		   }

		   mode_task();

		   if (mode != RUN_MODE)
			   usleep(500000);
		}

	   // say goodbye and exit - should never reach here
	   microblaze_disable_interrupts();
	   cleanup_platform();

	   return 0;
	}

	void input_task()
	{
		static bool isInitialized = false;	// true if the function has run at least once
		static bool dpOn;					// true if decimal point 0 is on

		// initialize the static variables the first time the function is called
		if (!isInitialized) {
			btn = 0x8F;	// invert btns
			sw = 0xFFFF;	// invert switches to get everything started
			dpOn = true;
			isInitialized = true;
			newbtnsSw = false;
		}

		// return if main() hasn't handled the last button and switch changes
		if (newbtnsSw) {
			return;
		}

		// toggle DP0 to indicate that FIT handler is being called
		// this happens every time the FIT handler is called
		dpOn = (dpOn) ? false: true;
		NX4IO_SSEG_setDecPt (SSEGLO, DIGIT0, dpOn);

		// get the value of the switches.  Will be written to the LEDs in main()
		sw_temp =  NX4IO_getSwitches();
		btn_temp = NX4IO_getBtns ();

		// Get the values of the encoder button and switches
		u32 encBtnSW_temp = PMODENC544_getBtnSwReg();
		encBtn_temp = encBtnSW_temp & 0x01;
		encSW_temp = (encBtnSW_temp >>1) & 0x01;

		// check if there is a change in the values of all the pushbuttons and switches
		if (sw != sw_temp || btn != btn_temp || encBtn_temp!= encBtn || encSW != encSW_temp) {
			newbtnsSw = true;
		}

		// Check is the rpm is needed to be sent over uart
		// Check if button L is set to 1 or not
		if (GET_BIT(btn,1))
			   copyData = 1;
		else
			 copyData = 0;

	}

	/**
	 * update_btnsw_val() - Switch between modes
	 *
	 * @brief checks the values of the push buttons and changes mode accordingly
	 */
	void update_btnsw_val()
	{

		btn		=  btn_temp;
		sw		=  sw_temp;
		encBtn 	=  encBtn_temp;
		encSW 	=  encSW_temp;

		/* Check if the center button was pressed.
		 * If yes, change the mode between SET_MODE and RUN_MODE
		 */
		if(GET_BIT(btn,4)){
		   mode++;
		   if(mode > 1)
			   mode = SET_MODE;
		}

		/* Check if the encoder switch is pushed to ON
		 * If yes, change the mode to CRASH_MODE
		 */
		if (GET_BIT(encSW,0))
		   mode = CRASH_MODE;
	}

	/**
	 * set_task() - handles all the SET mode configurations
	 *
	 * @brief This function handles all the SET mode configurations such as checking the switches to set the correct
	 * multiplier for k-parameters and setpoint. Also checks the pushbuttons to fulfill their purpose and displays
	 * on the 7-segment.
	 */
	void set_task()
	{
		uint8_t param_temp = sel_k_params;

		xil_printf("\n\r//////////////SET TASK////////////////\n\r");

		/* check if the switches [6:5] are pressed.
		 * Update the factors of Kp, Ki, Kd accordingly.
		 */
		switch ((sw & 0x0060) >> 5) {
			case 0:
				k_param_change = FACTOR_1;
				break;
			case 1:
				k_param_change = FACTOR_5;
				break;
			case 2:
			case 3:
				k_param_change = FACTOR_10;
				break;
		}

		/* check if the switches [4:3] are pressed.
		 * Update the factor of setpoint from the rotary encoder accordingly.
		 */
		switch ((sw & 0x0018) >> 3) {
		   case 0:
			   set_pt_mod = FACTOR_1;
			   break;
		   case 1:
			   set_pt_mod = FACTOR_5;
			   break;
		   case 2:
		   case 3:
			   set_pt_mod = FACTOR_10;
			   break;
		}

		xil_printf("Btn R: %d Btn U: %d Btn D: %d Btn L:%d\n\r", GET_BIT(btn,0), GET_BIT(btn,3), GET_BIT(btn,2), GET_BIT(btn,1));

		/* Kx parameters modifications
		 * If the button R is pressed, select between Kp, Ki, Kd, to
		 * increment or decrement their values
		 */
		if (GET_BIT(btn,0)) {
		   param_temp++;
		}

		if (param_temp > 2)
		   param_temp = 0;

		if(param_temp != sel_k_params)
		   sel_k_params = param_temp;

		/* Kx parameters modifications
		 * If the button u is pressed, Increment Kp or Ki or Kd values
		 */
		if (GET_BIT(btn,3)) {
		   u16 temp;

		   temp = kpid[sel_k_params] + k_param_change;

		   if(temp > 255)
			   kpid[sel_k_params] = 255;
		   else
			   kpid[sel_k_params] = temp;
		}

		/* Kx parameters modifications
		 * If the button u is pressed, Increment Kp or Ki or Kd values
		 */
		else if (GET_BIT(btn,2)) {
		   int16_t temp;

		   temp = kpid[sel_k_params] - k_param_change;

		   if (temp < 0)
			   kpid[sel_k_params] = 0;
		   else
			   kpid[sel_k_params] = temp;
		}

		/* Display the kp, ki, kd values on to the 7 segment display
		 * Digit[1:0] -> kd
		 * Digit[3:2] -> ki
		 * Digit[5:4] -> kp
		 */
		NX410_SSEG_setAllDigits(SSEGLO, (kpid[1] / 10) % 10, kpid[1] % 10,
										(kpid[2] / 10) % 10, kpid[2] % 10,
										(GET_BIT(sw, 1) << 3 | GET_BIT(sw, 0) << 1));
		usleep(100);

		NX410_SSEG_setAllDigits(SSEGHI, CC_BLANK		   , CC_BLANK,
										(kpid[0] / 10) % 10, kpid[0] % 10,
										GET_BIT(sw,2) << 1);

		xil_printf("k_param_change: %d, set_pt_mod: %d, sel_k_params: %d, kpid[sel_k_params]: %d \n\r", k_param_change, set_pt_mod, sel_k_params,
						   kpid[sel_k_params]);
	}

	/**
	 * run_task() - handles all the RUN mode configurations
	 *
	 * @brief The function gets the rotary count from the encoder, converts to RPM, checks the direction of the motor
	 *  configures the setpoint while displaying out on the 7-segment
	 */
	void run_task()
	{
		u8 pwmVal;

		// Get the rotary count from the encoder
		rotaryCount = PMODENC544_getRotaryCount()  * set_pt_mod;


		xil_printf("rotary_count:%d Setting rpm:%d set_pt_mod: %d\n\r", 1, rotaryCount, set_pt_mod);

		// Convert the rotary count to RPM
		stptRPM_temp = abs(rotaryCount) * 6000/ 255;

		// Determine the direction
		if (rotaryCount > 0) {
		   direction = 1;
		}
		else if (rotaryCount < 0)
		   direction = 0;

		// Restrict the setpoint RPM
		if (stptRPM_temp > 5000) {
		   stptRPM_temp = 5000;
		}

		// Ensure that the RPM is non negative
		if (stptRPM_temp < 0) {
		   stptRPM_temp = 0;
		}

		/* check if a new setpoint is assigned.
		 * Display on the rpm on Digit [3:0]
		 * Map the rotary Count from 0  to 255.
		 * For each setpoint, update the integral value to 0 for the PID controller.
		 */
		if (stptRPM_temp != stptRPM) {
		   NX410_SSEG_setAllDigits(SSEGLO, (stptRPM_temp / 1000) % 10, (stptRPM_temp / 100) % 10, (stptRPM_temp / 10) % 10, stptRPM_temp % 10, DP_NONE);
		   pwmVal = (u8)fmin(abs(rotaryCount), 255);

		   stptRPM =  stptRPM_temp;
		   integralVal = 0;
		}

		// Check if the encoder button is pressed to toggle between motor on and off
		if(GET_BIT(encBtn,0))
		   rpm = !rpm;

		if(!rpm)
		   stptRPM = 0;

		pid(GET_BIT(sw,2), GET_BIT(sw, 1), GET_BIT(sw, 0), pwmVal);

	}

	/**
	 * crash_task() - handles all the CRASH mode configurations
	 *
	 * @brief The function when encoder switch is toggled, sets all the parameters to zero.
	 *
	 */
	void crash_task()
	{
		xil_printf("\n\r///////////CRASH MODE/////////////\n\r");

		// Set all important parameters to zero
		btn 	= 0;
		sw 		= 0;
		encBtn 	= 0;
		encSW 	= 0;

		// Display ECE 540 on the 7 seg display
		NX4IO_SSEG_setSSEG_DATA(SSEGHI, 0x0058E30E);
		NX4IO_SSEG_setSSEG_DATA(SSEGLO, 0x00144116);

		// Infinite loop
		while(1);
	}

	/**
	 * mode_task() - Switches between modes as input is changed
	 *
	 * @brief Gets mode from the user and calls the respective mode tasks
	 *
	 */
	void mode_task(void)
	{
	   // Operate between modes
	   switch(mode) {
		   case SET_MODE:
			   set_task();
			   break;
		   case RUN_MODE:
			   run_task();
			   break;
		   case CRASH_MODE:
			   crash_task();
			   break;
		   default:
			   break;
	   }
	}

	/**
	 * pid() - Drives the motor based on P/I/D controller
	 *
	 * @brief Initializes the pid if uninitialized by setting the motor to a high value.
	 * 		  Captures the rpm of the motor and passes the P/I/D control to the motor.
	 *
	 */
	void pid(u8 kp_Sel, u8 ki_Sel, u8 kd_Sel, u8 pwmVal)
	{
		static bool pid_IsInitialized = false;
		static  float time_step = 0.01f;

		u8 pwm_actual, pwm_new, pwm_target;

		// check if pid is not intialized
		if (!pid_IsInitialized)
		{
		   // Send the first rpm to the motor
		   PMODHB3_SetConfig(XPAR_PMODHB3_IP_0_S00_AXI_BASEADDR, PMODHB3_IP_S00_AXI_SLV_REG1_OFFSET,
				   	   	   	   (1 << 9 | direction << 8 | 0x1f));
		   pid_IsInitialized = true;

		   return;
		}

		int32_t prev_error = error;

		// Delay to let hardware accumulate pulses for calculating rpm
		usleep(200000);

		// Capture the rpm
		u16 rpm_actual = PMODHB3_GetRpm(XPAR_PMODHB3_IP_0_S00_AXI_BASEADDR,
							PMODHB3_IP_S00_AXI_SLV_REG0_OFFSET);

		pwm_actual = (rpm_actual * 255) / 6000;
		pwm_target = (stptRPM * 255) / 6000;

		// display the captured rpm onto the 7 segment display -> Digit[7:4]
		NX410_SSEG_setAllDigits(SSEGHI, (rpm_actual / 1000) % 10, (rpm_actual / 100) % 10,
										(rpm_actual / 10) % 10, rpm_actual % 10, DP_NONE);

		if(rpm_actual < 5000) {

			// Calculate the error between the setpoint and rpm detected from digital encoder
			error = pwm_target - pwm_actual;

			integralVal += error;

			// Calculate the rpm by selecting the type of controller
			pwm_new = kp_Sel * kpid[0] * error + kd_Sel * kpid[1] * ((error - prev_error) / time_step) + ki_Sel * kpid[2] * integralVal * time_step;

			// Map the rpm_new to pwm_new from 0  to 255
			rpm_new = (pwm_new * 6000) / 255;

			// Cap the pwm till 200
			pwm_new = fmin(pwm_new, 200);

			PMODHB3_SetConfig(XPAR_PMODHB3_IP_0_S00_AXI_BASEADDR, PMODHB3_IP_S00_AXI_SLV_REG1_OFFSET,
							   (1 << 9 | !direction << 8 | pwm_new));

			if (copyData == 1) {
				xil_printf("%u,%u,", rpm_actual, stptRPM);
				xil_printf("%u\n\r", rpm_new);
			}
		}
	}

	/**
	* initialize the system
	*
	* This function is executed once at start-up and after resets.  It initializes
	* the peripherals and registers the interrupt handler(s)
	*/
	XStatus do_init(void)
	{
		uint32_t status;				// status from Xilinx Lib calls

		// Configure the uart Module
		uart.IsReady= true;
		uart.RegBaseAddress = XPAR_UARTLITE_1_BASEADDR;
		XUartLite_Config uart_cfg;
		uart_cfg.BaudRate = 115200;
		uart_cfg.DataBits= 8;
		uart_cfg.RegBaseAddr = XPAR_UARTLITE_1_BASEADDR;
		uart_cfg.UseParity = false;
		uart_cfg.ParityOdd = true;

		// Initialize the uart
		XUartLite_CfgInitialize(&uart, &uart_cfg, XPAR_UARTLITE_1_BASEADDR);
		XUartLite_Initialize(&uart, XPAR_UARTLITE_1_DEVICE_ID);

		// Initialize the PMODENC544 Encoder peripheral
		status = PMODENC544_initialize(XPAR_PMODENC544_0_S00_AXI_BASEADDR);
		if (status != XST_SUCCESS)
			return XST_FAILURE;

		// Initialize the PMODHB3 peripheral
		status = PMODHB3_Initialize(XPAR_PMODHB3_IP_0_S00_AXI_BASEADDR);
		if (status != XST_SUCCESS)
			return XST_FAILURE;

		// initialize the Nexys4 driver and (some of)the devices
		status = (uint32_t) NX4IO_initialize(N4IO_BASEADDR);
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

		// Initialize the watchdog timer
		status = XWdtTb_Initialize(&WDT_Inst, XPAR_AXI_TIMEBASE_WDT_0_DEVICE_ID);

		if (status == XST_FAILURE)
		{
			xil_printf("Failed to initialize watchdog timer\r\n");
		}

		// start
		XWdtTb_Start(&WDT_Inst);

		return XST_SUCCESS;
	}

