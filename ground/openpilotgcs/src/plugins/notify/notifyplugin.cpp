/**
 ******************************************************************************
 *
 * @file       notifyplugin.cpp
 * @author     The OpenPilot Team, http://www.openpilot.org Copyright (C) 2010.
 * @brief      
 * @see        The GNU Public License (GPL) Version 3
 * @defgroup   notifyplugin
 * @{
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

#include "notifyplugin.h"
#include "notificationitem.h"
#include "notifypluginoptionspage.h"
#include "notifylogging.h"
#include <coreplugin/icore.h>
#include <QDebug>
#include <QtPlugin>
#include <QStringList>

#include <extensionsystem/pluginmanager.h>

#include <iostream>
#include "qxttimer.h"
#include "backendcapabilities.h"

static const QString VERSION = "1.0.0";

//#define DEBUG_NOTIFIES


SoundNotifyPlugin::SoundNotifyPlugin():
    _audioWrapper(0),
    _audioThread(0)
{
}

SoundNotifyPlugin::~SoundNotifyPlugin()
{
    Core::ICore::instance()->saveSettings(this);
    if (_audioWrapper != NULL)
    {
        delete _audioWrapper;
    }

    if(_audioThread != NULL)
    {
        _audioThread->exit();
        delete _audioThread;
    }
}

bool SoundNotifyPlugin::initialize(const QStringList& args, QString *errMsg)
{ 
   Q_UNUSED(args); 
   Q_UNUSED(errMsg); 

   mop = new NotifyPluginOptionsPage(this);
   addAutoReleasedObject(mop);

   return true; 
} 

void SoundNotifyPlugin::extensionsInitialized()
{ 
    Core::ICore::instance()->readSettings(this);

    ExtensionSystem::PluginManager* pm = ExtensionSystem::PluginManager::instance();
    connect(pm, SIGNAL(objectAdded(QObject*)), this, SLOT(onTelemetryManagerAdded(QObject*)));
    _toRemoveNotifications.clear();
    connectNotifications();
} 

void SoundNotifyPlugin::saveConfig( QSettings* settings, UAVConfigInfo *configInfo){

    configInfo->setVersion(VERSION);

    settings->beginWriteArray("Current");
    settings->setArrayIndex(0);
    currentNotification.saveState(settings);
    settings->endArray();

    settings->beginGroup("listNotifies");
    settings->remove("");
    settings->endGroup();

    settings->beginWriteArray("listNotifies");
    for (int i = 0; i < _notificationList.size(); i++) {
        settings->setArrayIndex(i);
        _notificationList.at(i)->saveState(settings);
    }
    settings->endArray();
    settings->setValue(QLatin1String("EnableSound"), enableSound);

}

void SoundNotifyPlugin::readConfig( QSettings* settings, UAVConfigInfo *configInfo)
{
    // Just for migration to the new format.
    //Q_ASSERT(configInfo->version() == UAVConfigVersion());

    settings->beginReadArray("Current");
    settings->setArrayIndex(0);
    currentNotification.restoreState(settings);
    settings->endArray();

    // read list of notifications from settings
    int size = settings->beginReadArray("listNotifies");
    for (int i = 0; i < size; ++i) {
        settings->setArrayIndex(i);
        NotificationItem* notification = new NotificationItem;
        notification->restoreState(settings);
        _notificationList.append(notification);
    }
    settings->endArray();
    setEnableSound(settings->value(QLatin1String("EnableSound"),0).toBool());
}

void SoundNotifyPlugin::onTelemetryManagerAdded(QObject* obj)
{
    telMngr = qobject_cast<TelemetryManager *>(obj);
    if (telMngr)
        connect(telMngr, SIGNAL(disconnected()), this, SLOT(onAutopilotDisconnect()));
}

void SoundNotifyPlugin::shutdown()
{ 
   // Do nothing 
}

void SoundNotifyPlugin::onAutopilotDisconnect()
{
    connectNotifications();
}

/*!
    clear any notify timers from previous flight;
    reset will be perform on start of option page
 */
void SoundNotifyPlugin::resetNotification(void)
{
    //first, reject empty args and unknown fields.
    foreach(NotificationItem* ntf, _notificationList) {
        ntf->disposeTimer();
        disconnect(ntf->getTimer(), SIGNAL(timeout()), this, SLOT(on_timerRepeated_Notification()));
        ntf->disposeExpireTimer();
        disconnect(ntf->getExpireTimer(), SIGNAL(timeout()), this, SLOT(on_timerRepeated_Notification()));
    }
}

/*!
    update list of notifies;
    will be perform on OK or APPLY press of option page
 */
void SoundNotifyPlugin::updateNotificationList(QList<NotificationItem*> list)
{
    _toRemoveNotifications.clear();
    resetNotification();
    _notificationList.clear();
    _notificationList=list;
    connectNotifications();

    Core::ICore::instance()->saveSettings(this);
}

void SoundNotifyPlugin::connectNotifications()
{
    foreach(UAVDataObject* obj,lstNotifiedUAVObjects) {
        if (obj != NULL)
            disconnect(obj,SIGNAL(objectUpdated(UAVObject*)),this,SLOT(on_arrived_Notification(UAVObject*)));
    }
    if (!enableSound) return;

    ExtensionSystem::PluginManager *pm = ExtensionSystem::PluginManager::instance();
    UAVObjectManager *objManager = pm->getObject<UAVObjectManager>();

    lstNotifiedUAVObjects.clear();
    _pendingNotifications.clear();
    _notificationList.append(_toRemoveNotifications);
    _toRemoveNotifications.clear();

    //first, reject empty args and unknown fields.
    foreach(NotificationItem* notify, _notificationList) {
        notify->_isPlayed = false;
        notify->isNowPlaying=false;

        if(notify->mute()) continue;
        // check is all sounds presented for notification,
        // if not - we must not subscribe to it at all
        if(notify->toSoundList().isEmpty()) continue;

        UAVDataObject* obj = dynamic_cast<UAVDataObject*>( objManager->getObject(notify->getDataObject()) );
        if (obj != NULL ) {
            if (!lstNotifiedUAVObjects.contains(obj)) {
                lstNotifiedUAVObjects.append(obj);

                connect(obj, SIGNAL(objectUpdated(UAVObject*)), this, SLOT(on_arrived_Notification(UAVObject*)));
            }
        } else {
            qNotifyDebug() << "Error: Object is unknown (" << notify->getDataObject() << ").";
        }
    }

    if (_notificationList.isEmpty()) return;

    if(_audioWrapper != NULL)
    {
        delete _audioWrapper;
    }
    if(_audioThread != NULL)
    {
        _audioThread->exit();
        delete _audioThread;
    }
    _audioWrapper = new NotifyAudioWrapper();
    _audioWrapper->initialise();
    connect(_audioWrapper, SIGNAL(stateChanged(Phonon::State,Phonon::State)),
        this, SLOT(stateChanged(Phonon::State,Phonon::State)));
    connect(this, SIGNAL(playAudio(QString)),
            _audioWrapper, SLOT(play(QString)));

    _audioThread = new QThread();
    _audioWrapper->moveToThread(_audioThread);
    _audioThread->start();
}

void SoundNotifyPlugin::on_arrived_Notification(UAVObject *object)
{
    foreach(NotificationItem* ntf, _notificationList) {
        if (object->getName() != ntf->getDataObject())
            continue;


        // skip duplicate notifications
        if (_nowPlayingNotification == ntf)
            continue;

        // skip periodical notifications
        // this condition accepts:
        // 1. Periodical notifications played firstly;
        //    NOTE: At first time it will be played, then it played only by timer,
        //          when conditions became false firstStart flag has been cleared and
        //          notification can be accepted again;
        // 2. Once time notifications, they removed immediately after first playing;
        // 3. Instant notifications(played one by one without interval);
        if (ntf->retryValue() != NotificationItem::eInstantly &&
                ntf->retryValue() != NotificationItem::eOnce && ntf->_isPlayed)
           continue;

        qNotifyDebug() << QString("new notification: | %1 | %2 | val1: %3 | val2: %4")
                                      .arg(ntf->getDataObject())
                                        .arg(ntf->getObjectField())
                                        .arg(ntf->singleValue().toString())
                                        .arg(ntf->valueRange2());

        QString fieldName = ntf->getObjectField();
        UAVObjectField* field = object->getField(fieldName);

        qNotifyDebug() << QString("UAV object: %1 | value: %2")
                                      .arg(object->getName())
                                      .arg(field->getValue().toString());

        checkNotificationRule(ntf, object);
    }
    connect(object, SIGNAL(objectUpdated(UAVObject*)),
            this, SLOT(on_arrived_Notification(UAVObject*)), Qt::UniqueConnection);
}


void SoundNotifyPlugin::on_timerRepeated_Notification()
{
    NotificationItem* notification = static_cast<NotificationItem*>(sender()->parent());
    if (!notification)
        return;
    // skip duplicate notifications
    // WARNING: generally we shoudn't ever trap here
    //          this means, that timer fires to early and notification overlap itself
    if (_nowPlayingNotification == notification) {
        qNotifyDebug() << "WARN: on_timerRepeated - notification was skipped!";
        notification->restartTimer();
        return;
    }

    qNotifyDebug() << QString("repeatTimer: %1% | %2 | %3").arg(notification->getDataObject())
                                                    .arg(notification->getObjectField())
                                                    .arg(notification->toString());

    ExtensionSystem::PluginManager *pm = ExtensionSystem::PluginManager::instance();
    UAVObjectManager *objManager = pm->getObject<UAVObjectManager>();
    UAVObject* object = objManager->getObject(notification->getDataObject());
    if (object)
        checkNotificationRule(notification,object);
}


void SoundNotifyPlugin::on_expiredTimer_Notification()
{
    // fire expire timer
    NotificationItem* notification = static_cast<NotificationItem*>(sender()->parent());
    if(!notification)
        return;
    notification->stopExpireTimer();

    volatile QMutexLocker lock(&_mutex);

    if (!_pendingNotifications.isEmpty()) {
        qNotifyDebug() << QString("expireTimer: %1% | %2 | %3").arg(notification->getDataObject())
                                                        .arg(notification->getObjectField())
                                                        .arg(notification->toString());

        _pendingNotifications.removeOne(notification);
    }
}

void SoundNotifyPlugin::stateChanged(Phonon::State newstate, Phonon::State oldstate)
{
    Q_UNUSED(oldstate)

    //qNotifyDebug() << "File length (ms): " << phonon.mo->totalTime();

//#ifndef Q_OS_WIN
//    // This is a hack to force Linux to wait until the end of the
//    // wav file before moving to the next in the queue.
//    // I wish I did not have to go through a #define, but I did not
//    // manage to make this work on both platforms any other way!
//    if (phonon.mo->totalTime()>0)
//        phonon.mo->setTransitionTime(phonon.mo->totalTime());
//#endif
    if ((newstate  == Phonon::PausedState) ||
       (newstate  == Phonon::StoppedState))
    {
        qNotifyDebug() << "New State: " << QVariant(newstate).toString();

        volatile QMutexLocker lock(&_mutex);
        // assignment to NULL needed to detect that palying is finished
        // it's useful in repeat timer handler, where we can detect
        // that notification has not overlap with itself
        _nowPlayingNotification = NULL;

        if (!_pendingNotifications.isEmpty())
        {
            NotificationItem* notification = _pendingNotifications.takeFirst();
            qNotifyDebug_if(notification) << "play audioFree - " << notification->toString();
            playNotification(notification);
        }
    } /*else {
        if (newstate  == Phonon::ErrorState) {
            if (phonon.mo->errorType()==0) {
                qDebug() << "Phonon::ErrorState: ErrorType = " << phonon.mo->errorType();
                phonon.mo->clear();
            }
        }
    }*/
}

bool checkRange(QString fieldValue, QString enumValue, QStringList values, NotificationItem::ERange direction)
{

    bool ret = false;
    switch(direction)
    {
    case NotificationItem::eEqualTo:
        ret = !QString::compare(enumValue, fieldValue, Qt::CaseInsensitive) ? true : false;
        break;

    default:
        ret = true;
        break;
    };
    return ret;
}

bool checkRange(double fieldValue, double min, double max, NotificationItem::ERange direction)
{
    bool ret = false;
    switch(direction)
    {
    case NotificationItem::eEqualTo:
        ret = (fieldValue == min);
        break;

    case NotificationItem::eGreaterThan:
        ret = (fieldValue > min);
        break;

    case NotificationItem::eLessThan:
        ret = (fieldValue < min);
        break;

    default:
        Q_ASSERT(min < max);
        ret = (fieldValue > min) && (fieldValue < max);
        break;
    };

    return ret;
}

void SoundNotifyPlugin::checkNotificationRule(NotificationItem* notification, UAVObject* object)
{
    bool condition=false;

    if (notification->mute())
        return;

    NotificationItem::ERange direction = notification->range();
    QString fieldName = notification->getObjectField();
    UAVObjectField* field = object->getField(fieldName);

    if (field->getName().isEmpty())
        return;

    QVariant value = field->getValue();

    // Only trigger if we haven't already triggered for this value. As notifications
    // occur on a UAVObject by UAVObject basis rather than individual fields, we would
    // trigger notifications for multiple fields in an object if they happen to still
    // match a trigger value the next time round
    if(value != notification->previousTriggerValue())
    {
        notification->setPreviousTriggerValue(value);
        if(UAVObjectField::ENUM == field->getType()) {
            condition = checkRange(value.toString(),
                                   notification->singleValue().toString(),
                                   field->getOptions(),
                                   direction);
        } else {
            condition = checkRange(value.toDouble(),
                                   notification->singleValue().toDouble(),
                                   notification->valueRange2(),
                                   direction);
        }
    }

    notification->_isPlayed = condition;
    // if condition has been changed, and already in false state
    // we should reset _isPlayed flag and stop repeat timer
    if (!notification->_isPlayed) {
        notification->stopTimer();
        return;
    }

    volatile QMutexLocker lock(&_mutex);

    if (!playNotification(notification)) {
        if (!_pendingNotifications.contains(notification)
                && (_nowPlayingNotification != notification)) {
            notification->stopTimer();

            qNotifyDebug() << "add to pending list - " << notification->toString();
            // if audio is busy, start expiration timer
            //ms = (notification->getExpiredTimeout()[in sec])*1000
            //QxtTimer::singleShot(notification->getExpireTimeout()*1000, this, SLOT(expirationTimerHandler(NotificationItem*)), qVariantFromValue(notification));
            _pendingNotifications.append(notification);
            notification->startExpireTimer();
            connect(notification->getExpireTimer(), SIGNAL(timeout()),
                    this, SLOT(on_expiredTimer_Notification()), Qt::UniqueConnection);
        }
    }
}

bool SoundNotifyPlugin::playNotification(NotificationItem* notification)
{
    if(!notification)
        return false;

    // Check: race condition, if phonon.mo got deleted don't go further
    if (_audioWrapper == NULL)
        return false;

    //qNotifyDebug() << "Phonon State: " << phonon.mo->state();

    if (_audioWrapper->readyToPlay())
    {
        _nowPlayingNotification = notification;
        notification->stopExpireTimer();

        if (notification->retryValue() == NotificationItem::eOnce) {
            _toRemoveNotifications.append(_notificationList.takeAt(_notificationList.indexOf(notification)));
        } else {
            if (notification->retryValue() != NotificationItem::eInstantly) {
                int timer_value;

                // needs to correct repeat timer value,
                // acording to message play duration,
                // we don't measure duration of each message,
                // simply take average duration
                enum { eAverageDurationSec = 8 };

                enum { eSecToMsec = 1000 };

                timer_value = ((int)notification->retryValue() + eAverageDurationSec) * eSecToMsec;


                notification->startTimer(timer_value);
                connect(notification->getTimer(), SIGNAL(timeout()),
                        this, SLOT(on_timerRepeated_Notification()), Qt::UniqueConnection);
            }
        }
        qNotifyDebug() << "play: " << notification->toString();
        QString soundList = notification->toSoundList().join("|");
        emit playAudio(soundList);
        return true;

    }

    return false;
}

Q_EXPORT_PLUGIN(SoundNotifyPlugin)
