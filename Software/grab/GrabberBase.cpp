/*
 * GrabberBase.cpp
 *
 *  Created on: 18.07.2012
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

#include "GrabberBase.hpp"
#include "../src/debug.h"

int validCoord(int a) {
    const unsigned int neg = (1 << 15);
    if (a & neg)
        a = ((~a + 1) & 0x0000ffff) * -1;
    return a;
}

inline QRect & getValidRect(QRect & rect) {
    int x1,x2,y1,y2;
    rect.getCoords(&x1, &y1, &x2, &y2);
    x1 = validCoord(x1);
    y1 = validCoord(y1);
    rect.setCoords(x1, y1, x1 + rect.width() - 1, y1 + rect.height() - 1);
    return rect;
}

GrabberBase::GrabberBase(QObject *parent, GrabberContext *grabberContext) : QObject(parent) {
    _context = grabberContext;
    initGPU();
}

void GrabberBase::initGPU()
{
    std::vector<cl::Platform> platforms;
    cl::Platform::get(&platforms);

    for (unsigned int iPlatform=0; iPlatform<platforms.size(); iPlatform++)
    {
        std::vector<cl::Device> devices;
        platforms[iPlatform].getDevices(CL_DEVICE_TYPE_GPU, &devices);
        for (unsigned int iDevice=0; iDevice<devices.size(); iDevice++) {
            qDebug() << "Device GPU: " << devices[iDevice].getInfo<CL_DEVICE_NAME>().data();
        }
    }
}

const GrabbedScreen * GrabberBase::screenOfRect(const QRect &rect) const {
    QPoint center = rect.center();
    for (int i = 0; i < _screensWithWidgets.size(); ++i) {
        if (_screensWithWidgets[i].screenInfo.rect.contains(center))
            return &_screensWithWidgets[i];
    }
    for (int i = 0; i < _screensWithWidgets.size(); ++i) {
        if (_screensWithWidgets[i].screenInfo.rect.intersects(rect))
            return &_screensWithWidgets[i];
    }
    return NULL;
}

bool GrabberBase::isReallocationNeeded(const QList< ScreenInfo > &screensWithWidgets) const  {
    if (_screensWithWidgets.size() == 0 || screensWithWidgets.size() != _screensWithWidgets.size())
        return true;

    for (int i = 0; i < screensWithWidgets.size(); ++i) {
        if (screensWithWidgets[i].rect != _screensWithWidgets[i].screenInfo.rect)
            return true;
    }
    return false;
}

//NG: send data to calculate on GPU
QRgb GrabberBase::GPU_calculateAvgColor(QRgb *result, const unsigned char *buffer, unsigned int pitch, const QRect &rect)
{
    int count = 0; // count the amount of pixels taken into account
    unsigned int red = 0, green = 0, blue = 0;

    count = GPU_accumulateBufferFormatArgb(buffer, pitch, rect.x(), rect.y(), rect.height(), rect.width(), &red, &green, &blue);

    if ( count > 1 ) {
        red   = ( red   / count) & 0xff;
        green = ( green / count) & 0xff;
        blue  = ( blue  / count) & 0xff;
    }

    *result = qRgb(red, green, blue);
    return *result;
}

//NG: for translate to CL file
int GrabberBase::GPU_accumulateBufferFormatArgb(const unsigned char *buffer, unsigned int pitch,
                                                const int x, const int y, const int height, const int width,
                                                unsigned int *red, unsigned int *green, unsigned int *blue)
{
    unsigned int r=0, g=0, b=0;
    const char bytesPerPixel = 4;
    int count = 0; // count the amount of pixels taken into account
    for(int currentY = 0; currentY < height; currentY++) {
        int index = pitch * (y+currentY) + x*bytesPerPixel;
        for(int currentX = 0; currentX < width; currentX += 4) {
            b += buffer[index]   + buffer[index + 4] + buffer[index + 8 ] + buffer[index + 12];
            g += buffer[index+1] + buffer[index + 5] + buffer[index + 9 ] + buffer[index + 13];
            r += buffer[index+2] + buffer[index + 6] + buffer[index + 10] + buffer[index + 14];
            count += 4;
            index += bytesPerPixel * 4;
        }
    }

    red = &r;
    green = &g;
    blue = &b;
    return count;
}

void GrabberBase::GPU_grab()
{
    DEBUG_MID_LEVEL << Q_FUNC_INFO << this->metaObject()->className();
    QList< ScreenInfo > screens2Grab;
    screens2Grab.reserve(5);
    screensWithWidgets(&screens2Grab, *_context->grabWidgets);
    if (isReallocationNeeded(screens2Grab)) {
        if (!reallocate(screens2Grab)) {
            qCritical() << Q_FUNC_INFO << " couldn't reallocate grabbing buffer";
            emit frameGrabAttempted(GrabResultError);
            return;
        }
    }
    _lastGrabResult = grabScreens();
    if (_lastGrabResult == GrabResultOk) {
        _context->grabResult->clear();

        const int grabWidgetsSize = _context->grabWidgets->size();   //count of grabs area
        for (int i = 0; i < _context->grabWidgets->size(); ++i) {
            const int currentWidgetIndex = i;
            QRect widgetRect = _context->grabWidgets->at(i)->frameGeometry();
            getValidRect(widgetRect);

            const GrabbedScreen *grabbedScreen = screenOfRect(widgetRect);
            if (grabbedScreen == NULL) {
                DEBUG_HIGH_LEVEL << Q_FUNC_INFO << " widget is out of screen " << Debug::toString(widgetRect);
                _context->grabResult->append(0);
                continue;
            }
            DEBUG_HIGH_LEVEL << Q_FUNC_INFO << Debug::toString(widgetRect);
            QRect monitorRect = grabbedScreen->screenInfo.rect;

            QRect clippedRect = monitorRect.intersected(widgetRect);

            // Checking for the 'grabme' widget position inside the monitor that is used to capture color
            if( !clippedRect.isValid() ){

                DEBUG_MID_LEVEL << "Widget 'grabme' is out of screen:" << Debug::toString(clippedRect);

                _context->grabResult->append(qRgb(0,0,0));
                continue;
            }

            // Convert coordinates from "Main" desktop coord-system to capture-monitor coord-system
            QRect preparedRect = clippedRect.translated(-monitorRect.x(), -monitorRect.y());

            // Align width by 4 for accelerated calculations
            preparedRect.setWidth(preparedRect.width() - (preparedRect.width() % 4));

            if( !preparedRect.isValid() ){
                qWarning() << Q_FUNC_INFO << " preparedRect is not valid:" << Debug::toString(preparedRect);
                // width and height can't be negative

                _context->grabResult->append(qRgb(0,0,0));
                continue;
            }

            using namespace Grab;
            const int bytesPerPixel = 4;
            QRgb avgColor = 0;
            if (_context->grabWidgets->at(i)->isAreaEnabled()) {
//                Calculations::calculateAvgColor(&avgColor, grabbedScreen->imgData, grabbedScreen->imgFormat, grabbedScreen->screenInfo.rect.width() * bytesPerPixel, preparedRect );
                //NG: use GPU_calculateAvgColor
                GPU_calculateAvgColor(&avgColor, grabbedScreen->imgData, grabbedScreen->screenInfo.rect.width() * bytesPerPixel, preparedRect);
                _context->grabResult->append(avgColor);
            } else {
                _context->grabResult->append(qRgb(0,0,0));
            }
        }

    }
    emit frameGrabAttempted(_lastGrabResult);
}

void GrabberBase::grab() {
    DEBUG_MID_LEVEL << Q_FUNC_INFO << this->metaObject()->className();
    QList< ScreenInfo > screens2Grab;
    screens2Grab.reserve(5);
    screensWithWidgets(&screens2Grab, *_context->grabWidgets);
    if (isReallocationNeeded(screens2Grab)) {
        if (!reallocate(screens2Grab)) {
            qCritical() << Q_FUNC_INFO << " couldn't reallocate grabbing buffer";
            emit frameGrabAttempted(GrabResultError);
            return;
        }
    }
    _lastGrabResult = grabScreens();
    if (_lastGrabResult == GrabResultOk) {
        _context->grabResult->clear();

        for (int i = 0; i < _context->grabWidgets->size(); ++i) {
            QRect widgetRect = _context->grabWidgets->at(i)->frameGeometry();
            getValidRect(widgetRect);

            const GrabbedScreen *grabbedScreen = screenOfRect(widgetRect);
            if (grabbedScreen == NULL) {
                DEBUG_HIGH_LEVEL << Q_FUNC_INFO << " widget is out of screen " << Debug::toString(widgetRect);
                _context->grabResult->append(0);
                continue;
            }
            DEBUG_HIGH_LEVEL << Q_FUNC_INFO << Debug::toString(widgetRect);
            QRect monitorRect = grabbedScreen->screenInfo.rect;

            QRect clippedRect = monitorRect.intersected(widgetRect);

            // Checking for the 'grabme' widget position inside the monitor that is used to capture color
            if( !clippedRect.isValid() ){

                DEBUG_MID_LEVEL << "Widget 'grabme' is out of screen:" << Debug::toString(clippedRect);

                _context->grabResult->append(qRgb(0,0,0));
                continue;
            }

            // Convert coordinates from "Main" desktop coord-system to capture-monitor coord-system
            QRect preparedRect = clippedRect.translated(-monitorRect.x(), -monitorRect.y());

            // Align width by 4 for accelerated calculations
            preparedRect.setWidth(preparedRect.width() - (preparedRect.width() % 4));

            if( !preparedRect.isValid() ){
                qWarning() << Q_FUNC_INFO << " preparedRect is not valid:" << Debug::toString(preparedRect);
                // width and height can't be negative

                _context->grabResult->append(qRgb(0,0,0));
                continue;
            }

            using namespace Grab;
            const int bytesPerPixel = 4;
            QRgb avgColor;
            if (_context->grabWidgets->at(i)->isAreaEnabled()) {
                Calculations::calculateAvgColor(&avgColor, grabbedScreen->imgData, grabbedScreen->imgFormat, grabbedScreen->screenInfo.rect.width() * bytesPerPixel, preparedRect );
                _context->grabResult->append(avgColor);
            } else {
                _context->grabResult->append(qRgb(0,0,0));
            }
        }

    }
    emit frameGrabAttempted(_lastGrabResult);
}
