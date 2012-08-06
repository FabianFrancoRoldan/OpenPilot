/**
 ******************************************************************************
 *
 * @file       simulator.h
 * @author     The OpenPilot Team, http://www.openpilot.org Copyright (C) 2010.
 * @addtogroup GCSPlugins GCS Plugins
 * @{
 * @addtogroup HITLPlugin HITL Plugin
 * @{
 * @brief The Hardware In The Loop plugin
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

#ifndef ISIMULATOR_H
#define ISIMULATOR_H

#include <QObject>
#include <QUdpSocket>
#include <QTimer>
#include <QProcess>
#include <qmath.h>

#include "qscopedpointer.h"
#include "uavtalk/telemetrymanager.h"
#include "uavobjectmanager.h"

#include "accels.h"
#include "actuatordesired.h"
#include "actuatorcommand.h"
#include "attitudeactual.h"
#include "attitudesettings.h"
#include "baroaltitude.h"
#include "baroairspeed.h"
#include "gcsreceiver.h"
#include "gcstelemetrystats.h"
#include "gpsposition.h"
#include "gpsvelocity.h"
#include "gyros.h"
#include "flightstatus.h"
#include "homelocation.h"
#include "manualcontrolcommand.h"
#include "positionactual.h"
#include "sonaraltitude.h"
#include "velocityactual.h"

#include "utils/coordinateconversions.h"

/**
 * just imagine this was a class without methods and all public properties
 */
typedef struct _FLIGHT_PARAM {

    // time
    float T;
    float dT;
    unsigned int i;

    // speed (relative)
    float ias;
    float cas;
    float tas;
    float groundspeed;

    // position (absolute)
    float X;
    float Y;
    float Z;

    // speed (absolute)
    float dX;
    float dY;
    float dZ;

    // acceleration (absolute)
    float ddX;
    float ddY;
    float ddZ;

    //angle
    float azimuth;
    float pitch;
    float roll;

    //rotation speed
    float dAzimuth;
    float dPitch;
    float dRoll;

} FLIGHT_PARAM;

typedef struct _CONNECTION
{
    QString simulatorId;
    QString binPath;
    QString dataPath;
    QString hostAddress;
    QString remoteHostAddress;
    int outPort;
    int inPort;
    bool manual;
    bool startSim;
    bool addNoise;
    QString latitude;
    QString longitude;
} SimulatorSettings;


struct Output2OP{
    float latitude;
    float longitude;
    float altitude;
    float heading;
    float groundspeed; //[m/s]
    float calibratedAirspeed;    //[m/s]
    float pitch;
    float roll;
    float pressure;
    float temperature;
    float velNorth;   //[m/s]
    float velEast;    //[m/s]
    float velDown;    //[m/s]
    float dstN;       //[m]
    float dstE;       //[m]
    float dstD;       //[m]
    float accX;       //[m/s^2]
    float accY;       //[m/s^2]
    float accZ;       //[m/s^2]
    float rollRate;     //[deg/s]
    float pitchRate;     //[deg/s]
    float yawRate;     //[deg/s]
};


class Simulator : public QObject
{
    Q_OBJECT

public:
    Simulator(const SimulatorSettings& params);
    virtual ~Simulator();

    bool isAutopilotConnected() const { return autopilotConnectionStatus; }
    bool isSimulatorConnected() const { return simConnectionStatus; }
    QString Name() const { return name; }
    void setName(QString str) { name = str; }

    QString SimulatorId() const { return simulatorId; }
    void setSimulatorId(QString str) { simulatorId = str; }



    static bool IsStarted() { return isStarted; }
    static void setStarted(bool val) { isStarted = val; }
    static QStringList& Instances() { return Simulator::instances; }
    static void setInstance(const QString& str) { Simulator::instances.append(str); }

    virtual void stopProcess() {}
    virtual void setupUdpPorts(const QString& host, int inPort, int outPort) { Q_UNUSED(host) Q_UNUSED(inPort) Q_UNUSED(outPort)}

    void resetInitialHomePosition();
    void updateUAVOs(Output2OP out);

signals:
    void autopilotConnected();
    void autopilotDisconnected();
    void simulatorConnected();
    void simulatorDisconnected();
    void processOutput(QString str);
    void deleteSimProcess();
    void myStart();
public slots:
    Q_INVOKABLE virtual bool setupProcess() { return true;}
private slots:
    void onStart();
    //void transmitUpdate();
    void receiveUpdate();
    void onAutopilotConnect();
    void onAutopilotDisconnect();
    void onSimulatorConnectionTimeout();
    void telStatsUpdated(UAVObject* obj);
    Q_INVOKABLE void onDeleteSimulator(void);

    virtual void transmitUpdate() = 0;
    virtual void processUpdate(const QByteArray& data) = 0;

protected:
    static const float GEE;
    static const float FT2M;
    static const float KT2MPS;
    static const float INHG2KPA;
    static const float FPS2CMPS;
    static const float DEG2RAD;
    static const float RAD2DEG;

    QProcess* simProcess;
    QTime* time;
    QUdpSocket* inSocket;//(new QUdpSocket());
    QUdpSocket* outSocket;

    ActuatorCommand* actCommand;
    ActuatorDesired* actDesired;
    ManualControlCommand* manCtrlCommand;
    FlightStatus* flightStatus;
    BaroAltitude* baroAlt;
    BaroAirspeed* baroAirspeed;
    AttitudeActual* attActual;
    VelocityActual* velActual;
    GPSPosition* gpsPos;
    GPSVelocity* gpsVel;
    PositionActual* posActual;
    HomeLocation* posHome;
    Accels* accels;
    Gyros*  gyros;
    GCSTelemetryStats* telStats;

    SimulatorSettings settings;

    FLIGHT_PARAM current;
    FLIGHT_PARAM old;
    QMutex lock;

private:
    bool once;
    float initN;
    float initE;
    float initD;

    int updatePeriod;
    int simTimeout;
    volatile bool autopilotConnectionStatus;
    volatile bool simConnectionStatus;
    QTimer* txTimer;
    QTimer* simTimer;
    QString name;
    QString simulatorId;
    volatile static bool isStarted;
    static QStringList instances;
    //QList<QScopedPointer<UAVDataObject> > requiredUAVObjects;
    void setupOutputObject(UAVObject* obj, int updatePeriod);
    void setupInputObject(UAVObject* obj, int updatePeriod);
    void setupObjects();
};



class SimulatorCreator
{
public:
    SimulatorCreator(QString id, QString descr) :
        classId(id),
        description(descr)
    {}
    virtual ~SimulatorCreator() {}

    QString ClassId() const {return classId;}
    QString Description() const {return description;}

    virtual Simulator* createSimulator(const SimulatorSettings& params) = 0;

private:
    QString classId;
    QString description;
};

#endif // ISIMULATOR_H
