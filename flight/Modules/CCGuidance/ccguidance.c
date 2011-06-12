/**
 ******************************************************************************
 *
 * @file       ccguidance.c
 * @author     The OpenPilot Team, http://www.openpilot.org Copyright (C) 2010.
 * @brief      CCGuidance for CopterControl. Fixed wing only.
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

/**
 * Input object: GPSPosition
 * Input object: ManualControlCommand
 * Output object: AttitudeDesired
 *
 * This module will periodically update the value of the AttitudeDesired object.
 *
 * The module executes in its own thread in this example.
 *
 * Modules have no API, all communication to other modules is done through UAVObjects.
 * However modules may use the API exposed by shared libraries.
 * See the OpenPilot wiki for more details.
 * http://www.openpilot.org/OpenPilot_Application_Architecture
 *
 */

#include "openpilot.h"
#include "ccguidancesettings.h"
#include "gpsposition.h"
#include "positiondesired.h"	// object that will be updated by the module
#include "manualcontrol.h"
#include "manualcontrolcommand.h"
#include "flightstatus.h"
#include "stabilizationdesired.h"
#include "systemsettings.h"

/**
 * Mad Hack: Integrate GPS code into CCGuidance Module
 * This allows reusing stack memory and also safes
 * another 256 byte of thread overhead
 */
#ifdef PIOS_GPS_INTEGRATED_TASK
	extern void gpsInit();
	extern void gpsLoop();
#endif

// Private constants
#define MAX_QUEUE_SIZE 1
#define CCG_STACK_SIZE_BYTES 500
#define GPS_STACK_SIZE_BYTES 800
#define CCG_TASK_PRIORITY (tskIDLE_PRIORITY+2)
#define RAD2DEG (180.0/M_PI)
#define DEG2RAD (M_PI/180.0)
#define GEE 9.81
#define XDelay (100 / portTICK_RATE_MS)
// Private types

// Private variables
static xTaskHandle ccguidanceTaskHandle;
#ifndef PIOS_GPS_INTEGRATED_TASK
static xQueueHandle queue;
#endif
static uint8_t positionHoldLast = 0;

// Private functions
static void ccguidanceTask(void *parameters);
static float bound(float val, float min, float max);
static float sphereDistance(float lat1,float long1,float lat2,float long2);
static float sphereCourse(float lat1,float long1,float lat2,float long2);

/**
 * Initialise the module, called on startup
 * \returns 0 on success or -1 if initialisation failed
 */
int32_t CCGuidanceInitialize()
{

#ifndef PIOS_GPS_INTEGRATED_TASK

	// Create object queue
	queue = xQueueCreate(MAX_QUEUE_SIZE, sizeof(UAVObjEvent));
	
	// Listen for updates.
	GPSPositionConnectQueue(queue);
	
	// Start main task
	xTaskCreate(ccguidanceTask, (signed char *)"Guidance", CCG_STACK_SIZE_BYTES/4, NULL, CCG_TASK_PRIORITY, &ccguidanceTaskHandle);

#else  // PIOS_GPS_INTEGRATED_TASK

	xTaskCreate(ccguidanceTask, (signed char *)"Guidance", GPS_STACK_SIZE_BYTES/4, NULL, CCG_TASK_PRIORITY, &ccguidanceTaskHandle);

#endif  // PIOS_GPS_INTEGRATED_TASK

	TaskMonitorAdd(TASKINFO_RUNNING_GUIDANCE, ccguidanceTaskHandle);

	return 0;
}

/**
 * Module thread, should not return.
 */
static void ccguidanceTask(void *parameters)
{
	SystemSettingsData systemSettings;
	CCGuidanceSettingsData ccguidanceSettings;
	ManualControlCommandData manualControl;
	FlightStatusData flightStatus;

	portTickType thisTime;
	portTickType lastUpdateTime;
	float dT;
	#ifndef PIOS_GPS_INTEGRATED_TASK
	UAVObjEvent ev;
	#endif

	float courseError;
	uint8_t alarm;

	#ifdef PIOS_GPS_INTEGRATED_TASK
		gpsInit();
	#endif
	
	// Main task loop
	lastUpdateTime = xTaskGetTickCount();
	while (1) {

		CCGuidanceSettingsGet(&ccguidanceSettings);

		#ifdef PIOS_GPS_INTEGRATED_TASK
			vTaskDelay(XDelay);
			gpsLoop();
			alarm=0;
		#else  // PIOS_GPS_INTEGRATED_TASK
			// Wait until the GPSPosition object is updated, if a timeout then go to failsafe
			if ( xQueueReceive(queue, &ev, ccguidanceSettings.UpdatePeriod / portTICK_RATE_MS) != pdTRUE )
			{
				alarm=SYSTEMALARMS_ALARM_WARNING;
			} else {
				alarm=0;
			}
		#endif  // PIOS_GPS_INTEGRATED_TASK
				
		// Continue collecting data if not enough time
		thisTime = xTaskGetTickCount();
		if( (thisTime - lastUpdateTime) < (ccguidanceSettings.UpdatePeriod / portTICK_RATE_MS) )
			continue;

		dT = (thisTime - lastUpdateTime) / portTICK_RATE_MS / 1000.0f;

		lastUpdateTime = thisTime;
		
		ManualControlCommandGet(&manualControl);
		FlightStatusGet(&flightStatus);
		SystemSettingsGet(&systemSettings);
		
		if ((PARSE_FLIGHT_MODE(flightStatus.FlightMode) == FLIGHTMODE_GUIDANCE) &&
		    ((systemSettings.AirframeType == SYSTEMSETTINGS_AIRFRAMETYPE_FIXEDWING) ||
		     (systemSettings.AirframeType == SYSTEMSETTINGS_AIRFRAMETYPE_FIXEDWINGELEVON) ||
		     (systemSettings.AirframeType == SYSTEMSETTINGS_AIRFRAMETYPE_FIXEDWINGVTAIL) ))
		{
			StabilizationDesiredData stabDesired;
			StabilizationDesiredGet(&stabDesired);
			PositionDesiredData positionDesired;
			PositionDesiredGet(&positionDesired);
			GPSPositionData positionActual;
			GPSPositionGet(&positionActual);

			if(positionHoldLast == 0) {
				/* When enter position hold mode save current position */
				positionDesired.North = positionActual.Latitude;
				positionDesired.East = positionActual.Longitude;
				positionDesired.Down = positionActual.Altitude;
				PositionDesiredSet(&positionDesired);
				positionHoldLast = 1;
			}

			/* safety */
			if (positionActual.Status==GPSPOSITION_STATUS_FIX3D) {
						
				/* main position hold loop */
				/* 1. Altitude */

				if (positionActual.Altitude < positionDesired.Down) {
					stabDesired.Pitch = ccguidanceSettings.Pitch[CCGUIDANCESETTINGS_PITCH_CLIMB];
				} else {
					stabDesired.Pitch = ccguidanceSettings.Pitch[CCGUIDANCESETTINGS_PITCH_SINK];
				}

				/* 2. Heading */
				courseError = sphereCourse(
					positionActual.Latitude * 1e-7,
					positionActual.Longitude * 1e-7,
					positionDesired.North * 1e-7,
					positionDesired.East * 1e-7
					) - positionActual.Heading;
				if (courseError<-180.) courseError+=360.;
				if (courseError>180.) courseError-=360.;

				stabDesired.Roll = bound(
					ccguidanceSettings.Roll[CCGUIDANCESETTINGS_ROLL_NEUTRAL] +
					ccguidanceSettings.Roll[CCGUIDANCESETTINGS_ROLL_KP] * courseError,
					-ccguidanceSettings.Roll[CCGUIDANCESETTINGS_ROLL_MAX],
					ccguidanceSettings.Roll[CCGUIDANCESETTINGS_ROLL_MAX]);

				if (positionActual.Groundspeed>0) {

					stabDesired.Yaw = RAD2DEG * sinf(
						( stabDesired.Roll - ccguidanceSettings.Roll[CCGUIDANCESETTINGS_ROLL_NEUTRAL]
						) / RAD2DEG
						) * GEE / positionActual.Groundspeed;
				} else {
					stabDesired.Yaw = 0;
				}

			} else {
				/* Fallback, no position data! */
				stabDesired.Yaw = 0;
				stabDesired.Pitch = ccguidanceSettings.Pitch[CCGUIDANCESETTINGS_PITCH_CLIMB];
				stabDesired.Roll = ccguidanceSettings.Roll[CCGUIDANCESETTINGS_ROLL_MAX];
				alarm = SYSTEMALARMS_ALARM_CRITICAL;
			}

			/* 3. Throttle (manual) */

			stabDesired.Throttle = manualControl.Throttle;
			stabDesired.StabilizationMode[STABILIZATIONDESIRED_STABILIZATIONMODE_ROLL] = STABILIZATIONDESIRED_STABILIZATIONMODE_ATTITUDE;
			stabDesired.StabilizationMode[STABILIZATIONDESIRED_STABILIZATIONMODE_PITCH] = STABILIZATIONDESIRED_STABILIZATIONMODE_ATTITUDE;
			stabDesired.StabilizationMode[STABILIZATIONDESIRED_STABILIZATIONMODE_YAW] = STABILIZATIONDESIRED_STABILIZATIONMODE_RATE;
			
			StabilizationDesiredSet(&stabDesired);

		} else {
			// reset globals...
			positionHoldLast = 0;
		}

		if (alarm!=0) {
			AlarmsSet(SYSTEMALARMS_ALARM_GUIDANCE,alarm);
		} else {
			AlarmsClear(SYSTEMALARMS_ALARM_GUIDANCE);
		}

	}
}


/**
 * Bound input value between limits
 */
static float bound(float val, float min, float max)
{
	if (val < min) {
		val = min;
	} else if (val > max) {
		val = max;
	}
	return val;
}

/**
 * calculate spherical distance and course between two coordinate pairs
 * see http://de.wikipedia.org/wiki/Orthodrome for details
 */
static float sphereDistance(float lat1, float long1, float lat2, float long2)
{
	float zeta=(RAD2DEG * acos (
		sin(DEG2RAD*lat1) * sin(DEG2RAD*lat2)
		+ cos(DEG2RAD*lat1) * cos(DEG2RAD*lat2) * cos(DEG2RAD*(long2-long1))
	));
	if (isnan(zeta)) {
		zeta=0;
	}
	return zeta;

}
static float sphereCourse(float lat1, float long1, float lat2, float long2)
{
	float zeta = sphereDistance(lat1,long1,lat2,long2);
	float angle = RAD2DEG * acos(
			( sin(DEG2RAD*lat2) - sin(DEG2RAD*lat1) * cos(DEG2RAD*zeta) )
			/ ( cos(DEG2RAD*lat1) * sin(DEG2RAD*zeta) )
		);
	if (isnan(angle)) angle=0;
	float diff=long2-long1;
	if (diff>180) diff-=360;
	if (diff<-180) diff+=360;
	if (diff>=0) {
		return angle;
	} else {
		return -angle;
	}
}

