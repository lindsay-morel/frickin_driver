/**
 * @file driver.h
 * @author Gary Chang (gary.chang@memryx.com)
 * @brief This file contains the driver definitions.
 * @version 0.1
 * @date 2023
 *
 * @copyright Copyright (c) 2023 MemryX. Inc. All Rights Reserved.
 *
 */
#pragma once
#include <ntddk.h>
#include <wdf.h>
#include <initguid.h>
#include <wdmguid.h>
#include "public.h"
#include "device.h"
#include "queue.h"
#include "trace.h"
#include "memx_dma.h"

#pragma warning(disable:4127)  // conditional expression is constant

#define TODO (0)

EXTERN_C_START
 /*!
     @brief DriverEntry initializes the driver and is the first routine called by the system after the driver is loaded.

     @param[in]      DriverObject   represents the instance of the function driver that is loaded
                                    into memory. DriverEntry must initialize members of DriverObject before it
                                    returns to the caller. DriverObject is allocated by the system before the
                                    driver is loaded, and it is released by the system after the system unloads
                                    the function driver from memory.
     @param[in]      RegistryPath   represents the driver specific path in the Registry.
                                    The function driver can use the path to store driver related data between
                                    reboots. The path does not store hardware instance specific data.
     @return         NTSTATUS       STATUS_SUCCESS if successful, STATUS_UNSUCCESSFUL otherwise.
 */
DRIVER_INITIALIZE DriverEntry;

/*!
    @brief Free all the resources allocated in DriverEntry.

    @param[in]      DriverObject  handle to a WDF Driver object.

    @return         none
*/
EVT_WDF_OBJECT_CONTEXT_CLEANUP MemxEvtDriverContextCleanup;

/*!
    @brief EvtDeviceAdd is called by the framework in response to AddDevice call from the PnP manager.

    We create and initialize a device object to represent a new instance of the device.
    Here the driver should register all the PNP, power and Io callbacks, register interfaces and allocate other software resources required by the device.
    The driver can query any interfaces or get the config space information from the bus driver but cannot access hardware registers or initialize the device.

    @param[in]      Driver         Handle to a framework driver object created in DriverEntry
    @param[in]      DeviceInit     Pointer to a framework-allocated WDFDEVICE_INIT structure.
    @return         NTSTATUS       STATUS_SUCCESS if successful, STATUS_UNSUCCESSFUL otherwise.
 */
EVT_WDF_DRIVER_DEVICE_ADD MemxEvtDeviceAdd;

/*!
    @brief  Invoked before the device object is deleted.

    Frees allocated memory that was saved in the WDFDEVICE's context, before the device object is deleted.

    @param[in]      DriverObject  handle to a WDF Driver object.
    @return         none
*/
EVT_WDF_OBJECT_CONTEXT_CLEANUP MemxEvtDeviceCleanup;

/*!
     @brief This entry point is called when hardware resources are assigned to one of our devices.

    We almost never use the Raw Resources (as these are of primary interest to bus drivers). Here, we only reference our Translated Resources.
    Performs whatever initialization is needed to setup the device, setting up a DMA channel or mapping any I/O port resources.
    This will only be called as a device starts or restarts, not every time the device moves into the D0 state.
    Consequently, most hardware initialization belongs elsewhere.

     @param[in]      Device                 Handle to our WDFDEVICE object
     @param[in]      RawResources           WDFCMRESLIST of hardware resources assigned to our device.
     @param[in]      TranslatedResources    A WDFCMRESLIST of hardware resources assigned to our device, made directly usable
                                            by the driver.  We expect one memory resource and one interrupt resources.
     @return         NTSTATUS       STATUS_SUCCESS if successful, STATUS_UNSUCCESSFUL otherwise.
*/
EVT_WDF_DEVICE_PREPARE_HARDWARE MemxEvtDevicePrepareHardware;

/*!
    @brief This function is called any time Windows wants us to release our hardware resources.

    Examples include "bus rebalancing" and when the "Disable Device" function is selected in Device Manager.
    This callback IS NOT CALLED during system shutdown.
    This will only be called when the device stopped for resource rebalance, surprise-removed or query-removed.

    @param[in]      Device                 Handle to our WDFDEVICE object
    @param[in]      ResourcesTranslated    The resources we're returning
    @return         NTSTATUS       STATUS_SUCCESS if successful, STATUS_UNSUCCESSFUL otherwise.
*/
EVT_WDF_DEVICE_RELEASE_HARDWARE MemxEvtDeviceReleaseHardware;

/*!
    @brief This function is called each time our device has been transitioned into the D0-Working (fully powered on) state.

    This includes during the "implicit power on" that occurs after the device is first discovered.
    Our job here is to initialize or restore the state of our device.
    This function is not marked pageable because this function is in the device power up path.
    When a function is marked pagable and the code section is paged out, it will generate a page fault which could impact the fast resume behavior
    because the client driver will have to wait until the system drivers can service this page fault.

    This function runs at PASSIVE_LEVEL, even though it is not paged.
    A driver can optionally make this function pageable if DO_POWER_PAGABLE is set.
    Even if DO_POWER_PAGABLE isn't set, this function still runs at PASSIVE_LEVEL.
    In this case, though, the function absolutely must not do anything that will cause a page fault.

    @param[in]      Device          Handle to our WDFDEVICE object
    @param[in]      PreviousState   The state from which we transitioned to D0
    @return         NTSTATUS        STATUS_SUCCESS if successful, STATUS_UNSUCCESSFUL otherwise.
*/
EVT_WDF_DEVICE_D0_ENTRY MemxEvtDeviceD0Entry;

/*!
    @brief This function is called when our device is about to transition OUT of D0.

    The target state is passed as an argument.
    Our job here is to save any state associated with the device, so it can be restored when power is returned to the device.
    It is called whenever the device leaves the D0 state, which happens when the device is stopped, when it is removed, and when it is powered off.
    The device is still in D0 when this callback is invoked, which means that the driver can still touch hardware in this routine.
    Note that interrupts have already been disabled by the time that thiscallback is invoked.

    @param[in]      Device          Handle to our WDFDEVICE object
    @param[in]      TagetState      The state to which we're transitioning, from D0
    @return         NTSTATUS        STATUS_SUCCESS if successful, STATUS_UNSUCCESSFUL otherwise.
*/
EVT_WDF_DEVICE_D0_EXIT  MemxEvtDeviceD0Exit;

/*!
    @brief Called by WDF to ask us to enable interrupts on our device.

    Called by the framework at DIRQL immediately after registering the ISR with the kernel by calling IoConnectInterrupt.

    @param[in]      Interrupt       Handle to our WDFINTERRUPT object
    @param[in]      Device          Handle to our WDFDEVICE object

    @return         NTSTATUS        STATUS_SUCCESS if successful, STATUS_UNSUCCESSFUL otherwise.
*/
EVT_WDF_INTERRUPT_ENABLE MemxEvtInterruptEnable;

/*!
    @brief Called by WDF to ask us to disable interrupt on our device.

    Called by the framework at DIRQL before Deregistering the ISR with the kernel by calling IoDisconnectInterrupt.

    @param[in]      Interrupt       Handle to our WDFINTERRUPT object
    @param[in]      Device          Handle to our WDFDEVICE object

    @return         NTSTATUS        STATUS_SUCCESS if successful, STATUS_UNSUCCESSFUL otherwise.
*/
EVT_WDF_INTERRUPT_DISABLE MemxEvtInterruptDisable;

/*!
    @brief This is our driver's interrupt service routine.

    Interrupt handler for this driver. Called at DIRQL level when the device or another device sharing the same interrupt line asserts the interrupt.
    The driver first checks the device to make sure whether this interrupt is generated by its device and if so clear the interrupt
    register to disable further generation of interrupts and queue a DPC to do other I/O work related to interrupt
    - such as reading the device memory, starting a DMA transaction, coping it to the request buffer and completing the request, etc.

    @param[in]      Interrupt       Handle to our WDFINTERRUPT object describing this Interrupt
    @param[in]      MessageId       The zero-based message number of the MSI/MSI-x message we're processing.

    @return         BOOLEAN
*/
EVT_WDF_INTERRUPT_ISR MemxEvtInterruptIsr;

/*!
    @brief This is our DpcForIsr function, where we complete any processing that was started in our ISR.

    DPC callback for ISR.
    Please note that on a multiprocessor system, you could have more than one DPCs running simulataneously on multiple processors.
    So if you are accesing any global resources make sure to synchrnonize the accesses with a spinlock.

    @param[in]      Interrupt       Handle to our WDFINTERRUPT object
    @param[in]      Device          Handle to our WDFDEVICE object

    @return         none
*/
EVT_WDF_INTERRUPT_DPC MemxEvtInterruptDpc;

/*!
    @brief A driver's event callback function performs device-specific operations that are required after the driver has enabled the device's hardware interrupts.

    @param[in]     Device           Handle to our WDFDEVICE object
    @param[in]     PreviousState    A WDF_POWER_DEVICE_STATE-typed enumerator that identifies the previous device power state.

    @return         none
*/
EVT_WDF_DEVICE_D0_ENTRY_POST_INTERRUPTS_ENABLED  MemxEvtPostInterruptEnable;

/*!
    @brief A driver's event callback function performs device-specific operations that are required before the driver disables the device's hardware interrupts.

    @param[in]     Device          Handle to our WDFDEVICE object
    @param[in]     TargetState     A WDF_POWER_DEVICE_STATE-typed enumerator that identifies the device power state that the device is about to enter.

    @return         none
*/
EVT_WDF_DEVICE_D0_EXIT_PRE_INTERRUPTS_DISABLED   MemxEvtPreInterruptDisable;

EXTERN_C_END
