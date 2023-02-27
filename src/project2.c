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
volatile bool newbtnsSw = false; // true if the FIT handler updated global buttons and switch values
volatile uint16_t sw = 0;	// switches - set in the FIT handler
volatile uint8_t btns = 0;	// buttons - set in the FIT handler
uint8_t pwm_en = TRUE;		// PWM Enable
uint32_t pwm_ctrl_reg_en = 0x00;	// control reg

uint16_t speed = 0;
volatile uint8_t rpm = 0;
uint16_t switch_val = 0, last_switch_val = 0;
int mode = SET_MODE;
uint8_t sel_k_params = 0;
uint8_t k_param_change = 1;
uint16_t set_pt_mod = 1;
int16_t stptRPM_temp = 0;
int16_t temp;

/***********Main Program***********/
int main()
{
    xil_printf("ECE 544 Nexys4IO Project-2 Application\r\n");
    xil_printf("By Omkar Jadhav, Supreet Gulavani\r\n");

	init_platform();
	uint32_t sts = do_init();
	if (XST_SUCCESS != sts){
		xil_printf("FATAL(main): System initialization failed\r\n");
		return 1;
	}

	microblaze_disable_interrupts();

	// main loop
	microblaze_enable_interrupts();
    while(1){
        if(newbtnsSw){
            
            newbtnsSw = false;
        }
    }

    // say goodbye and exit - should never reach here
	microblaze_disable_interrupts();
    cleanup_platform();
    return 0;
}

void input_task(uint8_t btnState)
{
    if(NX4IO_isPressed(BTNC)){
        switch_val = NX4IO_getSwitches();
        //PMODENC544_clearRotaryCount();
        mode++;
        if(mode > 2)
            mode = SET_MODE;
    }

}

void set_task()
{
    uint8_t btn = sel_k_params;

    switch_val = NX4IO_getSwitches();
    if(switch_val != last_switch_val){
        switch((switch_val & 0x0060) >> 5){
            case 0:
                k_param_change = 1;
                break;
            case 1:
                k_param_change = 5;
                break;
            case 2:
            case 3:
                k_param_change = 10;
                break;
        }
        switch((switch_val & 0x0018) >> 3){
            case 0:
                set_pt_mod = 1;
                break;
            case 1:
                set_pt_mod = 5;
                break; 
            case 2:
            case 3:
                set_pt_mod = 10;
            
            break;
        }
        last_switch_val = switch_val;
    }
    
    // asign temp value
    stptRPM_temp = stptRPM;
    // enc_state = 

    // get encoder rotation
    stptRPM_temp += PMODENC544_getRotaryCount() * set_pt_mod;

    // Limit RPM
    if(stptRPM_temp > 4000)
        stptRPM_temp = 4000;
    else if(stptRPM_temp < 0)
        stptRPM_temp = 0;
    
    // check difference and update value
    if (stptRPM_temp != stptRPM){
        stptRPM =  stptRPM_temp;
    }

    // Kx paramaters modifications 
    if (NX4IO_isPressed(BTNR))
        btn++;
    if (btn > 2)
        btn = 0;

    if(btn != sel_k_params)
        sel_k_params = btn;

    if(NX4IO_isPressed(BTNU)){
        temp = kpid[sel_k_params] + k_param_change;
        if(temp > 255)
            kpid[sel_k_params] = 255;
        else    
            kpid[sel_k_params] = temp;
    }
    else if(NX4IO_isPressed(BTND)){
        temp = kpid[sel_k_params] - k_param_change;
        if(temp < 0)
            kpid[sel_k_params] = 0;
        else    
            kpid[sel_k_params] = temp;
    }
    if(NX4IO_isPressed(BTNL)){
        XUartLite_Send(&uart, (u8*)"TEST Pass\r\n", 14 * sizeof(u8*));\
        // send setpt rpm, curr rpm, error rpm, kp, ki, kd
    }
}


// Dynamically change Kx Parameters
void run_task()
{
    switch_val = NX4IO_getSwitches();
    if(switch_val != last_switch_val){
        if(switch_val & 0x0001)
            kpid[0] = 0;
        if((switch_val & 0x0002) >> 1) 
            kpid[1] = 0;
        if((switch_val & 0x0004) >> 2)
            kpid[2] = 0;
        last_switch_val = switch_val;
    }

}

void crash_task()
{
    if(PMODENC544_isBtnPressed())
        force_crash = 1;        // flag 
    else  
        force_crash = 0;
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
