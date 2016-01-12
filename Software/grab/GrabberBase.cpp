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

const char *getClErrorString(cl_int error)
{
switch(error){
    // run-time and JIT compiler errors
    case 0: return "CL_SUCCESS";
    case -1: return "CL_DEVICE_NOT_FOUND";
    case -2: return "CL_DEVICE_NOT_AVAILABLE";
    case -3: return "CL_COMPILER_NOT_AVAILABLE";
    case -4: return "CL_MEM_OBJECT_ALLOCATION_FAILURE";
    case -5: return "CL_OUT_OF_RESOURCES";
    case -6: return "CL_OUT_OF_HOST_MEMORY";
    case -7: return "CL_PROFILING_INFO_NOT_AVAILABLE";
    case -8: return "CL_MEM_COPY_OVERLAP";
    case -9: return "CL_IMAGE_FORMAT_MISMATCH";
    case -10: return "CL_IMAGE_FORMAT_NOT_SUPPORTED";
    case -11: return "CL_BUILD_PROGRAM_FAILURE";
    case -12: return "CL_MAP_FAILURE";
    case -13: return "CL_MISALIGNED_SUB_BUFFER_OFFSET";
    case -14: return "CL_EXEC_STATUS_ERROR_FOR_EVENTS_IN_WAIT_LIST";
    case -15: return "CL_COMPILE_PROGRAM_FAILURE";
    case -16: return "CL_LINKER_NOT_AVAILABLE";
    case -17: return "CL_LINK_PROGRAM_FAILURE";
    case -18: return "CL_DEVICE_PARTITION_FAILED";
    case -19: return "CL_KERNEL_ARG_INFO_NOT_AVAILABLE";

    // compile-time errors
    case -30: return "CL_INVALID_VALUE";
    case -31: return "CL_INVALID_DEVICE_TYPE";
    case -32: return "CL_INVALID_PLATFORM";
    case -33: return "CL_INVALID_DEVICE";
    case -34: return "CL_INVALID_CONTEXT";
    case -35: return "CL_INVALID_QUEUE_PROPERTIES";
    case -36: return "CL_INVALID_COMMAND_QUEUE";
    case -37: return "CL_INVALID_HOST_PTR";
    case -38: return "CL_INVALID_MEM_OBJECT";
    case -39: return "CL_INVALID_IMAGE_FORMAT_DESCRIPTOR";
    case -40: return "CL_INVALID_IMAGE_SIZE";
    case -41: return "CL_INVALID_SAMPLER";
    case -42: return "CL_INVALID_BINARY";
    case -43: return "CL_INVALID_BUILD_OPTIONS";
    case -44: return "CL_INVALID_PROGRAM";
    case -45: return "CL_INVALID_PROGRAM_EXECUTABLE";
    case -46: return "CL_INVALID_KERNEL_NAME";
    case -47: return "CL_INVALID_KERNEL_DEFINITION";
    case -48: return "CL_INVALID_KERNEL";
    case -49: return "CL_INVALID_ARG_INDEX";
    case -50: return "CL_INVALID_ARG_VALUE";
    case -51: return "CL_INVALID_ARG_SIZE";
    case -52: return "CL_INVALID_KERNEL_ARGS";
    case -53: return "CL_INVALID_WORK_DIMENSION";
    case -54: return "CL_INVALID_WORK_GROUP_SIZE";
    case -55: return "CL_INVALID_WORK_ITEM_SIZE";
    case -56: return "CL_INVALID_GLOBAL_OFFSET";
    case -57: return "CL_INVALID_EVENT_WAIT_LIST";
    case -58: return "CL_INVALID_EVENT";
    case -59: return "CL_INVALID_OPERATION";
    case -60: return "CL_INVALID_GL_OBJECT";
    case -61: return "CL_INVALID_BUFFER_SIZE";
    case -62: return "CL_INVALID_MIP_LEVEL";
    case -63: return "CL_INVALID_GLOBAL_WORK_SIZE";
    case -64: return "CL_INVALID_PROPERTY";
    case -65: return "CL_INVALID_IMAGE_DESCRIPTOR";
    case -66: return "CL_INVALID_COMPILER_OPTIONS";
    case -67: return "CL_INVALID_LINKER_OPTIONS";
    case -68: return "CL_INVALID_DEVICE_PARTITION_COUNT";

    // extension errors
    case -1000: return "CL_INVALID_GL_SHAREGROUP_REFERENCE_KHR";
    case -1001: return "CL_PLATFORM_NOT_FOUND_KHR";
    case -1002: return "CL_INVALID_D3D10_DEVICE_KHR";
    case -1003: return "CL_INVALID_D3D10_RESOURCE_KHR";
    case -1004: return "CL_D3D10_RESOURCE_ALREADY_ACQUIRED_KHR";
    case -1005: return "CL_D3D10_RESOURCE_NOT_ACQUIRED_KHR";
    default: return "Unknown OpenCL error";
    }
}

inline void checkErr(cl_int err, const char * name) {
    if (err != CL_SUCCESS) {
      std::cerr << "ERROR: " << name  << " (" << err << ") "  << getClErrorString(err) << std::endl;
      exit(EXIT_FAILURE);
   }
}

void GrabberBase::initGPU()
{
    cl::Platform::get(&platforms);
    cl_int err;

    for (unsigned int iPlatform=0; iPlatform<platforms.size(); iPlatform++)
    {
        std::vector<cl::Device> devices;
        platforms[iPlatform].getDevices(CL_DEVICE_TYPE_GPU, &devices);
        for (unsigned int iDevice=0; iDevice<devices.size(); iDevice++) {
            device = devices[iDevice];
            qDebug() << "Device GPU: " << device.getInfo<CL_DEVICE_NAME>().data();
        }
    }

    contextDevices.push_back(device);
    context = cl::Context(contextDevices);
    queue = cl::CommandQueue(context, device, err);
    checkErr(err, "CommandQueue(context, device, err)");

    //Load OpenCL source code
    std::ifstream sourceFile("/home/family/workspace/github/Lightpack/Software/src/avg.cl");
    std::string sourceCode(std::istreambuf_iterator<char>(sourceFile),(std::istreambuf_iterator<char>()));

    //Build OpenCL program and make the kernel
    cl::Program::Sources source(1, std::make_pair(sourceCode.c_str(), sourceCode.length()+1));
    program = cl::Program(context, source, &err);
    checkErr(err, "Program(context, source, &err)");
    err = program.build(contextDevices);
    checkErr(err, "Program::build()");

    kernel = cl::Kernel(program, "avgcalc", &err);
    checkErr(err, "Kernel::Kernel()");

    GPU_DATA_COUNT = _context->grabWidgets->size();
    if (!GPU_DATA_COUNT)
        GPU_DATA_COUNT = 10;
    GPU_BLOCK_SIZE = 400*400;   //max size grab widget

    pSource = new cl_char[GPU_DATA_COUNT*GPU_BLOCK_SIZE];
    pWidth  = new int[GPU_DATA_COUNT*sizeof(cl_int)];
    pHeight = new int[GPU_DATA_COUNT*sizeof(cl_int)];
    pX = new int[GPU_DATA_COUNT*sizeof(cl_int)];
    pY = new int[GPU_DATA_COUNT*sizeof(cl_int)];
    pPitch = new int[GPU_DATA_COUNT*sizeof(cl_int)];

    pRed   = new int[GPU_DATA_COUNT*sizeof(cl_int)];
    pGreen = new int[GPU_DATA_COUNT*sizeof(cl_int)];
    pBlue  = new int[GPU_DATA_COUNT*sizeof(cl_int)];
    pCount = new int[GPU_DATA_COUNT*sizeof(cl_int)];

    memset(pSource, 0, GPU_DATA_COUNT*GPU_BLOCK_SIZE*sizeof(cl_char));
    memset(pWidth, 0, GPU_DATA_COUNT*sizeof(cl_int));
    memset(pHeight, 0, GPU_DATA_COUNT*sizeof(cl_int));
    memset(pX, 0, GPU_DATA_COUNT*sizeof(cl_int));
    memset(pY, 0, GPU_DATA_COUNT*sizeof(cl_int));
    memset(pPitch, 0, GPU_DATA_COUNT*sizeof(cl_int));
    memset(pRed, 0, GPU_DATA_COUNT*sizeof(cl_int));
    memset(pGreen, 0, GPU_DATA_COUNT*sizeof(cl_int));
    memset(pBlue, 0, GPU_DATA_COUNT*sizeof(cl_int));
    memset(pCount, 0, GPU_DATA_COUNT*sizeof(cl_int));

//    exit(0);
//    //Set arguments to kernel
//    cl::Buffer clmInputVector1 = cl::Buffer(context, CL_MEM_READ_ONLY|CL_MEM_COPY_HOST_PTR,  GPU_DATA_COUNT * sizeof(float), pInputVector1);
//    cl::Buffer clmInputVector2 = cl::Buffer(context, CL_MEM_READ_ONLY|CL_MEM_COPY_HOST_PTR,  GPU_DATA_COUNT * sizeof(float), pInputVector2);
//    cl::Buffer clmOutputVector = cl::Buffer(context, CL_MEM_READ_WRITE|CL_MEM_COPY_HOST_PTR, GPU_DATA_COUNT * sizeof(float), pOutputVector);

//    int iArg = 0;
//    kernel.setArg(iArg++, clmInputVector1);
//    kernel.setArg(iArg++, clmInputVector2);
//    kernel.setArg(iArg++, clmOutputVector);
//    kernel.setArg(iArg++, GPU_DATA_COUNT);

//    queue.enqueueNDRangeKernel(kernel, cl::NullRange, cl::NDRange(GPU_DATA_COUNT), cl::NDRange(128));
//    queue.finish();

    // Read buffer C into a local list
//    queue.enqueueReadBuffer(clmOutputVector, CL_TRUE, 0, GPU_DATA_COUNT * sizeof(float), pOutputVector);

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

        //count of grabs area
        const int grabWidgetsSize = _context->grabWidgets->size();

        //array for returned colors

        int offset = 0;
//        for (int i = 0; i < _context->grabWidgets->size(); ++i) {
        for (int i = 0; i < GPU_DATA_COUNT; ++i) {
            const int currentWidgetIndex = i;

//            if (!_context->grabWidgets->at(i)->isAreaEnabled()) {
//                _context->grabResult->append(qRgb(0,0,0));
//                continue;
//            }

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
            cl_int pitchValue = grabbedScreen->screenInfo.rect.width() * bytesPerPixel;
//            GPU_calculateAvgColor(&avgColor[0], grabbedScreen->imgData, pitchValue, preparedRect);
//            qDebug() << currentWidgetIndex << preparedRect;
            if (_context->grabWidgets->at(i)->isAreaEnabled()) {
                memcpy(pSource+offset, grabbedScreen->imgData, preparedRect.width()*preparedRect.height());
                pWidth[i] = preparedRect.width();
                pHeight[i] = preparedRect.height();
                pX[i] = preparedRect.x();
                pY[i] = preparedRect.y();
                pPitch[i] = pitchValue;

                pRed[i] = 0;
                pGreen[i] = 0;
                pBlue[i] = 0;
                pCount[i] = 0;
            }
            offset+=(sizeof(char)*GPU_BLOCK_SIZE);
        }

        //run GPU calculate...
        {
            //Set arguments to kernel
            cSource  = cl::Buffer(context, CL_MEM_READ_ONLY|CL_MEM_COPY_HOST_PTR,  GPU_BLOCK_SIZE*GPU_DATA_COUNT*sizeof(cl_char), pSource);
            cWidth   = cl::Buffer(context, CL_MEM_READ_ONLY|CL_MEM_COPY_HOST_PTR,  GPU_DATA_COUNT * sizeof(cl_int), pWidth);
            cHeight  = cl::Buffer(context, CL_MEM_READ_ONLY|CL_MEM_COPY_HOST_PTR,  GPU_DATA_COUNT * sizeof(cl_int), pHeight);
            cX       = cl::Buffer(context, CL_MEM_READ_ONLY|CL_MEM_COPY_HOST_PTR,  GPU_DATA_COUNT * sizeof(cl_int), pX);
            cY       = cl::Buffer(context, CL_MEM_READ_ONLY|CL_MEM_COPY_HOST_PTR,  GPU_DATA_COUNT * sizeof(cl_int), pY);
            cPitch   = cl::Buffer(context, CL_MEM_READ_ONLY|CL_MEM_COPY_HOST_PTR,  GPU_DATA_COUNT * sizeof(cl_int), pPitch);

            cRed     = cl::Buffer(context, CL_MEM_READ_WRITE|CL_MEM_COPY_HOST_PTR, GPU_DATA_COUNT * sizeof(cl_int), pRed);
            cGreen   = cl::Buffer(context, CL_MEM_READ_WRITE|CL_MEM_COPY_HOST_PTR, GPU_DATA_COUNT * sizeof(cl_int), pGreen);
            cBlue    = cl::Buffer(context, CL_MEM_READ_WRITE|CL_MEM_COPY_HOST_PTR, GPU_DATA_COUNT * sizeof(cl_int), pBlue);
            cCount   = cl::Buffer(context, CL_MEM_READ_WRITE|CL_MEM_COPY_HOST_PTR, GPU_DATA_COUNT * sizeof(cl_int), pCount);

            cl_int res = 0;

            int iArg = 0;
            res = kernel.setArg(iArg++, cSource);
            res = kernel.setArg(iArg++, cWidth);
            res = kernel.setArg(iArg++, cHeight);
            res = kernel.setArg(iArg++, cX);
            res = kernel.setArg(iArg++, cY);
            res = kernel.setArg(iArg++, cPitch);
            res = kernel.setArg(iArg++, GPU_BLOCK_SIZE);
            res = kernel.setArg(iArg++, GPU_DATA_COUNT);

            res = kernel.setArg(iArg++, cRed);
            res = kernel.setArg(iArg++, cGreen);
            res = kernel.setArg(iArg++, cBlue);
            res = kernel.setArg(iArg++, cCount);

            res = queue.enqueueNDRangeKernel(kernel, cl::NullRange, cl::NDRange(GPU_DATA_COUNT), cl::NDRange(128));
            res = queue.finish();

//             Read buffer C into a local list
            res = queue.enqueueReadBuffer(cRed,   CL_FALSE, 0, GPU_DATA_COUNT * sizeof(int), pRed);
            res = queue.enqueueReadBuffer(cGreen, CL_FALSE, 0, GPU_DATA_COUNT * sizeof(int), pGreen);
            res = queue.enqueueReadBuffer(cBlue,  CL_FALSE, 0, GPU_DATA_COUNT * sizeof(int), pBlue);
            res = queue.enqueueReadBuffer(cCount, CL_FALSE, 0, GPU_DATA_COUNT * sizeof(int), pCount);
            res = queue.finish();

            for (int i=0; i<GPU_DATA_COUNT; i++) {
                _context->grabResult->append(qRgb(pRed[i], pGreen[i], pBlue[i]));
            }
        }
    }

    QList<QRgb> GPUgrabResult;
    for (int i=0; i<_context->grabResult->size(); i++) {
        GPUgrabResult.append(_context->grabResult->at(i));
    }

//    grab();

//    qDebug() << "";
//    emit frameGrabAttempted(_lastGrabResult);
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
