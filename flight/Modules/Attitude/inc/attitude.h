/**
 ******************************************************************************
 * @addtogroup OpenPilotModules OpenPilot Modules
 * @{ 
 * @addtogroup Attitude Attitude Module
 * @{ 
 *
 * @file       attitude.h
 * @author     The OpenPilot Team, http://www.openpilot.org Copyright (C) 2011.
 * @brief      Acquires sensor data and fuses it into attitude estimate for CC
 *
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
#ifndef ATTITUDE_H
#define ATTITUDE_H

#include "openpilot.h"

struct GlobalAttitudeVariables {
	float accelKi;
	float accelKp;
	float yawBiasRate;
	float gyroGain[3];
	float gyroGain_ref;
	float accelbias[3];
	float gyro_correct_int[3];
	float q[4];
	float Rsb[3][3]; //Rotation matrix that transforms from the sensor frame to the body frame
	bool rotate;
	bool zero_during_arming;
	bool bias_correct_gyro;	
	bool filter_choice;
	
	// For running trim flights
	volatile bool trim_requested;
	volatile int32_t trim_accels[3];
	volatile int32_t trim_samples;
	
};


int32_t AttitudeInitialize(void);


#endif // ATTITUDE_H
