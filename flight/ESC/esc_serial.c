//
//  esc_serial.c
//  OpenPilotOSX
//
//  Created by James Cotton on 12/1/11.
//  Copyright 2011 OpenPilot. All rights reserved.
//

#include <stdio.h>

#include "pios.h"
#include "esc.h"
#include "esc_fsm.h"
#include "esc_settings.h"
#include "fifo_buffer.h"

#define ESC_SYNC_BYTE 0x85

#define MAX_SERIAL_BUFFER_SIZE 128

static int32_t esc_serial_command_received();

//! The set of commands the ESC understands
enum esc_serial_command {
	ESC_COMMAND_SET_CONFIG = 0,
	ESC_COMMAND_GET_CONFIG = 1,
	ESC_COMMAND_SAVE_CONFIG = 2,
	ESC_COMMAND_ENABLE_SERIAL_LOGGING = 3,
	ESC_COMMAND_DISABLE_SERIAL_LOGGING = 4,
	ESC_COMMAND_REBOOT_BL = 5,
	ESC_COMMAND_ENABLE_SERIAL_CONTROL = 6,
	ESC_COMMAND_DISABLE_SERIAL_CONTROL = 7,
	ESC_COMMAND_SET_SPEED = 8,
	ESC_COMMAND_WHOAMI = 9,
	ESC_COMMAND_ENABLE_ADC_LOGGING = 10,
	ESC_COMMAND_GET_ADC_LOG = 11,
	ESC_COMMAND_SET_PWM_FREQ = 12,
	ESC_COMMAND_GET_STATUS = 13,
	ESC_COMMAND_LAST = 14,
};

//! The size of the data packets
const uint8_t esc_command_data_size[ESC_COMMAND_LAST] = {
	[ESC_COMMAND_SET_CONFIG] = sizeof(EscSettingsData) + 0, // No CRC yet
	[ESC_COMMAND_GET_CONFIG] = 0,
	[ESC_COMMAND_SAVE_CONFIG] = 0,
	[ESC_COMMAND_ENABLE_SERIAL_LOGGING] = 0,
	[ESC_COMMAND_DISABLE_SERIAL_LOGGING] = 0,
	[ESC_COMMAND_REBOOT_BL] = 0,
	[ESC_COMMAND_DISABLE_SERIAL_CONTROL] = 0,
	[ESC_COMMAND_SET_SPEED] = 2,
	[ESC_COMMAND_SET_PWM_FREQ] = 2,
	[ESC_COMMAND_GET_STATUS] = 0,
};

//! States for the ESC parsers
enum esc_serial_parser_state {
	ESC_SERIAL_WAIT_SYNC,
	ESC_SERIAL_WAIT_FOR_COMMAND,
	ESC_SERIAL_WAIT_FOR_DATA,
	ESC_SERIAL_WAIT_FOR_PROCESS,
	ESC_SERIAL_SENDING_LOG,
	ESC_SERIAL_SENDING_CONFIG,
};

static struct esc_serial_state {
	enum esc_serial_parser_state state;
	enum esc_serial_command command;
	uint8_t buffer[MAX_SERIAL_BUFFER_SIZE];
	uint32_t buffer_pointer;
	int32_t crc;
	int32_t sending_pointer;
} esc_serial_state;

/**
 * Initialize the esc serial parser
 */
int32_t esc_serial_init()
{
	esc_serial_state.buffer_pointer = 0;
	esc_serial_state.state = ESC_SERIAL_WAIT_SYNC;
	return 0;
}

/**
 * Parse a character from the comms
 * @param[in] c if less than zero thrown out
 * @return 0 if success, -1 if failure
 */
int32_t esc_serial_parse(int32_t c)
{
	if (c < 0 || c > 0xff)
		return -1;
	
	switch (esc_serial_state.state) {
		case ESC_SERIAL_WAIT_SYNC:
			if (c == ESC_SYNC_BYTE) {
				esc_serial_state.state = ESC_SERIAL_WAIT_FOR_COMMAND;
				esc_serial_state.buffer_pointer = 0;
			}
			break;
		case ESC_SERIAL_WAIT_FOR_COMMAND:
			if (c >= ESC_COMMAND_LAST) {
				esc_serial_state.state = ESC_SERIAL_WAIT_SYNC;
				return 0;
			}			
			esc_serial_state.command = c;
			if (esc_command_data_size[esc_serial_state.command] == 0) {
				esc_serial_state.state = ESC_SERIAL_WAIT_FOR_PROCESS;
				return esc_serial_command_received();
			}
			esc_serial_state.state = ESC_SERIAL_WAIT_FOR_DATA;
			break;
		case ESC_SERIAL_WAIT_FOR_DATA:
			esc_serial_state.buffer[esc_serial_state.buffer_pointer] = c;
			esc_serial_state.buffer_pointer++;
			if (esc_serial_state.buffer_pointer >= esc_command_data_size[esc_serial_state.command]) {
				esc_serial_state.state = ESC_SERIAL_WAIT_FOR_PROCESS;
				return esc_serial_command_received();
			}
			break;
		case ESC_SERIAL_WAIT_FOR_PROCESS:
			break;
		default:
			PIOS_DEBUG_Assert(0);
	}
	
	return 0;
}

extern EscSettingsData config;
extern EscStatusData status;
extern struct esc_control esc_control;

/**
 * Process a command once it is parsed
 */
static int32_t esc_serial_command_received()
{
	int32_t retval = -1;
	
	switch(esc_serial_state.command) {
		case ESC_COMMAND_SET_CONFIG:
			memcpy((uint8_t *) &config, (uint8_t *) esc_serial_state.buffer, sizeof(config));
			break;
		case ESC_COMMAND_GET_CONFIG:
			// TODO: Send esc configuration
			retval = PIOS_COM_SendBufferNonBlocking(PIOS_COM_DEBUG, (uint8_t *) &config, sizeof(config));
			break;
		case ESC_COMMAND_SAVE_CONFIG:
			esc_control.save_requested = true;
			retval = 0;
			break;
		case ESC_COMMAND_ENABLE_SERIAL_LOGGING:
			esc_control.serial_logging_enabled = true;
			retval = 0;
			break;
		case ESC_COMMAND_DISABLE_SERIAL_LOGGING:
			esc_control.serial_logging_enabled = false;
			retval = 0;
			break;
		case ESC_COMMAND_REBOOT_BL:
			retval = -1;
			break;
		case ESC_COMMAND_ENABLE_SERIAL_CONTROL:
			esc_control.control_method = ESC_CONTROL_SERIAL;
			retval = 0;
			break;
		case ESC_COMMAND_DISABLE_SERIAL_CONTROL:
			esc_control.control_method = ESC_CONTROL_PWM;
			retval = 0;
			break;
		case ESC_COMMAND_SET_SPEED:
		{
			int16_t new_speed = esc_serial_state.buffer[0] | (esc_serial_state.buffer[1] << 8);
			if(new_speed >= 0 || new_speed < 10000) {
				esc_control.serial_input = new_speed;
				retval = 0;
			} else 
				retval = -1;
		}
			break;
		case ESC_COMMAND_WHOAMI:
		{
			uint8_t retbuf[34];
			PIOS_SYS_SerialNumberGetBinary(&retbuf[1]);
			retbuf[0] = 0x73;
			retbuf[33] = 0x73;
			PIOS_COM_SendBuffer(PIOS_COM_DEBUG, retbuf, sizeof(retbuf));
		}
			break;
		case ESC_COMMAND_ENABLE_ADC_LOGGING:
			// Reset log poitner to zero and enable logging
			esc_logger_init();
			esc_control.backbuffer_logging_status = ESC_LOGGING_CAPTURE;
			break;
		case ESC_COMMAND_GET_ADC_LOG:
			if(esc_control.backbuffer_logging_status == ESC_LOGGING_FULL) {
				esc_serial_state.sending_pointer = 0; // Indicate we have not sent any of the buffer yet
				esc_serial_state.state = ESC_SERIAL_SENDING_LOG;
				esc_control.backbuffer_logging_status = ESC_LOGGING_ECHO;
				return 0; // Must return here to avoid falling through the esc_serial_init
			} else
				retval = -1;
			break;
		case ESC_COMMAND_SET_PWM_FREQ:
		{
			uint16_t new_freq = esc_serial_state.buffer[0] | (esc_serial_state.buffer[1] << 8);
			uint16_t pwm_base_rate=(PIOS_MASTER_CLOCK/2 / new_freq) - 1; //Divide by two in order to actually command PWM frequency, and not Timer frequency
			TIM_SetAutoreload(TIM2, pwm_base_rate);
			TIM_SetAutoreload(TIM3, pwm_base_rate);
		}
			break;
		case ESC_COMMAND_GET_STATUS:
			retval = PIOS_COM_SendBufferNonBlocking(PIOS_COM_DEBUG, (uint8_t *) &status, sizeof(status));
			break;
		default:
			PIOS_DEBUG_Assert(0);
	}

	esc_serial_init();
	
	return retval;
}

/**
 * Perform any period tasks like sending back data
 */
 
uint32_t bytes_sent;
int32_t esc_serial_process()
{
	switch(esc_serial_state.state) {
		case ESC_SERIAL_SENDING_LOG:
			// Send next part of ADC log
			if(esc_control.backbuffer_logging_status == ESC_LOGGING_ECHO) {
				if(PIOS_COM_ReceiveBufferUsed(PIOS_COM_DEBUG) == 0) {
				
					uint32_t to_send = esc_logger_getlength() - esc_serial_state.sending_pointer;
					to_send = (to_send > 2) ? 2 : to_send;
					bytes_sent+= to_send;
					// Send some samples from the buffer
					uint8_t * buf = esc_logger_getbuffer();
					esc_serial_state.sending_pointer += (PIOS_COM_SendBuffer(PIOS_COM_DEBUG, &buf[esc_serial_state.sending_pointer], to_send) == 0) ? to_send : 0;
					
					if (esc_serial_state.sending_pointer >= esc_logger_getlength()) {
						esc_serial_state.state = ESC_SERIAL_WAIT_SYNC;
						esc_control.backbuffer_logging_status = ESC_LOGGING_NONE;
						esc_serial_state.sending_pointer = 0;
					}
				}
				
			}
			return 0;
			break;
		default:
			return 0;
	}
	
	return -1;
}