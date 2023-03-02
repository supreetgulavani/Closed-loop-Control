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

   #include "platform.h"
   #include "system.h"
   #include "xparameters.h"
   #include "xstatus.h"
   #include "PmodENC544.h"
   #include "nexys4io.h"


   /********** Global Variables **********/

   u16 kpid[3] = {0};
   u8 pwm = 0;
   uint16_t switch_val = 0, last_switch_val = 0;
   u8 mode = SET_MODE;
   u8 sel_k_params = 0;
   u8 k_param_change = 1;
   u16 set_pt_mod = 1;
   volatile u16 stptRPM = 0;
   volatile u16 stptRPM_temp = 0;
   bool newbtnsSw = true;


   //Buttons and Switches Temp Variables
   volatile u8 btn_temp;			// previous value of button register
   volatile u16 sw_temp;				// previous value of the switch register
   volatile u8 encBtn_temp;			// previous value of the encoder button
   volatile u8 encSW_temp;				// previous value of the encoder switch

   //Buttons and Switches Variables
   volatile u8 btn;			// previous value of button register
   volatile u16 sw;				// previous value of the switch register
   volatile u8 encBtn;			// previous value of the encoder button
   volatile u8 encSW;				// previous value of the encoder switch

   void input_task();
   void update_btnsw_val();
   void set_task();
   void run_task();
   void crash_task();
   void mode_task(void);



   /***********Main Program***********/
   int main()
   {
       xil_printf("ECE 544 Nexys4IO Project-2 Application\r\n");
       xil_printf("By Omkar Jadhav, Supreet Gulavani\r\n");

   	init_platform();
   	uint32_t sts = do_init();

   	if (XST_SUCCESS != sts) {
   		xil_printf("FATAL(main): System initialization failed\r\n");
   		return 1;
   	}

   	//microblaze_disable_interrupts();
   	xil_printf("Here\r\n");
   	// main loop
   	//microblaze_enable_interrupts();
       while (1) {
           if (newbtnsSw)
           {
        	   input_task();
        	   update_btnsw_val();
        	   mode_task();

               //newbtnsSw = false;
           }

          sleep(1);
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

		xil_printf("///////////Input Task///////////\n\r");

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
		sw_temp =  0x57;//NX4IO_getSwitches();
		btn_temp = 0x19;//NX4IO_getBtns ();
		u32 encBtnSW_temp = 0x0; //PMODENC544_getBtnSwReg();
		encBtn_temp = encBtnSW_temp & 0x01;
		encSW_temp = (encBtnSW_temp >>1) & 0x01;

		if (sw != sw_temp || btn != btn_temp) {
			newbtnsSw = true;
		}

		xil_printf("Switches: %d  PushButton:%d Enc_btn: %d Enc_SW:%d \r\n", sw_temp, btn_temp, encBtn_temp, encSW_temp);
   }

   void update_btnsw_val()
   {
       btn		=  btn_temp;
       sw		=  sw_temp;
       encBtn 	=  encBtn_temp;
       encSW 	=  encSW_temp;

       if(GET_BIT(btn,4)){
           mode++;
           if(mode > 1)
               mode = SET_MODE;
       }

       if (GET_BIT(encSW,0)) {
    	   mode = CRASH_MODE;
       }

       (mode == SET_MODE) ? xil_printf("Mode == SET MODE\n\r") :
    		   (mode == RUN_MODE) ? xil_printf("Mode: RUN MODE\n\r") : xil_printf("Mode: CRASH MODE \n\r");

   }

   void set_task()
   {
	   uint8_t param_temp = sel_k_params;

	   xil_printf("//////////////SET TASK////////////////\n\r");

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

	   xil_printf("Btn R: %d Btn U: %d Btn D: %d Btn L:%d\n\r", GET_BIT(btn,3), GET_BIT(btn,3), GET_BIT(btn,2), GET_BIT(btn,1));
       // Kx paramaters modifications
       if (GET_BIT(btn,0)) {
           param_temp++;
       }

       if (param_temp > 2)
           param_temp = 0;

       if(param_temp != sel_k_params)
           sel_k_params = param_temp;


       if (GET_BIT(btn,3)) {
    	   u16 temp;
           temp = kpid[sel_k_params] + k_param_change;
           if(temp > 255)
               kpid[sel_k_params] = 255;
           else
               kpid[sel_k_params] = temp;
       }
       else if (GET_BIT(btn,2)) {
    	   int16_t temp;

           temp = kpid[sel_k_params] - k_param_change;

           if (temp < 0)
               kpid[sel_k_params] = 0;
           else
               kpid[sel_k_params] = temp;
       }

       if (GET_BIT(btn,1)) {
           //XUartLite_Send(&uart, (u8*)"TEST Pass\r\n", 14 * sizeof(u8*));\
           // send setpt rpm, curr rpm, error rpm, kp, ki, kd
       }

       xil_printf("k_param_change: %d, set_pt_mod: %d, sel_k_params: %d, kpid[sel_k_params]: %d \n\r", k_param_change, set_pt_mod, sel_k_params,
    		   	   	   kpid[sel_k_params]);
   }


   // Dynamically change Kx Parameters
   void run_task()
   {
	   xil_printf("///////////////RUN TASK////////////////\n\r");

	   // get encoder rotation
	   //stptRPM_temp += PMODENC544_getRotaryCount() * set_pt_mod - stptRPM;
	   //stptRPM_temp += ((1 + stptRPM_temp/set_pt_mod) * set_pt_mod) - stptRPM;


       if (stptRPM_temp > 5000)
           stptRPM_temp = 5000;
       else if (stptRPM_temp < 0)
           stptRPM_temp = 0;

	   if (GET_BIT(encBtn,0)) {
    	  xil_printf("Setting rpm to 0  and pwm to 0\n\r");
		   //rpm = 0;

       }

       // check difference and update value
       if (stptRPM_temp != stptRPM) {
           stptRPM =  stptRPM_temp;
       }

       //pid(GET_BIT(sw,2), GET_BIT(sw, 1), GET_BIT(sw, 0));

       stptRPM = stptRPM_temp;

       xil_printf("Setpoint RPM: %d\n\r", stptRPM);

   }

   void crash_task()
   {
	   xil_printf("///////////CRASH MODE/////////////\n\r");
	   /*
   	// make this external
       if(GET_BIT((encSW,0)))
          // force_crash = 1;        // flag
      // else
           //force_crash = 0;
	    */
   }

   void mode_task(void)
   {
       switch(mode){
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