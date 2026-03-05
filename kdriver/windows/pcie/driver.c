/**
 * @file driver.c
 * @author Gary Chang (gary.chang@memryx.com)
 * @brief This file contains the driver entry points and callbacks.
 * @version 0.1
 * @date 2023
 *
 * @copyright Copyright (c) 2023 MemryX. Inc. All Rights Reserved.
 *
 */
#include "driver.h"
#include "driver.tmh"

#ifdef ALLOC_PRAGMA
    #pragma alloc_text (INIT, DriverEntry)
    #pragma alloc_text (PAGE, MemxEvtDeviceAdd)
    #pragma alloc_text (PAGE, MemxEvtDriverContextCleanup)
    #pragma alloc_text (PAGE, MemxEvtDevicePrepareHardware)
    #pragma alloc_text (PAGE, MemxEvtDeviceReleaseHardware)
    #pragma alloc_text (PAGE, MemxEvtDeviceCleanup)
    #pragma alloc_text (PAGE, MemxEvtDeviceD0Exit)
    #pragma alloc_text (PAGE, MemxEvtDeviceD0Entry)
    #pragma alloc_text (PAGE, MemxEvtPostInterruptEnable)
#endif

static NTSTATUS IdentifyBarMappingType(WDFDEVICE Device)
{
    PDEVICE_CONTEXT                 devContext = GetDeviceContext(Device);;
    NTSTATUS                        status = STATUS_SUCCESS;

    devContext->BarInfo.BarMode         = MEMXBAR_NOTVALID;
    devContext->BarInfo.XflowVbufOffset = 0;
    devContext->BarInfo.XflowConfOffset = 0;
    devContext->BarInfo.SramIdx         = MAX_BAR;
    devContext->BarInfo.XflowVbufIdx    = MAX_BAR;
    devContext->BarInfo.XflowConfIdx    = MAX_BAR;

    if ((devContext->Bar[BAR0].Length == DEF_MB(256)) && (devContext->Bar[BAR1].Length == DEF_1MB)) {
        devContext->BarInfo.BarMode         = MEMXBAR_XFLOW256MB_SRAM1MB;
        devContext->BarInfo.SramIdx         = BAR1;
        devContext->BarInfo.XflowVbufIdx    = BAR0;
        devContext->BarInfo.XflowConfIdx    = BAR0;
    } else if ((devContext->Bar[BAR0].Length == DEF_MB(128)) && (devContext->Bar[BAR1].Length == DEF_1MB)) {
        devContext->BarInfo.BarMode         = MEMXBAR_XFLOW128MB64B_SRAM1MB;
        devContext->BarInfo.SramIdx         = BAR1;
        devContext->BarInfo.XflowVbufIdx    = BAR0;
        devContext->BarInfo.XflowConfIdx    = BAR0;
    } else if ((devContext->Bar[BAR0].Length == DEF_MB(16)) && (devContext->Bar[BAR1].Length == DEF_MB(16)) && (devContext->Bar[BAR2].Length == DEF_1MB)) {
        devContext->BarInfo.BarMode         = MEMXBAR_3BAR_BAR0VB_BAR2CI_16MB_BAR4SRAM;
        devContext->BarInfo.SramIdx         = BAR2;
        devContext->BarInfo.XflowVbufIdx    = BAR0;
        devContext->BarInfo.XflowConfIdx    = BAR1;
        devContext->BarInfo.XflowVbufOffset = XFLOW_VIRTUAL_BUFFER_PREFIX;
        devContext->BarInfo.XflowConfOffset = XFLOW_CONFIG_REG_PREFIX;
    } else if ((devContext->Bar[BAR0].Length == DEF_MB(64)) && (devContext->Bar[BAR1].Length == DEF_MB(64)) && (devContext->Bar[BAR2].Length == DEF_1MB)) {
        devContext->BarInfo.BarMode         = MEMXBAR_3BAR_BAR0VB_BAR2CI_64MB_BAR4SRAM;
        devContext->BarInfo.SramIdx         = BAR2;
        devContext->BarInfo.XflowVbufIdx    = BAR0;
        devContext->BarInfo.XflowConfIdx    = BAR1;
        devContext->BarInfo.XflowVbufOffset = XFLOW_VIRTUAL_BUFFER_PREFIX;
        devContext->BarInfo.XflowConfOffset = XFLOW_CONFIG_REG_PREFIX;
    } else if ((devContext->Bar[BAR0].Length == DEF_KB(512)) && (devContext->Bar[BAR1].Length == DEF_KB(256)) && (devContext->Bar[BAR2].Length == DEF_KB(4)) && (devContext->Bar[BAR3].Length == DEF_1MB)) {
        devContext->BarInfo.BarMode         = MEMXBAR_4BAR_BAR0VB_BAR2CI_BAR4MSIX_BAR5SRAM;
        devContext->BarInfo.SramIdx         = BAR3;
        devContext->BarInfo.XflowVbufIdx    = BAR0;
        devContext->BarInfo.XflowConfIdx    = BAR1;
        devContext->BarInfo.DeviceIrqIdx    = BAR2;
        devContext->BarInfo.XflowVbufOffset = XFLOW_VIRTUAL_BUFFER_PREFIX;
        devContext->BarInfo.XflowConfOffset = XFLOW_CONFIG_REG_PREFIX;
    } else {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER, "Detect BarMode is Not Supported\n");
        for (ULONG index = 0; index < MAX_BAR; index++)
        {
            TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "BAR %d Base 0x%I64x, Length %d\n", index, devContext->Bar[index].BaseAddress, devContext->Bar[index].Length);
        }
        WdfDeviceSetFailed(Device, WdfDeviceFailedNoRestart);
        status = STATUS_NOT_IMPLEMENTED;
    }

    if (devContext->BarInfo.BarMode != MEMXBAR_NOTVALID)
    {
        TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "Detect BarMode is %d\n", devContext->BarInfo.BarMode);
        devContext->mmap_fw_command_buffer_base = (PVOID)((ULONG_PTR)devContext->Bar[devContext->BarInfo.SramIdx].MappingAddress + (PHYSICAL_FW_COMMAND_SRAM_BASE - PHYSICAL_SRAM_BASE));
    }


    return status;
}

NTSTATUS DriverEntry(PDRIVER_OBJECT  DriverObject, PUNICODE_STRING RegistryPath)
{
    WDF_DRIVER_CONFIG       config;
    NTSTATUS                status      = STATUS_SUCCESS;
    WDF_OBJECT_ATTRIBUTES   attributes;

    // Initialize WPP Tracing
    WPP_INIT_TRACING(DriverObject, RegistryPath);

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "%!FUNC! Entry");
#if DBG
    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "PCIe kdriver Built %s %s\n", __DATE__, __TIME__);
#endif
    //
    // Register a cleanup callback so that we can call WPP_CLEANUP when
    // the framework driver object is deleted during driver unload.
    //
    WDF_OBJECT_ATTRIBUTES_INIT(&attributes);
    attributes.EvtCleanupCallback = MemxEvtDriverContextCleanup;

    WDF_DRIVER_CONFIG_INIT(&config, MemxEvtDeviceAdd);

    // Create our WDFDRIVER object
    status = WdfDriverCreate(DriverObject, RegistryPath, &attributes, &config, WDF_NO_HANDLE);

    if (!NT_SUCCESS(status)) {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER, "WdfDriverCreate failed 0x%x", status);
        WPP_CLEANUP(DriverObject);
    }
    else
    {
        TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "%!FUNC! Exit 0x%x", status);
    }

    return status;
}

VOID MemxEvtDriverContextCleanup(WDFOBJECT DriverObject)
{
    UNREFERENCED_PARAMETER(DriverObject);

    PAGED_CODE();

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "%!FUNC! Entry");

    //
    // Stop WPP Tracing
    //
    WPP_CLEANUP(WdfDriverWdmGetDriverObject((WDFDRIVER)DriverObject));
}

NTSTATUS MemxEvtDeviceAdd(WDFDRIVER Driver, PWDFDEVICE_INIT DeviceInit)
{
    NTSTATUS                        status = STATUS_SUCCESS;
    WDF_OBJECT_ATTRIBUTES           objAttributes;
    WDF_PNPPOWER_EVENT_CALLBACKS    pnpPowerCallbacks;
    WDFDEVICE                       device;
    WDF_IO_QUEUE_CONFIG             queueConfig;
    PDEVICE_CONTEXT                 devContext;
    WDF_FILEOBJECT_CONFIG           fileConfig;

    UNREFERENCED_PARAMETER(Driver);

    PAGED_CODE();

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "%!FUNC! Entry");

    // Initialize WDF_FILEOBJECT_CONFIG_INIT struct to tell the
    // framework whether you are interested in handle Create, Close and
    // Cleanup requests that gets genereate when an application or another
    // kernel component opens an handle to the device. If you don't register
    // the framework default behaviour would be complete these requests
    // with STATUS_SUCCESS. A driver might be interested in registering these
    // events if it wants to do security validation and also wants to maintain
    // per handle (fileobject) context.

    //1. If a driver calls WdfDeviceInitSetXXXX, it must do so before it calls WdfDeviceCreate.
    WDF_FILEOBJECT_CONFIG_INIT(&fileConfig, MemxEvtDeviceFileCreate, MemxEvtDeviceFileClose, WDF_NO_EVENT_CALLBACK);
    WdfDeviceInitSetFileObjectConfig(DeviceInit, &fileConfig, WDF_NO_OBJECT_ATTRIBUTES);
    WdfDeviceInitSetIoType(DeviceInit, WdfDeviceIoDirect);
    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&objAttributes, REQUEST_CONTEXT);
    WdfDeviceInitSetRequestAttributes(DeviceInit, &objAttributes);

    //2. Specify object-specific configuration. We want to specify PnP/Power  Callbacks to manage our hardware resources.
    WDF_PNPPOWER_EVENT_CALLBACKS_INIT(&pnpPowerCallbacks);

    // Prepare Hardware is called to give us our hardware resources
    // Release Hardware is called at when we need to return hardware resources
    pnpPowerCallbacks.EvtDevicePrepareHardware = MemxEvtDevicePrepareHardware;
    pnpPowerCallbacks.EvtDeviceReleaseHardware = MemxEvtDeviceReleaseHardware;

    // These two callbacks set up and tear down hardware state that must be
    // done every time the device moves in and out of the D0-Working state.
    pnpPowerCallbacks.EvtDeviceD0Entry = MemxEvtDeviceD0Entry;
    pnpPowerCallbacks.EvtDeviceD0Exit  = MemxEvtDeviceD0Exit;
    pnpPowerCallbacks.EvtDeviceD0EntryPostInterruptsEnabled = MemxEvtPostInterruptEnable;
    pnpPowerCallbacks.EvtDeviceD0ExitPreInterruptsDisabled  = MemxEvtPreInterruptDisable;

    //3.  Copy the contents of the PnP/Power Callbacks "collector structure" to
    //    our WDFDEVICE_INIT structure (which is the object-specific configurator
    //    for WDFDEVICE).
    WdfDeviceInitSetPnpPowerEventCallbacks(DeviceInit, &pnpPowerCallbacks);

    //4. Associate our device context structure type with our WDFDEVICE
    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&objAttributes, DEVICE_CONTEXT);

    //4.1 Set SynchronizationScope to WdfSynchronizationScopeQueue in the WDF_OBJECT_ATTRIBUTES structure of the device object.
    //    Use the default WdfSynchronizationScopeInheritFromParent value for each device's queue objects.
    objAttributes.SynchronizationScope = WdfSynchronizationScopeQueue;

    // This callback is invoked when the device is removed or the driver is unloaded.
    objAttributes.EvtCleanupCallback = MemxEvtDeviceCleanup;

    //5. instantiate the WDFDEVICE Object.
    status = WdfDeviceCreate(&DeviceInit, &objAttributes, &device);
    if (!NT_SUCCESS(status)) {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER, "WdfDeviceInitialize failed 0x%0x\n", status);
        goto done;
    }

    // Get a pointer to our device context,
    devContext = GetDeviceContext(device);
    devContext->WdfDevice = device;

    WDF_OBJECT_ATTRIBUTES attributes;
    WDF_OBJECT_ATTRIBUTES_INIT(&attributes);
    attributes.ParentObject = device;

    for (UCHAR i = 0; i < QUEUE_MAX_NUM; i++) {
        status = WdfSpinLockCreate(&attributes, &devContext->IoQueueLockHandle[i]);

        if (!NT_SUCCESS(status)) {
            TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER, "WdfSpinLockCreate failed 0x%0x\n", status);
            goto done;
        }
    }

    for (UCHAR i = 0; i < MAX_SUPPORT_CHIP_NUM; i++) {
        status = WdfSpinLockCreate(&attributes, &devContext->XflowLockHandle[i]);

        if (!NT_SUCCESS(status)) {
            TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER, "WdfSpinLockCreate failed 0x%0x\n", status);
            goto done;
        }
    }

    status = WdfSpinLockCreate(&attributes, &devContext->device_lock);
    if (!NT_SUCCESS(status)) {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER, "WdfSpinLockCreate device_lock failed 0x%0x\n", status);
        goto done;
    }

    // 4-2
    // And make our device accessible via a Device Interface Class GUID.
    // User-mode users would call CM_Get_Device_Interface_List to get a
    // list of Memx devices by specifying GUID_CLASS_MEMX_CASCADE_SINGLE_PCIE.
    // Optionally (user mode AND kernel mode) users can also register to be
    // notified of the arrival/departure of this device (for user mode, see
    // CM_Register_Notification).
    //
    status = WdfDeviceCreateDeviceInterface(device, &GUID_CLASS_MEMX_CASCADE_SINGLE_PCIE, NULL);
    if (!NT_SUCCESS(status)) {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER, "WdfDeviceCreateDeviceInterface failed 0x%0x\n", status);
        goto done;
    }

    //Drivers for DMA devices sometimes must allocate buffer space that both a device and the driver can access.
    //To allocate a common buffer, your driver's EvtDriverDeviceAdd callback function:

    //The WdfDeviceSetAlignmentRequirement method registers the driver's preferred address alignment for the data buffers
    WdfDeviceSetAlignmentRequirement(device, DEF_ADDRESS_ALIGEMENT(DMA_COHERENT_BUFFER_SIZE_1MB));

    //Calls WdfDmaEnablerCreate to create a DMA enabler object.
    WDF_DMA_ENABLER_CONFIG   dmaConfig;
    WDF_DMA_ENABLER_CONFIG_INIT(&dmaConfig, WdfDmaProfileScatterGather64Duplex, DEF_KB(16));
    // Opt - in to DMA version 3, which is required by WdfDmaTransactionSetSingleTransferRequirement
    dmaConfig.WdmDmaVersionOverride = 3;
    // Our Device DMA only address 32 bit
    dmaConfig.AddressWidthOverride  = 32;

    status = WdfDmaEnablerCreate(device, &dmaConfig, WDF_NO_OBJECT_ATTRIBUTES, &devContext->DmaEnabler);
    if (!NT_SUCCESS(status)) {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER, "WdfDmaEnablerCreate failed 0x%0x\n", status);
        goto done;
    }

    //Calls WdfCommonBufferCreate or WdfCommonBufferCreateWithConfig to create the buffer.
    UCHAR idx = 0;
    for (idx = 0; idx < MAX_NUM_COMBUFFER; idx++){
        //Calls WdfCommonBufferCreate or WdfCommonBufferCreateWithConfig to create the buffer.
        status = WdfCommonBufferCreate(devContext->DmaEnabler, MEMX_HOST_BUFFER_BLOCK_SIZE, WDF_NO_OBJECT_ATTRIBUTES, &devContext->CommonBuffer[idx]);
        if (!NT_SUCCESS(status)) {
            TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER, "WdfCommonBufferCreate failed 0x%0x\n", status);
            goto done;
        } else {
            //Calls WdfCommonBufferGetAlignedLogicalAddress to obtain the buffer's logical address, which the device can access.
            devContext->CommonBufferBaseDevice[idx] = WdfCommonBufferGetAlignedLogicalAddress(devContext->CommonBuffer[idx]);
            //Calls WdfCommonBufferGetAlignedVirtualAddress to obtain the buffer's virtual address, which the driver can access.
            devContext->CommonBufferBaseDriver[idx] = WdfCommonBufferGetAlignedVirtualAddress(devContext->CommonBuffer[idx]);
            TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "CommonBuffer[%d]  0x%p  (#0x%I64X), length %I64d", idx, devContext->CommonBufferBaseDriver[idx], devContext->CommonBufferBaseDevice[idx].QuadPart, WdfCommonBufferGetLength(devContext->CommonBuffer[idx]));
            RtlZeroMemory(devContext->CommonBufferBaseDriver[idx], MEMX_HOST_BUFFER_BLOCK_SIZE);
        }
    }

    RtlZeroMemory(&(devContext->busInterface), sizeof(BUS_INTERFACE_STANDARD));
    status = WdfFdoQueryForInterface(device, &GUID_BUS_INTERFACE_STANDARD, (PINTERFACE) &(devContext->busInterface), sizeof(BUS_INTERFACE_STANDARD), 1, NULL);
    if (!NT_SUCCESS(status)) {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER, "WdfFdoQueryForInterface GUID_BUS_INTERFACE_STANDARD failed 0x%0x\n", status);
        goto done;
    }

#if TODO
    //6. The attempt to set the Idle and Wake options is a best-effort try.
    NTSTATUS                              powerStatus = STATUS_SUCCESS;
    WDF_DEVICE_POWER_POLICY_IDLE_SETTINGS idleSettings;
    WDF_DEVICE_POWER_POLICY_WAKE_SETTINGS wakeSettings;

    //
    // Initialize our idle policy
    //
    // We accept most of the defaults here. Our device will idle in D3, and
    // WDF will create a property sheet for Device Manager that will allow
    // admin users to specify whether our device should idle in low-power
    // state.
    //
    WDF_DEVICE_POWER_POLICY_IDLE_SETTINGS_INIT(&idleSettings, IdleCanWakeFromS0);
    idleSettings.IdleTimeout = 10000; // 10-sec

    //
    // After 10 seconds of no activity, declare our device idle
    // Note that "idle" in this context means that the driver does not have
    // any Requests in progress.  So, while we have a Request on the
    // PendingQueue (waiting to be informed of a line state change), WDF
    // will *not* idle the device. Recall that a device can always be
    // made to enter into, and remain in, D0-Working by calling
    // WdfDeviceStopIdle.
    //
    powerStatus = WdfDeviceAssignS0IdleSettings(device, &idleSettings);

    if (!NT_SUCCESS(powerStatus)) {
        TraceEvents(TRACE_LEVEL_WARNING, TRACE_DRIVER, "WdfDeviceAssignS0IdleSettings failed 0x%0x\n", powerStatus);
    }

    WDF_DEVICE_POWER_POLICY_WAKE_SETTINGS_INIT(&wakeSettings);
    powerStatus = WdfDeviceAssignSxWakeSettings(device, &wakeSettings);

    if (!NT_SUCCESS(powerStatus)) {
        TraceEvents(TRACE_LEVEL_WARNING, TRACE_DRIVER, "WdfDeviceAssignSxWakeSettings failed 0x%0x\n", powerStatus);
    }
#endif

    //7. Configure a queue to handle incoming requests
    //
    // We use a single, default, queue for receiving Requests, and we only
    // support IRP_MJ_DEVICE_CONTROL.
    // We don't have Object Attributes for our Queue that we need to specify
    // With Sequential Dispatching, we will only get one request at a time
    // from our Queue.
    WDF_IO_QUEUE_CONFIG_INIT_DEFAULT_QUEUE(&queueConfig, WdfIoQueueDispatchSequential);

    queueConfig.EvtIoDeviceControl  = MemxEvtIoDeviceControl;

    //4. Associate our device context structure type with our WDFDEVICE
    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&objAttributes, QUEUE_CONTEXT);
    objAttributes.SynchronizationScope = WdfSynchronizationScopeInheritFromParent;

    status = WdfIoQueueCreate(device, &queueConfig, &objAttributes, &devContext->IoctlQueue);
    if (!NT_SUCCESS(status)) {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER, "WdfIoQueueCreate for ioctl queue failed 0x%0x\n", status);
        goto done;
    }

    //We also create a manual Queue to hold Requests that are waiting for device response
    WDF_IO_QUEUE_CONFIG_INIT( &queueConfig, WdfIoQueueDispatchManual);

    status = WdfIoQueueCreate(device, &queueConfig, WDF_NO_OBJECT_ATTRIBUTES, &devContext->IoctlPendingQueue);
    if (!NT_SUCCESS(status)) {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER, "WdfIoQueueCreate for IoctlPendingQueue failed 0x%0x\n", status);
        goto done;
    }

    // Setup a queue to handle only IRP_MJ_WRITE requests in Sequential dispatch mode. T
    WDF_IO_QUEUE_CONFIG_INIT( &queueConfig, WdfIoQueueDispatchSequential);

    queueConfig.EvtIoWrite  = MemxEvtIoWrite;

    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&objAttributes, QUEUE_CONTEXT);
    objAttributes.SynchronizationScope = WdfSynchronizationScopeInheritFromParent;

    status = WdfIoQueueCreate(device, &queueConfig, &objAttributes, &devContext->WriteQueue);
    if (!NT_SUCCESS(status)) {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER, "WdfIoQueueCreate for write queue failed 0x%0x\n", status);
        goto done;
    }

    status = WdfDeviceConfigureRequestDispatching( device, devContext->WriteQueue, WdfRequestTypeWrite);
    if(!NT_SUCCESS(status)) {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER, "DeviceConfigureRequestDispatching failed: 0x%0x", status);
        goto done;
    }

    //We also create a manual Queue to hold Requests that are waiting for device response
    WDF_IO_QUEUE_CONFIG_INIT( &queueConfig, WdfIoQueueDispatchManual);

    status = WdfIoQueueCreate(device, &queueConfig, WDF_NO_OBJECT_ATTRIBUTES, &devContext->WritePendingQueue);
    if (!NT_SUCCESS(status)) {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER, "WdfIoQueueCreate for WritePendingQueue failed 0x%0x\n", status);
        goto done;
    }

    // Setup a queue to handle only IRP_MJ_READ requests in Sequential dispatch mode.
    WDF_IO_QUEUE_CONFIG_INIT ( &queueConfig, WdfIoQueueDispatchSequential);

    queueConfig.EvtIoRead   = MemxEvtIoRead;

    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&objAttributes, QUEUE_CONTEXT);
    objAttributes.SynchronizationScope = WdfSynchronizationScopeInheritFromParent;

    status = WdfIoQueueCreate(device, &queueConfig, &objAttributes, &devContext->ReadQueue);

    if (!NT_SUCCESS(status)) {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER, "WdfIoQueueCreate for read queue failed 0x%0x\n", status);
        goto done;
    }

    status = WdfDeviceConfigureRequestDispatching( device, devContext->ReadQueue, WdfRequestTypeRead);
    if(!NT_SUCCESS(status)) {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER, "DeviceConfigureRequestDispatching failed: %!STATUS!", status);
        goto done;
    }

    //We also create a manual Queue to hold Requests that are waiting for device response
    WDF_IO_QUEUE_CONFIG_INIT( &queueConfig, WdfIoQueueDispatchManual);

    status = WdfIoQueueCreate(device, &queueConfig, WDF_NO_OBJECT_ATTRIBUTES, &devContext->ReadPendingQueue);
    if (!NT_SUCCESS(status)) {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER, "WdfIoQueueCreate for ReadPendingQueue failed 0x%0x\n", status);
        goto done;
    }

    devContext->IndicatorHeader = 0;
    devContext->IndicatorTail   = 0;
    devContext->is_read_abort   = 0;
    devContext->reference_count = 0;
    RtlZeroMemory(devContext->download_wmem_context, sizeof(devContext->download_wmem_context));
done:
    return status;
}

VOID MemxEvtDeviceCleanup(WDFOBJECT Device)
{
    PDEVICE_CONTEXT          devContext;

    PAGED_CODE();

    devContext = GetDeviceContext((WDFDEVICE)Device);
}

NTSTATUS MemxEvtDevicePrepareHardware(WDFDEVICE Device, WDFCMRESLIST RawResources, WDFCMRESLIST TranslatedResources)
{
    PAGED_CODE();
    PDEVICE_CONTEXT                 devContext;
    PCM_PARTIAL_RESOURCE_DESCRIPTOR descriptor;
    NTSTATUS                        status          = STATUS_SUCCESS;
    BOOLEAN                         interruptFound  = FALSE;
    ULONG                           RawCnt          = WdfCmResourceListGetCount(RawResources);
    ULONG                           TransCnt        = WdfCmResourceListGetCount(TranslatedResources);
    WDF_INTERRUPT_CONFIG            interruptConfig;
    WDF_OBJECT_ATTRIBUTES           objAttributes;
    ULONG                           IntrRegisterCnt = 1, MessageIntrCnt = 0, j;

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "%!FUNC! Entry");
    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "RawResources %u TranslatedResources %u\n", RawCnt, TransCnt);

    devContext = GetDeviceContext(Device);

    for (j = 0; j < TransCnt; j++) {

        descriptor = WdfCmResourceListGetDescriptor(TranslatedResources, j);
        if (descriptor == NULL) {
            TraceEvents(TRACE_LEVEL_WARNING, TRACE_DRIVER, "NULL resource returned??\n");
            break;
        }

        if ((descriptor->Type == CmResourceTypeInterrupt) && (descriptor->Flags & CM_RESOURCE_INTERRUPT_MESSAGE))
            MessageIntrCnt++;
    }

    // if only one Message interrupt is got, we assume this is MSI
    if (MessageIntrCnt == 1)
        IntrRegisterCnt = 16;

    for (ULONG i = 0; i < TransCnt; i++) {

        // Get the i'th partial resource descriptor from the list
        descriptor = WdfCmResourceListGetDescriptor(TranslatedResources, i);

        if (descriptor == NULL) {
            TraceEvents(TRACE_LEVEL_WARNING, TRACE_DRIVER, "NULL resource returned??\n");
            break;
        }

        // Examine and print the resources, based on their type
        switch (descriptor->Type) {
            case CmResourceTypeMemory: {
                TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER,  "Resource %lu: Memory Flag 0x%x\n", i, descriptor->Flags);
                TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER,  "Base: 0x%I64x Length: %lu KB\n", descriptor->u.Memory.Start.QuadPart, (descriptor->u.Memory.Length / DEF_1KB));

                if (descriptor->u.Memory.Start.QuadPart)
                {
                    for (ULONG index = 0; index < MAX_BAR; index++)
                    {
                        if (devContext->Bar[index].BaseAddress)
                        {
                            //BAR index has found
                            TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER,  "BAR %d has Found 0x%I64x\n", index, devContext->Bar[index].BaseAddress);
                        }
                        else
                        {
                            TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER,  "BAR %d Found\n", index);
                            devContext->Bar[index].BaseAddress = descriptor->u.Memory.Start.QuadPart;
                            devContext->Bar[index].Length      = descriptor->u.Memory.Length;
                            devContext->Bar[index].MappingAddress = MmMapIoSpaceEx(descriptor->u.Memory.Start, descriptor->u.Memory.Length, PAGE_READWRITE);
                            break;
                        }
                    }
                }
                break;
            }
            case CmResourceTypeInterrupt: {
                //Note that when using MSI, the driver only receives one interrupt resource descriptor, since all messages share the same address.
                //The MessageCount member of u.MessageInterrupt.Raw can be used to determine the number of messages assigned.
                //When using MSI-X, the driver receives a separate resource descriptor for each interrupt message.
                TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER,  "Resource %lu: Interrupt Flag 0x%x\n", i, descriptor->Flags);

                interruptFound = TRUE;

                if (descriptor->Flags & CM_RESOURCE_INTERRUPT_MESSAGE) { //MSI
                    PCM_PARTIAL_RESOURCE_DESCRIPTOR resourceRawItem;
                    PCM_PARTIAL_RESOURCE_DESCRIPTOR resourceTranslateItem;
                    resourceRawItem = WdfCmResourceListGetDescriptor(RawResources, i);
                    resourceTranslateItem = WdfCmResourceListGetDescriptor(TranslatedResources, i);
                    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "MSI-X INT %u Vector 0x%x\n", devContext->MsixCount, resourceRawItem->u.MessageInterrupt.Raw.Vector);

                    // Create an interrupt object that will later be associated with thevdevice's interrupt resource and connected by the Framework to our ISR.
                    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&objAttributes, INT_CONTEXT);

                    WDF_INTERRUPT_CONFIG_INIT(&interruptConfig, MemxEvtInterruptIsr, MemxEvtInterruptDpc);
                    interruptConfig.EvtInterruptEnable = MemxEvtInterruptEnable;
                    interruptConfig.EvtInterruptDisable = MemxEvtInterruptDisable;
                    interruptConfig.InterruptRaw = resourceRawItem;
                    interruptConfig.InterruptTranslated = resourceTranslateItem;
                    interruptConfig.AutomaticSerialization = TRUE;

                    for (j = 0; j < IntrRegisterCnt; j++) {
                        status = WdfInterruptCreate(Device, &interruptConfig, &objAttributes, &devContext->WdfInterrupt[devContext->MsixCount]);
                        if (!NT_SUCCESS(status)) {
                            TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER, "WdfInterruptCreate MSIX %d failed 0x%0x\n", devContext->MsixCount, status);
                        }
                        else
                        {
                            PINT_CONTEXT  IntContext = GetINTContext(devContext->WdfInterrupt[devContext->MsixCount]);
                            IntContext->msixIndex = devContext->MsixCount;
                        }
                        devContext->MsixCount++;
                    }
                    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER,  "Resource %lu: %lu Interrupt Registered\n", i, devContext->MsixCount);
                }
                else
                {
                    TraceEvents(TRACE_LEVEL_WARNING, TRACE_DRIVER, "Legacy INT Allocated: Not Support\n");
                }
                break;
            }
            case CmResourceTypePort: {
                TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER,  "Resource %lu: Port Flag 0x%x\n", i, descriptor->Flags);
                break;
            }
            case CmResourceTypeDma: {
                TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER,  "Resource %lu: DMA Flag 0x%x\n", i, descriptor->Flags);
                break;
            }
            case CmResourceTypeBusNumber: {
                TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER,  "Resource %lu: BusNumber Flag 0x%x\n", i, descriptor->Flags);
                break;
            }
            case CmResourceTypeMemoryLarge: {
                TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER,  "Resource %lu: MemLarge Flag 0x%x\n", i, descriptor->Flags);
                break;
            }
            case CmResourceTypeNonArbitrated: {
                TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER,  "Resource %lu: NonArbitrated Flag 0x%x\n", i, descriptor->Flags);
                break;
            }
            case CmResourceTypeDevicePrivate: {
                TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER,  "Resource %lu: DevicePrivate Flag 0x%x\n", i, descriptor->Flags);
                break;
            }
            case CmResourceTypePcCardConfig: {
                TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER,  "Resource %lu: PcCardConfig Flag 0x%x\n", i, descriptor->Flags);
                break;
            }
            default: {
                TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER,  "Resource %lu: Unhandled resource type 0x%0x\n", i, descriptor->Type);
                break;
            }
        }
    }

    status = IdentifyBarMappingType(Device);

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "%!FUNC! Exit 0x%0x", status);

    return status;
}

NTSTATUS MemxEvtDeviceReleaseHardware(WDFDEVICE Device, WDFCMRESLIST ResourcesTranslated)
{
    PAGED_CODE();
    PDEVICE_CONTEXT devContext;
    NTSTATUS        status;
    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "%!FUNC! Entry");

    UNREFERENCED_PARAMETER(ResourcesTranslated);

    devContext = GetDeviceContext(Device);

    if (devContext->BarInfo.BarMode != MEMXBAR_NOTVALID) {
        for(UCHAR chip_id = CHIP_ID0; chip_id < devContext->hw_info.chip.total_chip_cnt; chip_id++){
            // DebugLog Disable
            status = MemxUtilXflowWrite(devContext, (UCHAR)chip_id, MEMX_DBGLOG_CONTROL_BASE, MEMX_DBGLOG_CTRL_ENABLE_OFS, 0x0, TO_CONFIG_OUTPUT);
            // DebugLog Buffer Address
            status = MemxUtilXflowWrite(devContext, (UCHAR)chip_id, MEMX_DBGLOG_CONTROL_BASE, MEMX_DBGLOG_CTRL_BUFFERADDR_OFS, MEMX_DGBLOG_ADDRESS_DEFAULT, TO_CONFIG_OUTPUT);
            // DebugLog Buffer Size
            status = MemxUtilXflowWrite(devContext, (UCHAR)chip_id, MEMX_DBGLOG_CONTROL_BASE, MEMX_DBGLOG_CTRL_BUFFERSIZE_OFS, MEMX_DGBLOG_SIZE_DEFAULT, TO_CONFIG_OUTPUT);
            // DebugLog Buffer Write Pointer address
            status = MemxUtilXflowWrite(devContext, (UCHAR)chip_id, MEMX_DBGLOG_CONTROL_BASE, MEMX_DBGLOG_CTRL_WPTRADDR_OFS, MEMX_DGBLOG_WPTR_DEFAULT, TO_CONFIG_OUTPUT);
            // DebugLog Buffer Read Pointer address
            status = MemxUtilXflowWrite(devContext, (UCHAR)chip_id, MEMX_DBGLOG_CONTROL_BASE, MEMX_DBGLOG_CTRL_RPTRADDR_OFS, MEMX_DGBLOG_RPTR_DEFAULT, TO_CONFIG_OUTPUT);

            // RemoteCommand Control
            status = MemxUtilXflowWrite(devContext, (UCHAR)chip_id, MEMX_DBGLOG_CONTROL_BASE, MEMX_RMTCMD_CMDADDR_OFS, MEMX_RMTCMD_COMMAND_DEFAULT, TO_CONFIG_OUTPUT);
            // RemoteCommand Parameter
            status = MemxUtilXflowWrite(devContext, (UCHAR)chip_id, MEMX_DBGLOG_CONTROL_BASE, MEMX_RMTCMD_PARAMADDR_OFS, MEMX_RMTCMD_PARAMTER_DEFAULT, TO_CONFIG_OUTPUT);
            // RemoteCommand Parameter2
            status = MemxUtilXflowWrite(devContext, (UCHAR)chip_id, MEMX_DBGLOG_CONTROL_BASE, MEMX_RMTCMD_PARAM2ADDR_OFS, 0, TO_CONFIG_OUTPUT);

            // Admin Command Disable
            status = MemxUtilXflowWrite(devContext, (UCHAR)chip_id, MEMX_CHIP_ADMIN_TASK_EN_ADR, 0, 0x0, TO_CONFIG_OUTPUT);

            // Clear DVFS Address Setting
            status = MemxUtilXflowWrite(devContext, (UCHAR)chip_id, MEMX_DBGLOG_CONTROL_BASE, MEMX_DVFS_MPU_UTI_ADDR, 0x0, TO_CONFIG_OUTPUT);
        }
    }

    for (ULONG index = 0; index < MAX_BAR; index++)
    {
        if (devContext->Bar[index].BaseAddress)
        {
            TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER,  "BAR %d Release\n", index);
            MmUnmapIoSpace(devContext->Bar[index].MappingAddress, devContext->Bar[index].Length);
            RtlZeroMemory(&devContext->Bar[index], sizeof(BAR_CONTEXT));
        }
    }

    devContext->BarInfo.BarMode = MEMXBAR_NOTVALID;
    devContext->BarInfo.XflowVbufOffset = 0;
    devContext->BarInfo.XflowConfOffset = 0;
    devContext->BarInfo.SramIdx = MAX_BAR;
    devContext->BarInfo.XflowVbufIdx = MAX_BAR;
    devContext->BarInfo.XflowConfIdx = MAX_BAR;
    devContext->MsixCount = 0;
    //
    // Note that we don't have to do anything in this function to disconnect
    // or "return" our interrupt resource. WDF will automatically disconect
    // our ISR from any interrupts.  Also, interrupts from the device have
    // already been disabled at this point, because EvtDeviceInterruptDisable
    // was called before this callback.
    //

    return STATUS_SUCCESS;
}

#pragma warning(suppress: 26812)  // "Prefer 'enum class' over 'enum'"
NTSTATUS MemxEvtDeviceD0Entry(WDFDEVICE Device, WDF_POWER_DEVICE_STATE PreviousState)
{
    PAGED_CODE();
    NTSTATUS status = STATUS_SUCCESS;
    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "%!FUNC! Entry");

    UNREFERENCED_PARAMETER(PreviousState);
    PDEVICE_CONTEXT          devContext;
    devContext = GetDeviceContext((WDFDEVICE)Device);
    return status;
}

NTSTATUS MemxEvtDeviceD0Exit(WDFDEVICE Device, WDF_POWER_DEVICE_STATE TargetState)
{
    PAGED_CODE();
    NTSTATUS status = STATUS_SUCCESS;
    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "%!FUNC! Entry");

    UNREFERENCED_PARAMETER(TargetState);
    PDEVICE_CONTEXT          devContext;
    devContext = GetDeviceContext((WDFDEVICE)Device);
    return status;
}

NTSTATUS MemxEvtInterruptEnable(WDFINTERRUPT Interrupt, WDFDEVICE Device)
{

    PDEVICE_CONTEXT          devContext;
    PINT_CONTEXT             IntContext;
    devContext = GetDeviceContext((WDFDEVICE)Device);
    IntContext = GetINTContext(Interrupt);
    if (IntContext->msixIndex == 0)
    {
        TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "%!FUNC! Entry with msix %u INT ", IntContext->msixIndex);
    }

    return STATUS_SUCCESS;
}

NTSTATUS MemxEvtInterruptDisable(WDFINTERRUPT Interrupt, WDFDEVICE Device)
{
    PDEVICE_CONTEXT          devContext;
    PINT_CONTEXT             IntContext;
    devContext = GetDeviceContext((WDFDEVICE)Device);
    IntContext = GetINTContext(Interrupt);
    if (IntContext->msixIndex == 0)
    {
        TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "%!FUNC! Entry with msix %u INT ", IntContext->msixIndex);
    }

    return STATUS_SUCCESS;
}

NTSTATUS MemxEvtPostInterruptEnable(WDFDEVICE Device, WDF_POWER_DEVICE_STATE PreviousState)
{
    PAGED_CODE();
    UNREFERENCED_PARAMETER(PreviousState);
    PDEVICE_CONTEXT     devContext;
    NTSTATUS            status = STATUS_SUCCESS;

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "%!FUNC! Entry");
    devContext = GetDeviceContext((WDFDEVICE)Device);

    if (devContext->BarInfo.BarMode != MEMXBAR_NOTVALID) {
        status = MemxUtilXflowDownloadFirmware(devContext);
    }
    else {
        status = STATUS_UNSUCCESSFUL;
    }

    if (!NT_SUCCESS(status)) {
        WdfDeviceSetFailed(Device, WdfDeviceFailedNoRestart);
    }

    return status;
}

NTSTATUS MemxEvtPreInterruptDisable(WDFDEVICE Device, WDF_POWER_DEVICE_STATE TargetState)
{
    UNREFERENCED_PARAMETER(TargetState);
    PDEVICE_CONTEXT devContext;
    NTSTATUS        status = STATUS_SUCCESS;

    devContext = GetDeviceContext((WDFDEVICE)Device);
    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "%!FUNC! Entry");

    return status;
}

BOOLEAN MemxEvtInterruptIsr(WDFINTERRUPT Interrupt, ULONG MessageID)
{
    BOOLEAN         result      = TRUE;
    PINT_CONTEXT    IntContext  = GetINTContext(Interrupt);

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "%!FUNC! Entry Receive msix INT %u with Message ID %u", IntContext->msixIndex, MessageID);

    // If your driver supports message-signaled interrupts (MSIs), you must use DIRQL interrupt handling.
    // Our device DID cause this interrupt, so we will return TRUE to the Windows Interrupt Dispatcher
    if (IntContext->msixIndex < NUM_OF_MSIX_USED)
    {   // Queue a DpcForIsr to return the data to the user and notify them of this state change
        result = TRUE;
        WdfInterruptQueueDpcForIsr(Interrupt);
    }
    else
    {
        result = FALSE;
    }

    return result;
}

static NTSTATUS MemxInitChipInfo(PDEVICE_CONTEXT devContext, pcie_fw_cmd_format_t* buffer) {
    UCHAR           curr_mpu_group_id   = 0;
    UCHAR           curr_chip_count     = 0;
    UCHAR           chip_id             = 0;
    NTSTATUS        status              = STATUS_SUCCESS;
    struct fw_hw_info_pkt *pHwInfo;
    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "--> %!FUNC!");

    if (!devContext || !buffer) {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER, "<-- %!FUNC!: ptr is NULL");
        status = STATUS_INVALID_PARAMETER;
    }
    else
    {
        pHwInfo  = (struct fw_hw_info_pkt *)(&buffer->data[0]);
        devContext->hw_info.chip.total_chip_cnt = (UCHAR)(pHwInfo->total_chip_cnt & 0xff);
        devContext->hw_info.chip.generation = (USHORT)pHwInfo->chip_generation;
        for (chip_id = 0; chip_id < MAX_SUPPORT_CHIP_NUM; chip_id++)
        {
            devContext->hw_info.chip.roles[chip_id] = pHwInfo->chip_role[chip_id];
            devContext->hw_info.fw.ingress_dcore_mapping_sram_base[chip_id] = pHwInfo->igr_buf_sram_base_addr;
            devContext->hw_info.fw.egress_dcore_mapping_sram_base[chip_id] = pHwInfo->egr_pbuf_sram_base_addr[chip_id];
            devContext->hw_info.fw.egress_dcore_rx_dma_buffer_offset[chip_id] = pHwInfo->egr_dst_buf_start_addr[chip_id];
            TraceEvents(TRACE_LEVEL_VERBOSE, TRACE_DRIVER, "Chip %d Role: 0x%x", chip_id, devContext->hw_info.chip.roles[chip_id]);
            TraceEvents(TRACE_LEVEL_VERBOSE, TRACE_DRIVER, "Chip %d Engress Sram Base 0x%x", chip_id, devContext->hw_info.fw.egress_dcore_mapping_sram_base[chip_id]);
            TraceEvents(TRACE_LEVEL_VERBOSE, TRACE_DRIVER, "Chip %d Ingress Sram Base 0x%x", chip_id, devContext->hw_info.fw.ingress_dcore_mapping_sram_base[chip_id]);
            TraceEvents(TRACE_LEVEL_VERBOSE, TRACE_DRIVER, "Chip %d Rx DMA Buffer Offset 0x%x", chip_id, devContext->hw_info.fw.egress_dcore_rx_dma_buffer_offset[chip_id]);
        }

        for (chip_id = 0; chip_id < MAX_SUPPORT_CHIP_NUM; chip_id++) {
            switch (devContext->hw_info.chip.roles[chip_id]) {
                case ROLE_SINGLE: {
                    devContext->hw_info.chip.groups[curr_mpu_group_id].input_chip_id = chip_id;
                    devContext->hw_info.chip.groups[curr_mpu_group_id].output_chip_id = chip_id;
                    curr_mpu_group_id++;
                    curr_chip_count++;
                } break;
                case ROLE_MULTI_FIRST: {
                    devContext->hw_info.chip.groups[curr_mpu_group_id].input_chip_id = chip_id;
                    curr_chip_count++;
                } break;
                case ROLE_MULTI_LAST: {
                    devContext->hw_info.chip.groups[curr_mpu_group_id].output_chip_id = chip_id;
                    curr_mpu_group_id++;
                    curr_chip_count++;
                } break;
                case ROLE_MULTI_MIDDLE: {
                    curr_chip_count++;
                } break;
                default:
                    // The first unknow ROLE_UNCONFIGURED which means all chip
                    // already scan finsh! we can just break the loop early.
                    break;
            }
        }
        devContext->hw_info.chip.group_count = curr_mpu_group_id;
        devContext->hw_info.chip.curr_config_chip_count = curr_chip_count;
        TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "Total mpu group count %d.", devContext->hw_info.chip.group_count);
        TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "Total chip count %d.", devContext->hw_info.chip.total_chip_cnt);
        TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "Current config chip count %d.", devContext->hw_info.chip.curr_config_chip_count);
        TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "<-- %!FUNC!: Init Chip Info Success.");
    }


    return status;
}

NTSTATUS MemxProcessRxDone(PDEVICE_CONTEXT devContext, PINT_CONTEXT IntContext)
{
    NTSTATUS    status          = STATUS_SUCCESS;
    PULONG      pBuffer         = NULL;
    ULONG_PTR   chipIndicter    = MSIX_ERROR_INDICTOR_VAL;
    ULONG       RxMsixIndex     = IntContext->msixIndex;
    WDFREQUEST  Request;

    WdfSpinLockAcquire(devContext->IoQueueLockHandle[QUEUE_IDX_READ]);

    status = WdfIoQueueRetrieveNextRequest(devContext->ReadPendingQueue, &Request);

    if (!NT_SUCCESS(status))
    {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER, "Retrieve Read NextRequest failed Status = 0x%0x\n", status);

        BOOLEAN full = (((devContext->IndicatorTail + 1) % QUEUE_SIZE) == (devContext->IndicatorHeader));
        if (full)
        {
            TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER, "Indicator Queue Full Tail %d Header %d\n", devContext->IndicatorTail, devContext->IndicatorHeader);
            status = STATUS_INSUFFICIENT_RESOURCES;
        }
        else
        {
            devContext->IndicatorQueue[devContext->IndicatorTail] = GET_CHIPID_FROM_MSIX(RxMsixIndex);
            TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "Put Indicator %d into Queue[%d]\n", GET_CHIPID_FROM_MSIX(RxMsixIndex), devContext->IndicatorTail);
            devContext->IndicatorTail = (devContext->IndicatorTail + 1) % QUEUE_SIZE;
        }
    }
    else
    {
        WDF_REQUEST_PARAMETERS params;
        WDF_REQUEST_PARAMETERS_INIT(&params);
        WdfRequestGetParameters(Request, &params);

        status = WdfRequestRetrieveOutputBuffer(Request, params.Parameters.Read.Length, &pBuffer, NULL);
        if (!NT_SUCCESS(status)) {
            TraceEvents(TRACE_LEVEL_WARNING, TRACE_QUEUE,
                        "<-- %!FUNC!: Get Rx buffer fail!!!\n");
            WdfRequestCompleteWithInformation(Request, status, chipIndicter);
        }
        else
        {
            chipIndicter = GET_CHIPID_FROM_MSIX(RxMsixIndex);

            PUCHAR pbSrcBuf     = (PUCHAR) (devContext->CommonBufferBaseDriver[BUFFER_IDX_READ]);
            PUCHAR pbDesBuf     = (PUCHAR) pBuffer;
            ULONG  copy_length  = 0;

            pbSrcBuf += devContext->hw_info.fw.egress_dcore_rx_dma_buffer_offset[chipIndicter];
            pbDesBuf += devContext->hw_info.fw.egress_dcore_rx_dma_buffer_offset[chipIndicter];
            PULONG plBuffer = (PULONG) pbSrcBuf;
            ULONG ofmap_flows_triggered_bits    = plBuffer[2];
            ULONG output_buf_cnt = plBuffer[5];
            while (ofmap_flows_triggered_bits) {
                plBuffer = (PULONG)(pbSrcBuf + copy_length);
                ULONG hw_flow_id = plBuffer[14] & 0x1f;
                ULONG hw_buffer_count = plBuffer[15];
                copy_length += (MEMX_MPUIO_COMMON_HEADER_SIZE + hw_buffer_count);
                if (output_buf_cnt & (1 << hw_flow_id)) {
                    output_buf_cnt &= ~(1 << hw_flow_id);
                } else {
                    ofmap_flows_triggered_bits &= (ofmap_flows_triggered_bits - 1);
                }
            }

            if((devContext->hw_info.fw.egress_dcore_rx_dma_buffer_offset[chipIndicter] + copy_length) >= params.Parameters.Read.Length)
            {//if unexpected out of range error, just move data in size.
                pbSrcBuf    = (PUCHAR) (devContext->CommonBufferBaseDriver[BUFFER_IDX_READ]);
                pbDesBuf    = (PUCHAR) pBuffer;
                copy_length = (ULONG)  params.Parameters.Read.Length;
            }

            RtlCopyMemory(pbDesBuf, pbSrcBuf, copy_length);
            TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "<-- %!FUNC!: Rx ofmap with data size %u done\n", copy_length);

            WdfRequestCompleteWithInformation(Request, status, chipIndicter);
        }
    }

    WdfSpinLockRelease(devContext->IoQueueLockHandle[QUEUE_IDX_READ]);

    return status;
}

NTSTATUS MemxProcessTxDone(PDEVICE_CONTEXT devContext, PINT_CONTEXT IntContext)
{
    NTSTATUS    status      = STATUS_SUCCESS;
    PULONG      pBuffer     = NULL;
    ULONG_PTR   information = MSIX_ERROR_INDICTOR_VAL;
    ULONG       TxMsixIndex = IntContext->msixIndex;
    WDFREQUEST  request;

    WdfSpinLockAcquire(devContext->IoQueueLockHandle[QUEUE_IDX_WRITE]);

    status = WdfIoQueueRetrieveNextRequest(devContext->WritePendingQueue, &request);

    if (!NT_SUCCESS(status)) {
        TraceEvents(TRACE_LEVEL_WARNING, TRACE_DRIVER, "Retrieve write NextRequest failed Status = 0x%0x\n", status);
    }
    else
    {
        WDF_REQUEST_PARAMETERS params;
        WDF_REQUEST_PARAMETERS_INIT(&params);
        WdfRequestGetParameters(request, &params);

        status = WdfRequestRetrieveInputBuffer(request, params.Parameters.Write.Length, &pBuffer, NULL);
        if (!NT_SUCCESS(status)) {
            TraceEvents(TRACE_LEVEL_WARNING, TRACE_QUEUE,
                        "<-- %!FUNC!: Get Tx buffer fail!!!\n");
            WdfRequestCompleteWithInformation(request, status, information);
        }
        else
        {
            information = params.Parameters.Write.Length;
            pBuffer[0] = GET_CHIPID_FROM_MSIX(TxMsixIndex);
            WdfRequestCompleteWithInformation(request, status, information);
            TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "<-- %!FUNC!: Tx ifmap data done\n");
        }
    }

    WdfSpinLockRelease(devContext->IoQueueLockHandle[QUEUE_IDX_WRITE]);

    return status;
}

NTSTATUS MemxProcessFwAckDone(PDEVICE_CONTEXT devContext, PINT_CONTEXT IntContext)
{
    NTSTATUS status = STATUS_SUCCESS;

    WdfSpinLockAcquire(devContext->IoQueueLockHandle[QUEUE_IDX_IOCTL]);

    if (IntContext->msixIndex != Firmware_MSIx_Acknowledge_Notification) {
        TraceEvents(TRACE_LEVEL_WARNING, TRACE_DRIVER, "InVaild FW ACK MSIx idx(%d)\n", IntContext->msixIndex);
        status =  STATUS_INVALID_PARAMETER;
    }
    else
    {
        WDFREQUEST request;
        switch(devContext->deviceState)
        {
            case DEVICE_INITIAL:
                devContext->deviceState = DEVICE_FWDONE;
                MemxUtilSramWrite(devContext, (MEMX_DBGLOG_CONTROL_BASE + MEMX_DVFS_MPU_UTI_ADDR), MEMX_GET_DVFS_UTIL_BUS_ADDR);
                TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "Initial Firmware Boot Done Set DVFS Address mapping\n");
                status = MemxUtilSendFwCommand(devContext, PCIE_CMD_INIT_HOST_BUF_MAPPING, 8, CHIP_ID0);
                TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "Initial Firmware Boot Done Send INIT_HOST_BUF_MAPPING\n");
                break;
            case DEVICE_FWDONE:
                if (IntContext->flag) {
                    pcie_fw_cmd_format_t* fw_cmd_result = (pcie_fw_cmd_format_t*)devContext->mmap_fw_command_buffer_base;
                    MemxInitChipInfo(devContext, fw_cmd_result);
                    IntContext->flag = 0;
                    devContext->deviceState = DEVICE_RUNNING;
                    for(UCHAR chip_id = CHIP_ID0; chip_id < devContext->hw_info.chip.total_chip_cnt; chip_id++){
                        // DebugLog Buffer Address
                        status = MemxUtilXflowWrite(devContext, (UCHAR)chip_id, MEMX_DBGLOG_CONTROL_BASE, MEMX_DBGLOG_CTRL_BUFFERADDR_OFS, MEMX_GET_CHIP_DBGLOG_BUFFER_BUS_ADDR(chip_id), TO_CONFIG_OUTPUT);
                        // DebugLog Buffer Size
                        status = MemxUtilXflowWrite(devContext, (UCHAR)chip_id, MEMX_DBGLOG_CONTROL_BASE, MEMX_DBGLOG_CTRL_BUFFERSIZE_OFS, MEMX_DBGLOG_CHIP_BUFFER_SIZE(chip_id), TO_CONFIG_OUTPUT);
                        // DebugLog Buffer Write Pointer address
                        status = MemxUtilXflowWrite(devContext, (UCHAR)chip_id, MEMX_DBGLOG_CONTROL_BASE, MEMX_DBGLOG_CTRL_WPTRADDR_OFS, MEMX_GET_CHIP_DBGLOG_WRITER_PTR_BUS_ADDR(chip_id), TO_CONFIG_OUTPUT);
                        // DebugLog Buffer Read Pointer address
                        status = MemxUtilXflowWrite(devContext, (UCHAR)chip_id, MEMX_DBGLOG_CONTROL_BASE, MEMX_DBGLOG_CTRL_RPTRADDR_OFS, MEMX_GET_CHIP_DBGLOG_READ_PTR_BUS_ADDR(chip_id), TO_CONFIG_OUTPUT);
                        // DebugLog Enable
                        status = MemxUtilXflowWrite(devContext, (UCHAR)chip_id, MEMX_DBGLOG_CONTROL_BASE, MEMX_DBGLOG_CTRL_ENABLE_OFS, 0x1, TO_CONFIG_OUTPUT);

                        // RemoteCommand Control
                        status = MemxUtilXflowWrite(devContext, (UCHAR)chip_id, MEMX_DBGLOG_CONTROL_BASE, MEMX_RMTCMD_CMDADDR_OFS, MEMX_GET_CHIP_RMTCMD_COMMAND_BUS_ADDR(chip_id), TO_CONFIG_OUTPUT);
                        // RemoteCommand Parameter
                        status = MemxUtilXflowWrite(devContext, (UCHAR)chip_id, MEMX_DBGLOG_CONTROL_BASE, MEMX_RMTCMD_PARAMADDR_OFS, MEMX_GET_CHIP_RMTCMD_PARAM_BUS_ADDR(chip_id), TO_CONFIG_OUTPUT);
                        // RemoteCommand Parameter2
                        status = MemxUtilXflowWrite(devContext, (UCHAR)chip_id, MEMX_DBGLOG_CONTROL_BASE, MEMX_RMTCMD_PARAM2ADDR_OFS, MEMX_GET_CHIP_RMTCMD_PARAM2_BUS_ADDR(chip_id), TO_CONFIG_OUTPUT);

                        // Admin Command Enable
                        status = MemxUtilXflowWrite(devContext, (UCHAR)chip_id, MEMX_CHIP_ADMIN_TASK_EN_ADR, 0, 0x1, TO_CONFIG_OUTPUT);
                    }
                    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "Initial Send PCIE_CMD_GET_HW_INFO Done\n");
                } else {
                    devContext->BufMappingIdx = (devContext->BufMappingIdx + 1);
                    if(devContext->BufMappingIdx < MAX_NUM_COMBUFFER)
                    {
                        status = MemxUtilSendFwCommand(devContext, PCIE_CMD_INIT_HOST_BUF_MAPPING, 8, CHIP_ID0);
                        TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "Send INIT_HOST_BUF_MAPPING %d\n", devContext->BufMappingIdx);
                    }
                    else
                    {
                        devContext->BufMappingIdx = 0;
                        IntContext->flag = BIT(1);
                        status = MemxUtilSendFwCommand(devContext, PCIE_CMD_GET_HW_INFO, DEF_BYTE(256), CHIP_ID0);
                        TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "Initial Send INIT_HOST_BUF_MAPPING Done Send PCIE_CMD_GET_HW_INFO\n");
                    }
                }
                break;
            case DEVICE_RUNNING:
                status = WdfIoQueueRetrieveNextRequest(devContext->IoctlPendingQueue, &request);
                if (!NT_SUCCESS(status)) {
                    TraceEvents(TRACE_LEVEL_WARNING, TRACE_DRIVER, "RetrieveNextRequest failed Status = 0x%0x\n", status);
                }
                else
                {
                    WDF_REQUEST_PARAMETERS params;
                    WDF_REQUEST_PARAMETERS_INIT(&params);
                    WdfRequestGetParameters(request, &params);
                    ULONG_PTR information = params.Parameters.DeviceIoControl.InputBufferLength;

                    if (params.Parameters.DeviceIoControl.IoControlCode == MEMX_CONFIG_MPU_GROUP) {
                        PVOID buffer = NULL;
                        size_t OutputBufferLength = params.Parameters.DeviceIoControl.OutputBufferLength;
                        information = OutputBufferLength;
                        status = WdfRequestRetrieveOutputBuffer(request, OutputBufferLength, &buffer, NULL);
                        if (!NT_SUCCESS(status)) {
                            TraceEvents(TRACE_LEVEL_WARNING, TRACE_DRIVER, "WdfRequestRetrieveOutputBuffer0x%0x", status);
                        }
                        else
                        {
                            // 1. upadte hw info with new config mpu group
                            pcie_fw_cmd_format_t* fw_cmd_result = (pcie_fw_cmd_format_t*) devContext->mmap_fw_command_buffer_base;
                            MemxInitChipInfo(devContext, fw_cmd_result);
                            TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "Update Chip info after send PCIE_CMD_CONFIG_MPU_GROUP Done\n");

                            // 2. copy new hw info to user
                            RtlCopyMemory(buffer, &devContext->hw_info, sizeof(struct hw_info));
                        }
                    } else if (params.Parameters.DeviceIoControl.IoControlCode == MEMX_DOWNLOAD_FIRMWARE){
                         pcie_fw_cmd_format_t* fw_cmd_result = (pcie_fw_cmd_format_t*) devContext->mmap_fw_command_buffer_base;
                         TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "Downlaod Firmware status 0x%0x", fw_cmd_result->data[0]);
                         if(fw_cmd_result->data[0] != 0){
                            status = STATUS_UNSUCCESSFUL;
                         }
                    }

                    WdfRequestCompleteWithInformation(request, status, information);
                    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "FW Command  0x%x Done\n", params.Parameters.DeviceIoControl.IoControlCode);
                }
                break;
            default:
                TraceEvents(TRACE_LEVEL_WARNING, TRACE_DRIVER, "InVaild Device State(%d)\n", devContext->deviceState);
                status =  STATUS_INVALID_PARAMETER;
                break;
        }
    }

    WdfSpinLockRelease(devContext->IoQueueLockHandle[QUEUE_IDX_IOCTL]);

    return status;
}

VOID MemxEvtInterruptDpc(WDFINTERRUPT Interrupt, WDFOBJECT Device)
{
    NTSTATUS        status      = STATUS_SUCCESS;
    PDEVICE_CONTEXT devContext  = GetDeviceContext((WDFDEVICE)Device);
    PINT_CONTEXT    IntContext  = GetINTContext(Interrupt);

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "%!FUNC! Entry with msix %u INT", IntContext->msixIndex);

    if (IntContext->msixIndex >= NUM_OF_MSIX_USED) {
        TraceEvents(TRACE_LEVEL_WARNING, TRACE_DRIVER, "<-- %!FUNC!: Unused MSIX INT\n");
    }
    else
    {
        if (IntContext->msixIndex != Firmware_MSIx_Acknowledge_Notification) {
            if (IntContext->msixIndex & BIT(0)) {   //Egress_Dcore_Done_Notification
                status = MemxProcessRxDone(devContext, IntContext);
                TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_QUEUE, "<-- %!FUNC!: Chip %d MemxProcessRxDone, status(0x%0x)\n", (IntContext->msixIndex - 1) >> 1, status);
            } else { //Ingress_Dcore_Done_Notification
                status = MemxProcessTxDone(devContext, IntContext);
                TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_QUEUE, "<-- %!FUNC!: Chip %d MemxProcessTxDone, status(0x%0x)\n", (IntContext->msixIndex - 1) >> 1, status);
            }
        } else {
            status = MemxProcessFwAckDone(devContext, IntContext);
            TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_QUEUE, "<-- %!FUNC!: MemxProcessFwAckDone, status(0x%0x)\n", status);
        }
    }
}