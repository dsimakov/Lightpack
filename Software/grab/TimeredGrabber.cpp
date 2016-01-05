/*
 * TimeredGrabber.cpp
 *
 *  Created on: 27.08.2012
 *     Project: Lightpack
 *
 *  Copyright (c) 2012 Timur Sattarov
 *
 *  Lightpack a USB content-driving ambient lighting system
 *
 *  Lightpack is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  Lightpack is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */


#include "TimeredGrabber.hpp"

#include <QTimer>

TimeredGrabber::TimeredGrabber(QObject * parent, GrabberContext *context)
    : GrabberBase(parent, context) {
    if (m_timer && m_timer->isActive())
        m_timer->stop();
    m_timer.reset(new QTimer(this));

    //NG: connect signal for GPU_grab()
    connect(m_timer.data(), SIGNAL(timeout()), this, SLOT(GPU_grab()));
//    connect(m_timer.data(), SIGNAL(timeout()), this, SLOT(grab()));
}

void TimeredGrabber::setGrabInterval(int msec) {
    DEBUG_LOW_LEVEL << Q_FUNC_INFO <<  this->metaObject()->className();
    m_timer->setInterval(msec);
}
void TimeredGrabber::startGrabbing() {
    DEBUG_LOW_LEVEL << Q_FUNC_INFO << this->metaObject()->className();
    m_timer->start();
}
void TimeredGrabber::stopGrabbing() {
    DEBUG_LOW_LEVEL << Q_FUNC_INFO << this->metaObject()->className();
    m_timer->stop();
}

bool TimeredGrabber::isGrabbingStarted() const {
    DEBUG_LOW_LEVEL << Q_FUNC_INFO << this->metaObject()->className();
    return m_timer->isActive();
}
