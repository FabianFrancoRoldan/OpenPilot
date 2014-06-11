/**
 ******************************************************************************
 *
 * @file       gyrobiascalibrationmodel.cpp
 * @author     The OpenPilot Team, http://www.openpilot.org Copyright (C) 2014.
 * @addtogroup board level calibration
 * @{
 * @addtogroup ConfigPlugin Config Plugin
 * @{
 * @brief Telemetry configuration panel
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

#include <attitudestate.h>
#include <attitudesettings.h>
#include "extensionsystem/pluginmanager.h"
#include <gyrostate.h>
#include <gyrosensor.h>
#include <revocalibration.h>
#include <accelgyrosettings.h>
#include "calibration/gyrobiascalibrationmodel.h"
#include "calibration/calibrationutils.h"
#include "calibration/calibrationuiutils.h"

static const int LEVEL_SAMPLES = 100;
#include "gyrobiascalibrationmodel.h"
namespace OpenPilot {
GyroBiasCalibrationModel::GyroBiasCalibrationModel(QObject *parent) :
    QObject(parent),
    collectingData(false)
{}


/******* gyro bias zero ******/
void GyroBiasCalibrationModel::start()
{
    // Store and reset board rotation before calibration starts
    storeAndClearBoardRotation();

    disableAllCalibrations();
    progressChanged(0);

    RevoCalibration *revoCalibration = RevoCalibration::GetInstance(getObjectManager());
    Q_ASSERT(revoCalibration);
    RevoCalibration::DataFields revoCalibrationData = revoCalibration->getData();
    revoCalibrationData.BiasCorrectedRaw = RevoCalibration::BIASCORRECTEDRAW_FALSE;
    revoCalibration->setData(revoCalibrationData);
    revoCalibration->updated();

    // Disable gyro bias correction while calibrating
    AttitudeSettings *attitudeSettings = AttitudeSettings::GetInstance(getObjectManager());
    Q_ASSERT(attitudeSettings);
    AttitudeSettings::DataFields attitudeSettingsData = attitudeSettings->getData();
    attitudeSettingsData.BiasCorrectGyro = AttitudeSettings::BIASCORRECTGYRO_FALSE;
    attitudeSettings->setData(attitudeSettingsData);
    attitudeSettings->updated();

    displayVisualHelp(CALIBRATION_HELPER_PLANE_PREFIX + CALIBRATION_HELPER_IMAGE_NED);
    displayInstructions(tr("Calibrating the gyroscopes. Keep the copter/plane steady..."), true);

    gyro_accum_x.clear();
    gyro_accum_y.clear();
    gyro_accum_z.clear();

    gyro_state_accum_x.clear();
    gyro_state_accum_y.clear();
    gyro_state_accum_z.clear();

    UAVObject::Metadata metadata;

    GyroState *gyroState = GyroState::GetInstance(getObjectManager());
    Q_ASSERT(gyroState);
    initialGyroStateMdata = gyroState->getMetadata();
    metadata = initialGyroStateMdata;
    UAVObject::SetFlightTelemetryUpdateMode(metadata, UAVObject::UPDATEMODE_PERIODIC);
    metadata.flightTelemetryUpdatePeriod = 100;
    gyroState->setMetadata(metadata);

    UAVObject::Metadata gyroSensorMetadata;
    GyroSensor *gyroSensor = GyroSensor::GetInstance(getObjectManager());
    Q_ASSERT(gyroSensor);
    initialGyroSensorMdata = gyroSensor->getMetadata();
    gyroSensorMetadata     = initialGyroSensorMdata;
    UAVObject::SetFlightTelemetryUpdateMode(gyroSensorMetadata, UAVObject::UPDATEMODE_PERIODIC);
    gyroSensorMetadata.flightTelemetryUpdatePeriod = 100;
    gyroSensor->setMetadata(gyroSensorMetadata);

    // Now connect to the accels and mag updates, gather for 100 samples
    collectingData = true;
    connect(gyroState, SIGNAL(objectUpdated(UAVObject *)), this, SLOT(getSample(UAVObject *)));
    connect(gyroSensor, SIGNAL(objectUpdated(UAVObject *)), this, SLOT(getSample(UAVObject *)));
}

/**
   Updates the gyro bias raw values
 */
void GyroBiasCalibrationModel::getSample(UAVObject *obj)
{
    QMutexLocker lock(&sensorsUpdateLock);

    Q_UNUSED(lock);

    switch (obj->getObjID()) {
    case GyroState::OBJID:
    {
        GyroState *gyroState = GyroState::GetInstance(getObjectManager());
        Q_ASSERT(gyroState);
        GyroState::DataFields gyroStateData = gyroState->getData();

        gyro_state_accum_x.append(gyroStateData.x);
        gyro_state_accum_y.append(gyroStateData.y);
        gyro_state_accum_z.append(gyroStateData.z);
        break;
    }
    case GyroSensor::OBJID:
    {
        GyroSensor *gyroSensor = GyroSensor::GetInstance(getObjectManager());
        Q_ASSERT(gyroSensor);
        GyroSensor::DataFields gyroSensorData = gyroSensor->getData();

        gyro_accum_x.append(gyroSensorData.x);
        gyro_accum_y.append(gyroSensorData.y);
        gyro_accum_z.append(gyroSensorData.z);
        break;
    }
    default:
        Q_ASSERT(0);
    }

    // Work out the progress based on whichever has less
    double p1 = (double)gyro_state_accum_x.size() / (double)LEVEL_SAMPLES;
    double p2 = (double)gyro_accum_y.size() / (double)LEVEL_SAMPLES;
    progressChanged(((p1 > p2) ? p1 : p2) * 100);

    if ((gyro_accum_y.size() >= LEVEL_SAMPLES || (gyro_accum_y.size() == 0 && gyro_state_accum_y.size() >= LEVEL_SAMPLES)) &&
        collectingData == true) {
        collectingData = false;

        GyroState *gyroState = GyroState::GetInstance(getObjectManager());
        disconnect(gyroState, SIGNAL(objectUpdated(UAVObject *)), this, SLOT(getSample(UAVObject *)));
        Q_ASSERT(gyroState);

        GyroSensor *gyroSensor = GyroSensor::GetInstance(getObjectManager());
        Q_ASSERT(gyroSensor);
        disconnect(gyroSensor, SIGNAL(objectUpdated(UAVObject *)), this, SLOT(getSample(UAVObject *)));

        enableAllCalibrations();

        RevoCalibration *revoCalibration     = RevoCalibration::GetInstance(getObjectManager());
        Q_ASSERT(revoCalibration);
        AccelGyroSettings *accelGyroSettings = AccelGyroSettings::GetInstance(getObjectManager());
        Q_ASSERT(accelGyroSettings);

        RevoCalibration::DataFields revoCalibrationData     = revoCalibration->getData();
        AccelGyroSettings::DataFields accelGyroSettingsData = accelGyroSettings->getData();

        revoCalibrationData.BiasCorrectedRaw = RevoCalibration::BIASCORRECTEDRAW_TRUE;
        // Update biases based on collected data
        // check whether the board does supports gyroSensor(updates were received)
        if (gyro_accum_x.count() < LEVEL_SAMPLES / 10) {
            accelGyroSettingsData.gyro_bias[AccelGyroSettings::GYRO_BIAS_X] += OpenPilot::CalibrationUtils::listMean(gyro_state_accum_x);
            accelGyroSettingsData.gyro_bias[AccelGyroSettings::GYRO_BIAS_Y] += OpenPilot::CalibrationUtils::listMean(gyro_state_accum_y);
            accelGyroSettingsData.gyro_bias[AccelGyroSettings::GYRO_BIAS_Z] += OpenPilot::CalibrationUtils::listMean(gyro_state_accum_z);
        } else {
            accelGyroSettingsData.gyro_bias[AccelGyroSettings::GYRO_BIAS_X] += OpenPilot::CalibrationUtils::listMean(gyro_accum_x);
            accelGyroSettingsData.gyro_bias[AccelGyroSettings::GYRO_BIAS_Y] += OpenPilot::CalibrationUtils::listMean(gyro_accum_y);
            accelGyroSettingsData.gyro_bias[AccelGyroSettings::GYRO_BIAS_Z] += OpenPilot::CalibrationUtils::listMean(gyro_accum_z);
        }

        revoCalibration->setData(revoCalibrationData);
        revoCalibration->updated();
        accelGyroSettings->setData(accelGyroSettingsData);
        accelGyroSettings->updated();

        AttitudeSettings *attitudeSettings = AttitudeSettings::GetInstance(getObjectManager());
        Q_ASSERT(attitudeSettings);
        AttitudeSettings::DataFields attitudeSettingsData = attitudeSettings->getData();
        attitudeSettingsData.BiasCorrectGyro = AttitudeSettings::BIASCORRECTGYRO_TRUE;
        attitudeSettings->setData(attitudeSettingsData);
        attitudeSettings->updated();

        gyroState->setMetadata(initialGyroStateMdata);
        gyroSensor->setMetadata(initialGyroSensorMdata);

        displayInstructions(tr("Gyroscope calibration computed succesfully."), false);
        displayVisualHelp(CALIBRATION_HELPER_IMAGE_EMPTY);

        // Recall saved board rotation
        recallBoardRotation();
    }
}


UAVObjectManager *GyroBiasCalibrationModel::getObjectManager()
{
    ExtensionSystem::PluginManager *pm = ExtensionSystem::PluginManager::instance();
    UAVObjectManager *objMngr = pm->getObject<UAVObjectManager>();

    Q_ASSERT(objMngr);
    return objMngr;
}
}
