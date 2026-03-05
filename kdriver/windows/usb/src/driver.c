/**
 * @file driver.c
 *
 * @brief This file contains the driver entry points and callbacks.
 * @version 0.1
 * @date 2023
 *
 * @copyright Copyright (c) 2023 MemryX. Inc. All Rights Reserved.
 *
 */
#include "private.h"
#include "driver.tmh"

#ifdef ALLOC_PRAGMA
    #pragma alloc_text(INIT, DriverEntry)
    #pragma alloc_text(PAGE, MemxEvtDriverContextCleanup)
#endif

NTSTATUS DriverEntry(PDRIVER_OBJECT  DriverObject, PUNICODE_STRING RegistryPath)
{
    NTSTATUS                status = STATUS_SUCCESS;
    WDF_DRIVER_CONFIG       config;
    WDF_OBJECT_ATTRIBUTES   attributes;

    // Initialize WPP Tracing
    WPP_INIT_TRACING(DriverObject, RegistryPath);

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "%!FUNC! Entry");
#if DBG
    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "USB kdriver Built %s %s\n", __DATE__, __TIME__);
#endif

    // Register a cleanup callback so that we can call WPP_CLEANUP when the framework driver object is deleted during driver unload.
    WDF_OBJECT_ATTRIBUTES_INIT(&attributes);
    attributes.EvtCleanupCallback = MemxEvtDriverContextCleanup;
    WDF_DRIVER_CONFIG_INIT(&config, MemxEvtDeviceAdd);

    // Create our WDFDRIVER object
    status = WdfDriverCreate(DriverObject, RegistryPath, &attributes, &config, WDF_NO_HANDLE);
    if (!NT_SUCCESS(status)) {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER, "WdfDriverCreate failed %!STATUS!", status);
        WPP_CLEANUP(DriverObject);
    }
    else
    {
        TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "%!FUNC! Exit %u", status);
    }

    return status;
}

VOID MemxEvtDriverContextCleanup(WDFOBJECT DriverObject)
{
    UNREFERENCED_PARAMETER(DriverObject);

    PAGED_CODE();

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "%!FUNC! Entry");

    // Stop WPP Tracing
    WPP_CLEANUP(WdfDriverWdmGetDriverObject((WDFDRIVER)DriverObject));
}
