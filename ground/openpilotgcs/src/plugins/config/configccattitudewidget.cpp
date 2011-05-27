/**
 ******************************************************************************
 *
 * @file       configccattitudewidget.cpp
 * @author     The OpenPilot Team, http://www.openpilot.org Copyright (C) 2010.
 * @addtogroup GCSPlugins GCS Plugins
 * @{
 * @addtogroup ConfigPlugin Config Plugin
 * @{
 * @brief Configure Attitude module on CopterControl
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
#include "configccattitudewidget.h"
#include "ui_ccattitude.h"
#include "utils/coordinateconversions.h"
#include <QMutexLocker>
#include <QMessageBox>
#include <QDebug>
#include <QSignalMapper>

ConfigCCAttitudeWidget::ConfigCCAttitudeWidget(QWidget *parent) :
        ConfigTaskWidget(parent),
        ui(new Ui_ccattitude)
{
    ui->setupUi(this);
    connect(ui->zeroBias,SIGNAL(clicked()),this,SLOT(startAccelCalibration()));
    connect(ui->saveButton,SIGNAL(clicked()),this,SLOT(saveAttitudeSettings()));        
    connect(ui->applyButton,SIGNAL(clicked()),this,SLOT(applyAttitudeSettings()));
    connect(ui->getCurrentButton,SIGNAL(clicked()),this,SLOT(getCurrentAttitudeSettings()));

    // Make it smart:
    connect(parent, SIGNAL(autopilotConnected()),this, SLOT(getCurrentAttitudeSettings()));
    getCurrentAttitudeSettings(); // The 1st time this panel is instanciated, the autopilot is already connected.

    // Connect all the help buttons to signal mapper that passes button name to SLOT function
    QSignalMapper* signalMapper = new QSignalMapper(this);
    connect( ui->attitudeRotationHelp, SIGNAL(clicked()), signalMapper, SLOT(map()) );
    signalMapper->setMapping(ui->attitudeRotationHelp, ui->attitudeRotationHelp->objectName());
    connect( ui->attitudeCalibHelp, SIGNAL(clicked()), signalMapper, SLOT(map()) );
    signalMapper->setMapping(ui->attitudeCalibHelp, ui->attitudeCalibHelp->objectName());
    connect( ui->zeroOnArmHelp, SIGNAL(clicked()), signalMapper, SLOT(map()) );
    signalMapper->setMapping(ui->zeroOnArmHelp, ui->zeroOnArmHelp->objectName());
    connect( ui->commandHelp, SIGNAL(clicked()), signalMapper, SLOT(map()) );
    signalMapper->setMapping(ui->commandHelp, QString("commandHelp"));

    connect(signalMapper, SIGNAL(mapped(const QString &)), parent, SLOT(showHelp(const QString &)));
}

ConfigCCAttitudeWidget::~ConfigCCAttitudeWidget()
{
    delete ui;
}

void ConfigCCAttitudeWidget::attitudeRawUpdated(UAVObject * obj) {
    QMutexLocker locker(&startStop);

    ui->zeroBiasProgress->setValue((float) updates / NUM_ACCEL_UPDATES * 100);

    if(updates < NUM_ACCEL_UPDATES) {
        updates++;
        UAVObjectField * field = obj->getField(QString("accels"));
        x_accum.append(field->getDouble(0));
        y_accum.append(field->getDouble(1));
        z_accum.append(field->getDouble(2));
	qDebug("update %d: %f, %f, %f\n",updates,field->getDouble(0),field->getDouble(1),field->getDouble(2));
    } else if ( updates == NUM_ACCEL_UPDATES ) {
	updates++;
        timer.stop();
        disconnect(obj,SIGNAL(objectUpdated(UAVObject*)),this,SLOT(attitudeRawUpdated(UAVObject*)));
        disconnect(&timer,SIGNAL(timeout()),this,SLOT(timeout()));

        float x_bias = listMean(x_accum) / ACCEL_SCALE;
        float y_bias = listMean(y_accum) / ACCEL_SCALE;
        float z_bias = (listMean(z_accum) + 9.81) / ACCEL_SCALE;

        obj->setMetadata(initialMdata);

        UAVDataObject * settings = dynamic_cast<UAVDataObject*>(getObjectManager()->getObject(QString("AttitudeSettings")));
        UAVObjectField * field = settings->getField("AccelBias");
        field->setDouble(field->getDouble(0) + x_bias,0);
        field->setDouble(field->getDouble(1) + y_bias,1);
        field->setDouble(field->getDouble(2) + z_bias,2);
	qDebug("New X bias: %f\n", field->getDouble(0)+x_bias);
	qDebug("New Y bias: %f\n", field->getDouble(1)+y_bias);
	qDebug("New Z bias: %f\n", field->getDouble(2)+z_bias);
        settings->updated();
        ui->status->setText("Calibration done.");
    } else {
	// Possible to get here if weird threading stuff happens.  Just ignore updates.
	qDebug("Unexpected accel update received.");
    }
}

void ConfigCCAttitudeWidget::timeout() {
    QMutexLocker locker(&startStop);
    UAVDataObject * obj = dynamic_cast<UAVDataObject*>(getObjectManager()->getObject(QString("AttitudeRaw")));
    disconnect(obj,SIGNAL(objectUpdated(UAVObject*)),this,SLOT(attitudeRawUpdated(UAVObject*)));
    disconnect(&timer,SIGNAL(timeout()),this,SLOT(timeout()));

    QMessageBox msgBox;
    msgBox.setText(tr("Calibration timed out before receiving required updates."));
    msgBox.setStandardButtons(QMessageBox::Ok);
    msgBox.setDefaultButton(QMessageBox::Ok);
    msgBox.exec();

}

void ConfigCCAttitudeWidget::applyAttitudeSettings() {
    UAVDataObject * settings = dynamic_cast<UAVDataObject*>(getObjectManager()->getObject(QString("AttitudeSettings")));
    UAVObjectField * field = settings->getField("BoardRotation");

    field->setValue(ui->rollBias->value(),0);
    field->setValue(ui->pitchBias->value(),1);
    field->setValue(ui->yawBias->value(),2);

    field = settings->getField("ZeroDuringArming");
    // Handling of boolean values is done through enums on
    // uavobjects...
    field->setValue((ui->zeroGyroBiasOnArming->isChecked()) ? "TRUE": "FALSE");

    settings->updated();
}

void ConfigCCAttitudeWidget::getCurrentAttitudeSettings() {
    UAVDataObject * settings = dynamic_cast<UAVDataObject*>(getObjectManager()->getObject(QString("AttitudeSettings")));
    settings->requestUpdate();
    UAVObjectField * field = settings->getField("BoardRotation");
    ui->rollBias->setValue(field->getDouble(0));
    ui->pitchBias->setValue(field->getDouble(1));
    ui->yawBias->setValue(field->getDouble(2));
    field = settings->getField("ZeroDuringArming");
    // Handling of boolean values is done through enums on
    // uavobjects...
    bool enabled = (field->getValue().toString() == "FALSE") ? false : true;
    ui->zeroGyroBiasOnArming->setChecked(enabled);

}

void ConfigCCAttitudeWidget::startAccelCalibration() {
    QMutexLocker locker(&startStop);

    updates = 0;
    x_accum.clear();
    y_accum.clear();
    z_accum.clear();

    ui->status->setText(tr("Calibrating..."));

    // Set up to receive updates
    UAVDataObject * obj = dynamic_cast<UAVDataObject*>(getObjectManager()->getObject(QString("AttitudeRaw")));
    connect(obj,SIGNAL(objectUpdated(UAVObject*)),this,SLOT(attitudeRawUpdated(UAVObject*)));

    // Set up timeout timer
    timer.start(10000);
    connect(&timer,SIGNAL(timeout()),this,SLOT(timeout()));

    // Speed up updates
    initialMdata = obj->getMetadata();
    UAVObject::Metadata mdata = initialMdata;
    mdata.flightTelemetryUpdateMode = UAVObject::UPDATEMODE_PERIODIC;
    mdata.flightTelemetryUpdatePeriod = 100;
    obj->setMetadata(mdata);

}

void ConfigCCAttitudeWidget::saveAttitudeSettings() {
    applyAttitudeSettings();

    UAVDataObject * obj = dynamic_cast<UAVDataObject*>(getObjectManager()->getObject(QString("AttitudeSettings")));
    saveObjectToSD(obj);
}
