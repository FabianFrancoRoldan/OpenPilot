/**
 ******************************************************************************
 * @addtogroup ESC esc
 * @brief The main ESC code
 *
 * @file       esc.c
 * @author     The OpenPilot Team, http://www.openpilot.org Copyright (C) 2010.
 * @brief      INSGPS Test Program
 * @see        The GNU Public License (GPL) Version 3
 *
 *****************************************************************************/
/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

/* OpenPilot Includes */
#include "pios.h"
#include "esc.h"

#include "fifo_buffer.h"
#include <pios_stm32.h>

#define CURRENT_LIMIT 4600

#define DOWNSAMPLING 1

//TODO: Check the ADC buffer pointer and make sure it isn't dropping swaps
//TODO: Check the time commutation is being scheduled, make sure it's the future
//TODO: Slave two timers together so in phase
//TODO: Ideally lock ADC and delay timers together to both
//TODO: Look into using TIM1
//TODO: Reenable watchdog and replace all PIOS_DELAY_WaitmS with something safe
//know the exact time of each sample and the PWM phase

//TODO: Measure battery voltage and normalize the feedforward model to be DC / Voltage

#define BACKBUFFER_ADC
//#define BACKBUFFER_ZCD
//#define BACKBUFFER_DIFF

/* Prototype of PIOS_Board_Init() function */
extern void PIOS_Board_Init(void);

#define LED_GO  PIOS_LED_HEARTBEAT
#define LED_ERR PIOS_LED_ALARM

int16_t zero_current = 0;

const uint8_t dT = 1e6 / PIOS_ADC_RATE; // 6 uS per sample at 160k
float rate = 0;

static void test_esc();
static void panic(int diagnostic_code);
uint16_t pwm_duration ;
uint32_t counter = 0;

#define NUM_SETTLING_TIMES 20
uint32_t timer;
uint16_t timer_lower;
uint32_t step_period = 0x0080000;
uint32_t last_step = 0;
int16_t low_voltages[3];
int32_t avg_low_voltage;
struct esc_fsm_data * esc_data = 0;

/**
 * @brief ESC Main function
 */

uint32_t offs = 0;

// Major global variables
struct esc_control esc_control;
extern EscSettingsData config;

int main()
{
	esc_data = 0;
	PIOS_Board_Init();

	PIOS_ADC_Config(1);
	
	if (esc_settings_load(&config) != 0)
		esc_settings_defaults(&config);
	PIOS_ESC_SetPwmRate(config.PwmFreq);

	// TODO: Move this into an esc_control section
	esc_control.control_method = ESC_CONTROL_PWM;
	esc_control.serial_input = -1;
	esc_control.pwm_input = -1;
	esc_control.serial_logging_enabled = false;
	esc_control.save_requested = false;
	esc_control.backbuffer_logging_status = false;

	ADC_InitTypeDef ADC_InitStructure;
	ADC_StructInit(&ADC_InitStructure);
	ADC_InitStructure.ADC_Mode = ADC_Mode_RegSimult;
	ADC_InitStructure.ADC_ScanConvMode = ENABLE;
	ADC_InitStructure.ADC_ContinuousConvMode = DISABLE;
	ADC_InitStructure.ADC_ExternalTrigConv = ADC_ExternalTrigConv_T3_TRGO;
	ADC_InitStructure.ADC_DataAlign = ADC_DataAlign_Right;
	ADC_InitStructure.ADC_NbrOfChannel = ((PIOS_ADC_NUM_CHANNELS + 1) >> 1);
	ADC_Init(ADC1, &ADC_InitStructure);
	ADC_Init(ADC2, &ADC_InitStructure);
	ADC_ExternalTrigConvCmd(ADC1, ENABLE);
	ADC_ExternalTrigConvCmd(ADC2, ENABLE);

	// TODO: Move this into a PIOS_DELAY functi

	// TODO: Move this into a PIOS_DELAY function
	TIM_OCInitTypeDef tim_oc_init = {
		.TIM_OCMode = TIM_OCMode_PWM1,
		.TIM_OutputState = TIM_OutputState_Enable,
		.TIM_OutputNState = TIM_OutputNState_Disable,
		.TIM_Pulse = 0,
		.TIM_OCPolarity = TIM_OCPolarity_High,
		.TIM_OCNPolarity = TIM_OCPolarity_High,
		.TIM_OCIdleState = TIM_OCIdleState_Reset,
		.TIM_OCNIdleState = TIM_OCNIdleState_Reset,
	};
	TIM_OC1Init(TIM4, &tim_oc_init);
	TIM_ITConfig(TIM4, TIM_IT_CC1, ENABLE);  // Enabled by FSM

	NVIC_InitTypeDef NVIC_InitStructure;
	NVIC_InitStructure.NVIC_IRQChannel = TIM4_IRQn;
	NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = PIOS_IRQ_PRIO_HIGH;
	NVIC_InitStructure.NVIC_IRQChannelSubPriority = 0;
	NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
	NVIC_Init(&NVIC_InitStructure);

	PIOS_LED_On(LED_GO);
	PIOS_LED_Off(LED_ERR);

	PIOS_ESC_Off();
	PIOS_ESC_SetDirection(config.Direction == ESCSETTINGS_DIRECTION_FORWARD ? ESC_FORWARD : ESC_BACKWARD);

	esc_serial_init();
	
	test_esc();
	
	// Blink LED briefly once passed	
	PIOS_LED_Off(0);
	PIOS_LED_Off(1);
	PIOS_DELAY_WaitmS(250);
	PIOS_LED_On(0);
	PIOS_LED_On(1);
	PIOS_DELAY_WaitmS(500);
	PIOS_LED_Off(0);
	PIOS_LED_Off(1);
	PIOS_DELAY_WaitmS(250);
	
	esc_data = esc_fsm_init();
	esc_data->speed_setpoint = -1;

	PIOS_ADC_StartDma();
	
	counter = 0;
	uint32_t timeval = PIOS_DELAY_GetRaw();
	uint32_t ms_count = 0;
	while(1) {
		counter++;
		
		if(PIOS_DELAY_DiffuS(timeval) > 1000) {
			ms_count++;
			timeval = PIOS_DELAY_GetRaw();
			// Flash LED every 1024 ms
			if((ms_count & 0x000007ff) == 0x400) {
				PIOS_LED_Toggle(0);
			}

			if (esc_control.serial_logging_enabled) {
				uint16_t send_buffer[8] = {0xff00, (ms_count & 0x0000ffff), (ms_count & 0xffff0000) >> 16, esc_data->current_speed, esc_data->speed_setpoint, esc_data->duty_cycle};
				PIOS_COM_SendBufferNonBlocking(PIOS_COM_DEBUG, (uint8_t *) send_buffer, sizeof(send_buffer));
			}
		}
		

		esc_process_static_fsm_rxn();

		// Serial interface: Process any incoming characters, and then process
		// any ongoing messages
		uint8_t c;
		if(PIOS_COM_ReceiveBuffer(PIOS_COM_DEBUG, &c, 1, 0) == 1)
			esc_serial_parse(c);
		esc_serial_process();
		
		if(esc_control.save_requested && esc_data->state == ESC_STATE_IDLE) {
			esc_control.save_requested = false;
			esc_settings_save(&config);
			// TODO: Send serial ack if succeeded or failed
		}
		
		if (esc_control.control_method == ESC_CONTROL_SERIAL) {
			esc_data->speed_setpoint = esc_control.serial_input;
		}
	}
	return 0;
}

/* INS functions */
void panic(int diagnostic_code)
{
	// Turn off error LED.
	PIOS_LED_Off(LED_ERR);
	while(1) {
		for(int i=0; i<diagnostic_code; i++)
		{
			PIOS_LED_On(LED_ERR);
			PIOS_LED_On(PIOS_LED_HEARTBEAT);
			for(int i = 0 ; i < 250; i++) {
				PIOS_DELAY_WaitmS(1); //Count 1ms intervals in order to allow for possibility of watchdog
			}

			PIOS_LED_Off(LED_ERR);
			PIOS_LED_Off(PIOS_LED_HEARTBEAT);
			for(int i = 0 ; i < 250; i++) {
				PIOS_DELAY_WaitmS(1); //Count 1ms intervals in order to allow for possibility of watchdog
			}

		}
		PIOS_DELAY_WaitmS(1000);
	}
}

//TODO: Abstract out constants.  Need to know battery voltage too
//TODO: Other things to test for 
//      - impedance from motor(?)
//      - difference between high voltages
int32_t voltages[6][3];
void test_esc() {


	PIOS_ESC_Off();
	for(int i = 0; i < 150; i++) {
		PIOS_DELAY_WaitmS(1);
	}

	zero_current = PIOS_ADC_PinGet(0);

	PIOS_ESC_Arm();

	PIOS_ESC_TestGate(ESC_A_LOW);
	PIOS_DELAY_WaituS(250);
	low_voltages[0] = PIOS_ADC_PinGet(1);
	PIOS_ESC_TestGate(ESC_B_LOW);
	PIOS_DELAY_WaituS(250);
	low_voltages[1] = PIOS_ADC_PinGet(2);
	PIOS_ESC_TestGate(ESC_C_LOW);
	PIOS_DELAY_WaituS(250);
	low_voltages[2] = PIOS_ADC_PinGet(3);
	avg_low_voltage = low_voltages[0] + low_voltages[1] + low_voltages[2];

	PIOS_ESC_SetDutyCycle(PIOS_ESC_MAX_DUTYCYCLE / 2);
	PIOS_ESC_TestGate(ESC_A_LOW);
	PIOS_DELAY_WaituS(250);
	PIOS_ESC_SetDutyCycle(PIOS_ESC_MAX_DUTYCYCLE);
	PIOS_DELAY_WaituS(3000);
	voltages[1][0] = PIOS_ADC_PinGet(1);
	voltages[1][1] = PIOS_ADC_PinGet(2);
	voltages[1][2] = PIOS_ADC_PinGet(3);

	PIOS_ESC_SetDutyCycle(PIOS_ESC_MAX_DUTYCYCLE / 2);
	PIOS_ESC_TestGate(ESC_A_HIGH);
	PIOS_DELAY_WaituS(250);
	PIOS_ESC_SetDutyCycle(PIOS_ESC_MAX_DUTYCYCLE);
	PIOS_DELAY_WaituS(3000);
	voltages[0][0] = PIOS_ADC_PinGet(1);
	voltages[0][1] = PIOS_ADC_PinGet(2);
	voltages[0][2] = PIOS_ADC_PinGet(3);

	PIOS_ESC_SetDutyCycle(PIOS_ESC_MAX_DUTYCYCLE / 2);
	PIOS_ESC_TestGate(ESC_B_LOW);
	PIOS_DELAY_WaituS(250);
	PIOS_ESC_SetDutyCycle(PIOS_ESC_MAX_DUTYCYCLE);
	PIOS_DELAY_WaituS(3000);
	voltages[3][0] = PIOS_ADC_PinGet(1);
	voltages[3][1] = PIOS_ADC_PinGet(2);
	voltages[3][2] = PIOS_ADC_PinGet(3);

	PIOS_ESC_SetDutyCycle(PIOS_ESC_MAX_DUTYCYCLE / 2);
	PIOS_ESC_TestGate(ESC_B_HIGH);
	PIOS_DELAY_WaituS(250);
	PIOS_ESC_SetDutyCycle(PIOS_ESC_MAX_DUTYCYCLE);
	PIOS_DELAY_WaituS(3000);
	voltages[2][0] = PIOS_ADC_PinGet(1);
	voltages[2][1] = PIOS_ADC_PinGet(2);
	voltages[2][2] = PIOS_ADC_PinGet(3);

	PIOS_ESC_SetDutyCycle(PIOS_ESC_MAX_DUTYCYCLE / 2);
	PIOS_ESC_TestGate(ESC_C_LOW);
	PIOS_DELAY_WaituS(250);
	PIOS_ESC_SetDutyCycle(PIOS_ESC_MAX_DUTYCYCLE);
	PIOS_DELAY_WaituS(3000);
	voltages[5][0] = PIOS_ADC_PinGet(1);
	voltages[5][1] = PIOS_ADC_PinGet(2);
	voltages[5][2] = PIOS_ADC_PinGet(3);

	PIOS_ESC_SetDutyCycle(PIOS_ESC_MAX_DUTYCYCLE / 2);
	PIOS_ESC_TestGate(ESC_C_HIGH);
	PIOS_DELAY_WaituS(250);
	PIOS_ESC_SetDutyCycle(PIOS_ESC_MAX_DUTYCYCLE);
	PIOS_DELAY_WaituS(3000);
	voltages[4][0] = PIOS_ADC_PinGet(1);
	voltages[4][1] = PIOS_ADC_PinGet(2);
	voltages[4][2] = PIOS_ADC_PinGet(3);

	// If the particular phase isn't moving fet is dead
	if(voltages[0][0] < 1000) {
		PIOS_ESC_SetDutyCycle(PIOS_ESC_MAX_DUTYCYCLE / 10);
		PIOS_ESC_TestGate(ESC_A_HIGH);
		panic(1);
	}
	if(voltages[1][0] > 30) {
		PIOS_ESC_SetDutyCycle(PIOS_ESC_MAX_DUTYCYCLE / 10);
		PIOS_ESC_TestGate(ESC_A_LOW);
		panic(2);		
	}
	if(voltages[2][1] < 1000) {
		PIOS_ESC_SetDutyCycle(PIOS_ESC_MAX_DUTYCYCLE / 10);
		PIOS_ESC_TestGate(ESC_B_HIGH);
		panic(3);
	}
	if(voltages[3][1] > 30){
		PIOS_ESC_SetDutyCycle(PIOS_ESC_MAX_DUTYCYCLE / 10);
		PIOS_ESC_TestGate(ESC_B_LOW);
		panic(4);
	}	
	if(voltages[4][2] < 1000) {
		PIOS_ESC_SetDutyCycle(PIOS_ESC_MAX_DUTYCYCLE / 10);
		PIOS_ESC_TestGate(ESC_C_HIGH);
		panic(5);
	}
	if(voltages[5][2] > 30){
		PIOS_ESC_SetDutyCycle(PIOS_ESC_MAX_DUTYCYCLE / 10);
		PIOS_ESC_TestGate(ESC_C_LOW);
		panic(6);
	}
	// TODO: If other channels don't follow then motor lead bad
	
	PIOS_ESC_Off();

}

uint32_t bad_inputs;
void PIOS_TIM_4_irq_override();
extern void PIOS_DELAY_timeout();
void TIM4_IRQHandler(void) __attribute__ ((alias ("PIOS_TIM_4_irq_handler")));
static bool rising = false;

static uint16_t rise_value;
static uint16_t fall_value;
static uint16_t capture_value;

static void PIOS_TIM_4_irq_handler (void)
{
	static uint32_t last_input_update;
	
	if(TIM_GetITStatus(TIM4,TIM_IT_CC1)) {
		PIOS_DELAY_timeout();
		TIM_ClearITPendingBit(TIM4,TIM_IT_CC1);
	}
	
	if (TIM_GetITStatus(TIM4, TIM_IT_CC3)) {
		
		TIM_ClearITPendingBit(TIM4,TIM_IT_CC3);
		
		TIM_ICInitTypeDef TIM_ICInitStructure = {
			.TIM_ICPolarity = TIM_ICPolarity_Rising,
			.TIM_ICSelection = TIM_ICSelection_DirectTI,
			.TIM_ICPrescaler = TIM_ICPSC_DIV1,
			.TIM_ICFilter = 0x0,
		};
		
		if(rising) {
			rising = false;
			rise_value = TIM_GetCapture3(TIM4);
			
			/* Switch polarity of input capture */
			TIM_ICInitStructure.TIM_ICPolarity = TIM_ICPolarity_Falling;
			TIM_ICInitStructure.TIM_Channel = TIM_Channel_3;
			TIM_ICInit(TIM4, &TIM_ICInitStructure);
		} else {
			rising = true;
			fall_value = TIM_GetCapture3(TIM4);

			/* Switch polarity of input capture */
			TIM_ICInitStructure.TIM_ICPolarity = TIM_ICPolarity_Rising;
			TIM_ICInitStructure.TIM_Channel = TIM_Channel_3;
			TIM_ICInit(TIM4, &TIM_ICInitStructure);
			
			if (fall_value > rise_value) {
				capture_value = fall_value - rise_value;
			} else {
				capture_value = TIM4->ARR + fall_value - rise_value;
			}
			
		}

		// Not initialized yet
		if (esc_data == NULL)
			return;

		// Dont process crazy values
		if(capture_value > config.PwmMin * 0.5 && capture_value < config.PwmMax * 1.1) {
			last_input_update = PIOS_DELAY_GetRaw();
			
			// Limit input range
			if(capture_value < config.PwmMin)
				capture_value = 0;
			else if (capture_value > config.PwmMax)
				capture_value = config.PwmMax;
			
			esc_control.pwm_input = capture_value;
			if(esc_control.control_method == ESC_CONTROL_PWM) {
				if (config.Mode == ESCSETTINGS_MODE_CLOSED) {
					uint32_t scaled_capture = config.RpmMin + (capture_value - config.PwmMin) * (config.RpmMax - config.RpmMin) / (config.PwmMax - config.PwmMin);
					esc_data->speed_setpoint = (capture_value < config.PwmMin) ? 0 : scaled_capture;
					esc_data->duty_cycle_setpoint = 0;
				} else if (config.Mode == ESCSETTINGS_MODE_OPEN){
					uint32_t scaled_capture = (capture_value - config.PwmMin) * PIOS_ESC_MAX_DUTYCYCLE / (config.PwmMax - config.PwmMin);
					esc_data->duty_cycle_setpoint = (capture_value < config.PwmMin) ? 0 : scaled_capture;
					esc_data->speed_setpoint = 0;
				} else {
					esc_data->duty_cycle_setpoint = 0;
					esc_data->speed_setpoint = 0;

				}
			}
		}
	} 
	
	if (TIM_GetITStatus(TIM4, TIM_IT_Update)) {
		if (PIOS_DELAY_DiffuS(last_input_update) > 100000) {
			esc_control.pwm_input = -1;
			if(esc_control.control_method == ESC_CONTROL_PWM) {
				esc_data->speed_setpoint = -1;
			}
		}
		TIM_ClearITPendingBit(TIM4,TIM_IT_Update);
	}
}

/*
 Notes:
 1. For start up, definitely want to use complimentary PWM to ground the lower side, making zero crossing truly "zero"
 2. May want to use the "middle" sensor to actually pull it up, so that zero is above zero (in ADC range).  Should still
    see BEMF at -0.7 (capped by transistor range) relative to that point (divided down by whatever)
 3. Possibly use an inadequate voltage divider plus use the TVS cap to keep the part of the signal near zero clean
 */


