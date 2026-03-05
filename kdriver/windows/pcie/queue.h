/**
 * @file queue.h
 * @author Gary Chang (gary.chang@memryx.com)
 * @brief This file contains the queue definitions.
 * @version 0.1
 * @date 2023
 *
 * @copyright Copyright (c) 2023 MemryX. Inc. All Rights Reserved.
 *
 */
#pragma once
EXTERN_C_START

NTSTATUS MemxProcessFwAckDone(PDEVICE_CONTEXT devContext, PINT_CONTEXT IntContext);
NTSTATUS MemxProcessTxDone(PDEVICE_CONTEXT devContext, PINT_CONTEXT IntContext);
NTSTATUS MemxProcessRxDone(PDEVICE_CONTEXT devContext, PINT_CONTEXT IntContext);

//
// This context is associated with every request received by the driver
// from the app.
//
typedef struct _REQUEST_CONTEXT {
    PVOID               funcAry;
    PVOID               context;
    ULONG               curState;
    ULONG               totalState;
} REQUEST_CONTEXT, * PREQUEST_CONTEXT;

WDF_DECLARE_CONTEXT_TYPE_WITH_NAME(REQUEST_CONTEXT, GetRequestContext)

/*!
    @brief The framework calls a driver's EvtDeviceFileCreate callback when the framework receives an IRP_MJ_CREATE request.

    The system sends this request when a user application opens the device to perform an I/O operation, such as reading or writing a file.
    This callback is called synchronously

    @param[in]      Device          Handle to a framework device object.
    @param[in]      Request         A handle to a framework request object that represents a file creation request.
    @param[in]      FileObject      A handle to a framework file object that describes a file that is being opened for the specified request.
    @return         None
*/
EVT_WDF_DEVICE_FILE_CREATE  MemxEvtDeviceFileCreate;

/*!
    @brief  A driver's EvtFileClose callback function handles operations that must be performed when all of an application's accesses to a device have been closed.

    @param[in]      FileObject      A handle to a framework file object that describes a file that is being opened for the specified request.
    @return         None
*/
EVT_WDF_FILE_CLOSE MemxEvtDeviceFileClose;

/*!
    @brief This event is invoked when the framework receives IRP_MJ_DEVICE_CONTROL request.

    @param[in]      Queue              Handle to the framework queue object that is associated with the I/O request.
    @param[in]      Request            Handle to a framework request object.
    @param[in,out]  OutputBufferLength Size of the output buffer in bytes
    @param[in]      InputBufferLength  Size of the input buffer in bytes
    @param[in]      IoControlCode      I/O control code.
    @return         None
*/
EVT_WDF_IO_QUEUE_IO_DEVICE_CONTROL MemxEvtIoDeviceControl;

/*!
    @brief This event is invoked when the framework receives IRP_MJ_READ request.

    Called by the framework as soon as it receives a read request.
    If the device is not ready, fail the request.
    Otherwise get scatter-gather list for this request and send the packet to the hardware for DMA.

    The default property of the queue is to not dispatch zero lenght read & write requests to the driver
    and complete is with status success. So we will never get a zero length request.

    @param[in]      Queue              Handle to the framework queue object that is associated with the I/O request.
    @param[in]      Request            Handle to a framework request object.
    @param[in]      Length Size of the Length of the data buffer associated with the request.
    @return         None
*/
EVT_WDF_IO_QUEUE_IO_READ MemxEvtIoRead;

/*!
    @brief This event is invoked when the framework receives IRP_MJ_WRITE request.

    Called by the framework as soon as it receives a write request.
    If the device is not ready, fail the request.
    Otherwise get scatter-gather list for this request and send the packet to the hardware for DMA.

    The default property of the queue is to not dispatch zero lenght read & write requests to the driver
    and complete is with status success. So we will never get a zero length request.

    @param[in]      Queue       Handle to the framework queue object that is associated with the I/O request.
    @param[in]      Request     Handle to a framework request object.
    @param[in]      Length      Size of the Length of the data buffer associated with the request.
    @return         None
*/
EVT_WDF_IO_QUEUE_IO_WRITE MemxEvtIoWrite;

EXTERN_C_END
