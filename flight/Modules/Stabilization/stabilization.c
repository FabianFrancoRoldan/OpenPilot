/**
 ******************************************************************************
 * @addtogroup OpenPilotModules OpenPilot Modules
 * @{
 * @addtogroup StabilizationModule Stabilization Module
 * @brief Stabilization PID loops in an airframe type independent manner
 * @note This object updates the @ref ActuatorDesired "Actuator Desired" based on the
 * PID loops on the @ref AttitudeDesired "Attitude Desired" and @ref AttitudeActual "Attitude Actual"
 * @{
 *
 * @file       stabilization.c
 * @author     The OpenPilot Team, http://www.openpilot.org Copyright (C) 2010.
 * @brief      Attitude stabilization module.
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

#include "openpilot.h"
#include "stabilization.h"
#include "stabilizationsettings.h"
#include "stabilizationstatus.h"
#include "actuatordesired.h"
#include "ratedesired.h"
#include "stabilizationdesired.h"
#include "attitudeactual.h"
#include "gyros.h"
#include "flightstatus.h"
#include "manualcontrol.h" // Just to get a macro
#include "CoordinateConversions.h"

// Private constants
#define MAX_QUEUE_SIZE 1

#if defined(PIOS_STABILIZATION_STACK_SIZE)
#define STACK_SIZE_BYTES PIOS_STABILIZATION_STACK_SIZE
#else
#define STACK_SIZE_BYTES 800
#endif

#define TASK_PRIORITY (tskIDLE_PRIORITY+4)
#define FAILSAFE_TIMEOUT_MS 30

enum {PID_RATE_ROLL, PID_RATE_PITCH, PID_RATE_YAW, PID_ROLL, PID_PITCH, PID_YAW, PID_MAX};

enum {ROLL,PITCH,YAW,MAX_AXES};

// Private types
typedef struct {
	float p;
	float i;
	float d;
	float iLim;
	float iAccumulator;
	float lastErr;
#if defined(PIOS_SELFADJUSTING_STABILIZATION) || defined(DIAGNOSTICS)
	float e1;
	float e2;
#endif
#if defined(PIOS_SELFADJUSTING_STABILIZATION)
	float maxScale;
	float attack;
	float decay;
	float scale;
#endif
} pid_type;

// Private variables
static xTaskHandle taskHandle;
static StabilizationSettingsData settings;
static xQueueHandle queue;
float dT = 1;
float gyro_alpha = 0;
float error_alpha = 0;
float gyro_filtered[3] = {0,0,0};
float axis_lock_accum[3] = {0,0,0};
uint8_t max_axis_lock = 0;
uint8_t max_axislock_rate = 0;
float weak_leveling_kp = 0;
uint8_t weak_leveling_max = 0;
bool lowThrottleZeroIntegral;

pid_type pids[PID_MAX];

// Private functions
static void stabilizationTask(void* parameters);
static float ApplyPid(pid_type * pid, const float err);
static float bound(float val);
static void ZeroPids(void);
static void SettingsUpdatedCb(UAVObjEvent * ev);
#if defined(PIOS_SELFADJUSTING_STABILIZATION)
static float fakePow(const float n,const float x);
#endif

/**
 * Module initialization
 */
int32_t StabilizationStart()
{
	// Initialize variables

	// Start main task
	xTaskCreate(stabilizationTask, (signed char*)"Stabilization", STACK_SIZE_BYTES/4, NULL, TASK_PRIORITY, &taskHandle);
	TaskMonitorAdd(TASKINFO_RUNNING_STABILIZATION, taskHandle);
	PIOS_WDG_RegisterFlag(PIOS_WDG_STABILIZATION);
	return 0;
}

/**
 * Module initialization
 */
int32_t StabilizationInitialize()
{
	// Initialize variables
	StabilizationSettingsInitialize();
	ActuatorDesiredInitialize();
	GyrosInitialize();
#if defined(DIAGNOSTICS)
	RateDesiredInitialize();
	StabilizationStatusInitialize();
#endif

	// Create object queue
	queue = xQueueCreate(MAX_QUEUE_SIZE, sizeof(UAVObjEvent));

	// Listen for updates.
	//	AttitudeActualConnectQueue(queue);
	GyrosConnectQueue(queue);

	StabilizationSettingsConnectCallback(SettingsUpdatedCb);
	SettingsUpdatedCb(StabilizationSettingsHandle());
	// Start main task

	return 0;
}

MODULE_INITCALL(StabilizationInitialize, StabilizationStart)

/**
 * Module task
 */
static void stabilizationTask(void* parameters)
{
	UAVObjEvent ev;

	uint32_t timeval = PIOS_DELAY_GetRaw();

	ActuatorDesiredData actuatorDesired;
	StabilizationDesiredData stabDesired;
	RateDesiredData rateDesired;
	AttitudeActualData attitudeActual;
	GyrosData gyrosData;
	FlightStatusData flightStatus;
#if defined(DIAGNOSTICS)
	StabilizationStatusData stabilizationStatus;
#endif

	SettingsUpdatedCb((UAVObjEvent *) NULL);

#if defined(DIAGNOSTICS)
	StabilizationStatusGet(&stabilizationStatus);
#endif

	// Main task loop
	ZeroPids();
	while(1) {
		PIOS_WDG_UpdateFlag(PIOS_WDG_STABILIZATION);

		// Wait until the AttitudeRaw object is updated, if a timeout then go to failsafe
		if ( xQueueReceive(queue, &ev, FAILSAFE_TIMEOUT_MS / portTICK_RATE_MS) != pdTRUE )
		{
			AlarmsSet(SYSTEMALARMS_ALARM_STABILIZATION,SYSTEMALARMS_ALARM_WARNING);
			continue;
		}

		dT = PIOS_DELAY_DiffuS(timeval) * 1.0e-6f;
		timeval = PIOS_DELAY_GetRaw();

		FlightStatusGet(&flightStatus);
		StabilizationDesiredGet(&stabDesired);
		AttitudeActualGet(&attitudeActual);
		GyrosGet(&gyrosData);

#if defined(DIAGNOSTICS)
		RateDesiredGet(&rateDesired);
#endif

#if defined(PIOS_QUATERNION_STABILIZATION)
		// Quaternion calculation of error in each axis.  Uses more memory.
		float rpy_desired[3];
		float q_desired[4];
		float q_error[4];
		float local_error[3];

		// Essentially zero errors for anything in rate or none
		if(stabDesired.StabilizationMode[STABILIZATIONDESIRED_STABILIZATIONMODE_ROLL] == STABILIZATIONDESIRED_STABILIZATIONMODE_ATTITUDE)
			rpy_desired[0] = stabDesired.Roll;
		else
			rpy_desired[0] = attitudeActual.Roll;

		if(stabDesired.StabilizationMode[STABILIZATIONDESIRED_STABILIZATIONMODE_PITCH] == STABILIZATIONDESIRED_STABILIZATIONMODE_ATTITUDE)
			rpy_desired[1] = stabDesired.Pitch;
		else
			rpy_desired[1] = attitudeActual.Pitch;

		if(stabDesired.StabilizationMode[STABILIZATIONDESIRED_STABILIZATIONMODE_YAW] == STABILIZATIONDESIRED_STABILIZATIONMODE_ATTITUDE)
			rpy_desired[2] = stabDesired.Yaw;
		else
			rpy_desired[2] = attitudeActual.Yaw;

		RPY2Quaternion(rpy_desired, q_desired);
		quat_inverse(q_desired);
		quat_mult(q_desired, &attitudeActual.q1, q_error);
		quat_inverse(q_error);
		Quaternion2RPY(q_error, local_error);

#else
		// Simpler algorithm for CC, less memory
		float local_error[3] = {stabDesired.Roll - attitudeActual.Roll,
			stabDesired.Pitch - attitudeActual.Pitch,
			stabDesired.Yaw - attitudeActual.Yaw};
		local_error[2] = fmodf(local_error[2] + 180, 360) - 180;
#endif


		gyro_filtered[0] = gyro_filtered[0] * gyro_alpha + gyrosData.x * (1 - gyro_alpha);
		gyro_filtered[1] = gyro_filtered[1] * gyro_alpha + gyrosData.y * (1 - gyro_alpha);
		gyro_filtered[2] = gyro_filtered[2] * gyro_alpha + gyrosData.z * (1 - gyro_alpha);

		float *attitudeDesiredAxis = &stabDesired.Roll;
		float *actuatorDesiredAxis = &actuatorDesired.Roll;
		float *rateDesiredAxis = &rateDesired.Roll;

		//Calculate desired rate
		for(uint8_t i=0; i< MAX_AXES; i++)
		{
			switch(stabDesired.StabilizationMode[i])
			{
				case STABILIZATIONDESIRED_STABILIZATIONMODE_RATE:
					rateDesiredAxis[i] = attitudeDesiredAxis[i];

					// Zero attitude and axis lock accumulators
					pids[PID_ROLL + i].iAccumulator = 0;
					axis_lock_accum[i] = 0;
					break;

				case STABILIZATIONDESIRED_STABILIZATIONMODE_WEAKLEVELING:
				{
					float weak_leveling = local_error[i] * weak_leveling_kp;

					if(weak_leveling > weak_leveling_max)
						weak_leveling = weak_leveling_max;
					if(weak_leveling < -weak_leveling_max)
						weak_leveling = -weak_leveling_max;

					rateDesiredAxis[i] = attitudeDesiredAxis[i] + weak_leveling;

					// Zero attitude and axis lock accumulators
					pids[PID_ROLL + i].iAccumulator = 0;
					axis_lock_accum[i] = 0;
					break;
				}
				case STABILIZATIONDESIRED_STABILIZATIONMODE_ATTITUDE:
					rateDesiredAxis[i] = ApplyPid(&pids[PID_ROLL + i], local_error[i]);
#if defined(DIAGNOSTICS)
					stabilizationStatus.IAccumulator[PID_ROLL + i] = pids[PID_ROLL + i].iAccumulator;
					stabilizationStatus.E1[PID_ROLL + i] = pids[PID_ROLL + i].e1;
					stabilizationStatus.E2[PID_ROLL + i] = pids[PID_ROLL + i].e2;
#if defined(PIOS_SELFADJUSTING_STABILIZATION)
					stabilizationStatus.ScaleFactor[PID_ROLL + i] = fakePow(pids[PID_ROLL + i].maxScale, pids[PID_ROLL + i].scale);
#endif
#endif
					
					if(rateDesiredAxis[i] > settings.MaximumRate[i])
						rateDesiredAxis[i] = settings.MaximumRate[i];
					else if(rateDesiredAxis[i] < -settings.MaximumRate[i])
						rateDesiredAxis[i] = -settings.MaximumRate[i];
					
					
					axis_lock_accum[i] = 0;
					break;

				case STABILIZATIONDESIRED_STABILIZATIONMODE_AXISLOCK:
					if(fabsf(attitudeDesiredAxis[i]) > max_axislock_rate) {
						// While getting strong commands act like rate mode
						rateDesiredAxis[i] = attitudeDesiredAxis[i];
						axis_lock_accum[i] = 0;
					} else {
						// For weaker commands or no command simply attitude lock (almost) on no gyro change
						axis_lock_accum[i] += (attitudeDesiredAxis[i] - gyro_filtered[i]) * dT;
						if(axis_lock_accum[i] > max_axis_lock)
							axis_lock_accum[i] = max_axis_lock;
						else if(axis_lock_accum[i] < -max_axis_lock)
							axis_lock_accum[i] = -max_axis_lock;

						rateDesiredAxis[i] = ApplyPid(&pids[PID_ROLL + i], axis_lock_accum[i]);
#if defined(DIAGNOSTICS)
						stabilizationStatus.IAccumulator[PID_ROLL + i] = pids[PID_ROLL + i].iAccumulator;
						stabilizationStatus.E1[PID_ROLL + i] = pids[PID_ROLL + i].e1;
						stabilizationStatus.E2[PID_ROLL + i] = pids[PID_ROLL + i].e2;
#if defined(PIOS_SELFADJUSTING_STABILIZATION)
						stabilizationStatus.ScaleFactor[PID_ROLL + i] = fakePow(pids[PID_ROLL + i].maxScale, pids[PID_ROLL + i].scale);
#endif
#endif
					}
					
					if(rateDesiredAxis[i] > settings.MaximumRate[i])
						rateDesiredAxis[i] = settings.MaximumRate[i];
					else if(rateDesiredAxis[i] < -settings.MaximumRate[i])
						rateDesiredAxis[i] = -settings.MaximumRate[i];

					break;
			}
		}

		uint8_t shouldUpdate = 1;
#if defined(DIAGNOSTICS)
		RateDesiredSet(&rateDesired);
#endif
		ActuatorDesiredGet(&actuatorDesired);
		//Calculate desired command
		for(int8_t ct=0; ct< MAX_AXES; ct++)
		{
			switch(stabDesired.StabilizationMode[ct])
			{
				case STABILIZATIONDESIRED_STABILIZATIONMODE_RATE:
				case STABILIZATIONDESIRED_STABILIZATIONMODE_ATTITUDE:
				case STABILIZATIONDESIRED_STABILIZATIONMODE_AXISLOCK:
				case STABILIZATIONDESIRED_STABILIZATIONMODE_WEAKLEVELING:
				{
					float command = ApplyPid(&pids[PID_RATE_ROLL + ct],  rateDesiredAxis[ct] - gyro_filtered[ct]);
#if defined(DIAGNOSTICS)
					stabilizationStatus.IAccumulator[PID_RATE_ROLL + ct] = pids[PID_RATE_ROLL + ct].iAccumulator;
					stabilizationStatus.E1[PID_RATE_ROLL + ct] = pids[PID_RATE_ROLL + ct].e1;
					stabilizationStatus.E2[PID_RATE_ROLL + ct] = pids[PID_RATE_ROLL + ct].e2;
#if defined(PIOS_SELFADJUSTING_STABILIZATION)
					stabilizationStatus.ScaleFactor[PID_RATE_ROLL + ct] = fakePow(pids[PID_RATE_ROLL + ct].maxScale, pids[PID_RATE_ROLL + ct].scale);
#endif
#endif
					actuatorDesiredAxis[ct] = bound(command);
					break;
				}
				case STABILIZATIONDESIRED_STABILIZATIONMODE_NONE:
					switch (ct)
				{
					case ROLL:
						actuatorDesiredAxis[ct] = bound(attitudeDesiredAxis[ct]);
						shouldUpdate = 1;
						pids[PID_RATE_ROLL].iAccumulator = 0;
						pids[PID_ROLL].iAccumulator = 0;
						break;
					case PITCH:
						actuatorDesiredAxis[ct] = bound(attitudeDesiredAxis[ct]);
						shouldUpdate = 1;
						pids[PID_RATE_PITCH].iAccumulator = 0;
						pids[PID_PITCH].iAccumulator = 0;
						break;
					case YAW:
						actuatorDesiredAxis[ct] = bound(attitudeDesiredAxis[ct]);
						shouldUpdate = 1;
						pids[PID_RATE_YAW].iAccumulator = 0;
						pids[PID_YAW].iAccumulator = 0;
						break;
				}
					break;

			}
		}

#if defined(DIAGNOSTICS)
		StabilizationStatusSet(&stabilizationStatus);
#endif

		// Save dT
		actuatorDesired.UpdateTime = dT * 1000;

		if(PARSE_FLIGHT_MODE(flightStatus.FlightMode) == FLIGHTMODE_MANUAL)
			shouldUpdate = 0;

		if(shouldUpdate)
		{
			actuatorDesired.Throttle = stabDesired.Throttle;
			if(dT > 15)
				actuatorDesired.NumLongUpdates++;
			ActuatorDesiredSet(&actuatorDesired);
		}

		if(flightStatus.Armed != FLIGHTSTATUS_ARMED_ARMED ||
		   (lowThrottleZeroIntegral && stabDesired.Throttle < 0) ||
		   !shouldUpdate)
		{
			ZeroPids();
		}


		// Clear alarms
		AlarmsClear(SYSTEMALARMS_ALARM_STABILIZATION);
	}
}

float ApplyPid(pid_type * pid, const float err)
{

#if defined(PIOS_SELFADJUSTING_STABILIZATION)
	float scaleFactor = fakePow(pid->maxScale,pid->scale);
	// note that the scaling factor is always 1.0 if maxScale is set to 1 or
	// lower (the default) regardless of scale.
#else
	#define scaleFactor 1.0f
#endif

	float diff = (err - pid->lastErr);
	pid->lastErr = err;

	// Scale up accumulator by 1000 while computing to avoid losing precision
	pid->iAccumulator += scaleFactor * err * (pid->i * dT * 1000.0f);
	if(pid->iAccumulator > (pid->iLim * 1000.0f)) {
		pid->iAccumulator = pid->iLim * 1000.0f;
	} else if (pid->iAccumulator < -(pid->iLim * 1000.0f)) {
		pid->iAccumulator = -pid->iLim * 1000.0f;
	}

#if defined(PIOS_SELFADJUSTING_STABILIZATION) || defined(DIAGNOSTICS)

	float derivative = 0.0f;
	if ( dT > 0.0001f) {
		derivative = fabsf(diff) / dT;
	}

	// Calculate error indicators
	
	// We have two indicators, E1 for insufficient control loop efficiency
	// (indicated by the loop being unable to compensate an error in time), E2
	// for overcompensation (indicated by oscillation)
	
	// High E1 indicates coefficients too low.
	// E1 is the 'resident error' which is the error divided by its derivative.
	// E1 is high, if the error is high and does not change.
	// E1 is low if the error is low.
	// E1 is low if the error changes fast (In the hope that this change is a
	// decrease as the control loop tries to compensate).

	if (derivative>1.0f) {
		pid->e1 = pid->e1 * error_alpha + (fabsf(err)/derivative) * ( 1.0f - error_alpha );
	} else {
		pid->e1 = pid->e1 * error_alpha + ( fabsf(err) ) * ( 1.0f - error_alpha );
	}
	// High E2 indicates coefficients too high.
	// E2 is the 'zero crossing speed', which is the derivative of error
	// divided by the error.
	// E2 is high if the error is low but changes fast.
	// E2 is low if the error is high.
	// E2 is low if the error doesn't change much.
	
	if (fabsf(err)>1.0f) {
		pid->e2 = pid->e2 * error_alpha + ( derivative/fabsf(err) ) * ( 1.0f - error_alpha );
	} else {
		pid->e2 = pid->e2 * error_alpha + ( derivative ) * ( 1.0f - error_alpha );
	}
	//pid->e1=dT;
	//pid->e2=gyro_alpha;
	//pid->e2 += 1.0f;

	// Is the capping at "1" sensible? We need to prevent div by zero somehow, and
	// a cap at 1 prevents undesired amplification, but renders calculation nonlinear.

#endif // defined(PIOS_SELFADJUSTING_STABILIZATION) || defined(DIAGNOSTICS)
#if defined(PIOS_SELFADJUSTING_STABILIZATION)

	// Adjust scale coefficient according to E1 and E2
	pid->scale = pid->scale + (1.0f - error_alpha) * ( pid->e1 * pid->attack - pid->e2 * pid->decay);
	if (pid->scale > 1.0f) pid->scale = 1.0f;
	if (pid->scale < -1.0f) pid->scale = -1.0f;

#endif // defined(PIOS_SELFADJUSTING_STABILIZATION)

	return ((scaleFactor * err * pid->p) + pid->iAccumulator / 1000.0f + (scaleFactor * diff * pid->d / dT));
}


static void ZeroPids(void)
{
	for(int8_t ct = 0; ct < PID_MAX; ct++) {
		pids[ct].iAccumulator = 0.0f;
		pids[ct].lastErr = 0.0f;
#if defined(PIOS_SELFADJUSTING_STABILIZATION) || defined(DIAGNOSTICS)
		pids[ct].e1 = 0.0f;
		pids[ct].e2 = 0.0f;
#endif
#if defined(PIOS_SELFADJUSTING_STABILIZATION)
		pids[ct].scale = 0.0f;
#endif
	}
	for(uint8_t i = 0; i < 3; i++)
		axis_lock_accum[i] = 0.0f;
}


/**
 * Bound input value between limits
 */
static float bound(float val)
{
	if(val < -1.0f) {
		val = -1.0f;
	} else if(val > 1.0f) {
		val = 1.0f;
	}
	return val;
}


static void SettingsUpdatedCb(UAVObjEvent * ev)
{
	memset(pids,0,sizeof (pid_type) * PID_MAX);
	StabilizationSettingsGet(&settings);

	// Set the roll rate PID constants
	pids[0].p = settings.RollRatePID[STABILIZATIONSETTINGS_ROLLRATEPID_KP];
	pids[0].i = settings.RollRatePID[STABILIZATIONSETTINGS_ROLLRATEPID_KI];
	pids[0].d = settings.RollRatePID[STABILIZATIONSETTINGS_ROLLRATEPID_KD];
	pids[0].iLim = settings.RollRatePID[STABILIZATIONSETTINGS_ROLLRATEPID_ILIMIT];
#if defined(PIOS_SELFADJUSTING_STABILIZATION)
	pids[0].maxScale = settings.RollRatePID[STABILIZATIONSETTINGS_ROLLRATEPID_MAXSCALE];
	pids[0].attack = settings.RollRatePID[STABILIZATIONSETTINGS_ROLLRATEPID_ATTACK];
	pids[0].decay = settings.RollRatePID[STABILIZATIONSETTINGS_ROLLRATEPID_DECAY];
#endif

	// Set the pitch rate PID constants
	pids[1].p = settings.PitchRatePID[STABILIZATIONSETTINGS_PITCHRATEPID_KP];
	pids[1].i = settings.PitchRatePID[STABILIZATIONSETTINGS_PITCHRATEPID_KI];
	pids[1].d = settings.PitchRatePID[STABILIZATIONSETTINGS_PITCHRATEPID_KD];
	pids[1].iLim = settings.PitchRatePID[STABILIZATIONSETTINGS_PITCHRATEPID_ILIMIT];
#if defined(PIOS_SELFADJUSTING_STABILIZATION)
	pids[1].maxScale = settings.PitchRatePID[STABILIZATIONSETTINGS_PITCHRATEPID_MAXSCALE];
	pids[1].attack = settings.PitchRatePID[STABILIZATIONSETTINGS_PITCHRATEPID_ATTACK];
	pids[1].decay = settings.PitchRatePID[STABILIZATIONSETTINGS_PITCHRATEPID_DECAY];
#endif

	// Set the yaw rate PID constants
	pids[2].p = settings.YawRatePID[STABILIZATIONSETTINGS_YAWRATEPID_KP];
	pids[2].i = settings.YawRatePID[STABILIZATIONSETTINGS_YAWRATEPID_KI];
	pids[2].d = settings.YawRatePID[STABILIZATIONSETTINGS_YAWRATEPID_KD];
	pids[2].iLim = settings.YawRatePID[STABILIZATIONSETTINGS_YAWRATEPID_ILIMIT];
#if defined(PIOS_SELFADJUSTING_STABILIZATION)
	pids[2].maxScale = settings.YawRatePID[STABILIZATIONSETTINGS_YAWRATEPID_MAXSCALE];
	pids[2].attack = settings.YawRatePID[STABILIZATIONSETTINGS_YAWRATEPID_ATTACK];
	pids[2].decay = settings.YawRatePID[STABILIZATIONSETTINGS_YAWRATEPID_DECAY];
#endif

	// Set the roll attitude PI constants
	pids[3].p = settings.RollPI[STABILIZATIONSETTINGS_ROLLPI_KP];
	pids[3].i = settings.RollPI[STABILIZATIONSETTINGS_ROLLPI_KI];
	pids[3].iLim = settings.RollPI[STABILIZATIONSETTINGS_ROLLPI_ILIMIT];

	// Set the pitch attitude PI constants
	pids[4].p = settings.PitchPI[STABILIZATIONSETTINGS_PITCHPI_KP];
	pids[4].i = settings.PitchPI[STABILIZATIONSETTINGS_PITCHPI_KI];
	pids[4].iLim = settings.PitchPI[STABILIZATIONSETTINGS_PITCHPI_ILIMIT];

	// Set the yaw attitude PI constants
	pids[5].p = settings.YawPI[STABILIZATIONSETTINGS_YAWPI_KP];
	pids[5].i = settings.YawPI[STABILIZATIONSETTINGS_YAWPI_KI];
	pids[5].iLim = settings.YawPI[STABILIZATIONSETTINGS_YAWPI_ILIMIT];

	// Maximum deviation to accumulate for axis lock
	max_axis_lock = settings.MaxAxisLock;
	max_axislock_rate = settings.MaxAxisLockRate;

	// Settings for weak leveling
	weak_leveling_kp = settings.WeakLevelingKp;
	weak_leveling_max = settings.MaxWeakLevelingRate;

	// Whether to zero the PID integrals while throttle is low
	lowThrottleZeroIntegral = settings.LowThrottleZeroIntegral == STABILIZATIONSETTINGS_LOWTHROTTLEZEROINTEGRAL_TRUE;

	// The dT has some jitter iteration to iteration that we don't want to
	// make thie result unpredictable.  Still, it's nicer to specify the constant
	// based on a time (in ms) rather than a fixed multiplier.  The error between
	// update rates on OP (~300 Hz) and CC (~475 Hz) is negligible for this
	// calculation
#if defined(REVOLUTION)
	// grrr - negligible my ass!
	const float fakeDt = 0.0013;
#else
	const float fakeDt = 0.0025;
#endif
	if(settings.GyroTau < 0.0001f)
		gyro_alpha = 0.0f;   // not trusting this to resolve to 0
	else
		gyro_alpha = expf(-fakeDt  / settings.GyroTau);
	if(settings.ErrorTau < 0.0001f)
		error_alpha = 0.0f;   // not trusting this to resolve to 0
	else
		error_alpha = expf(-fakeDt  / settings.ErrorTau);
}

#if defined(PIOS_SELFADJUSTING_STABILIZATION)
// We need a performant implementation of y=n^x in the interval -1,+1
// for n>=1
// this is done using a hyperbolican aproximation:
// f(x) = (a / (b+x) + c)
// f(-1) = 1/n
// f(0) = 1
// f(1) = n
// ----------------
// a = -2*(n + 1)/(n - 1)
// b = -(v + 1)/(v - 1)
// c = -1
static float fakePow(const float n,const float x) {
	// make sure we are defined
	// prevent div by zero
    if (n<=1.0f) return 1.0f;
	if (x<-1.0f) return 1.0f/n;
	if (x>1.0f) return n;

	//fprintf(stderr,"debug: %4f ^ %4f = %4f\n",n,x,( ( ( -2 * (n+1) / (n-1) ) / ( x - (n+1) / (n-1) ) ) - 1 ));
	return ( ( ( -2.0f * (n+1.0f) / (n-1.0f) ) / ( x - (n+1.0f) / (n-1.0f) ) ) - 1.0f );
}
#endif

/**
 * @}
 * @}
 */
