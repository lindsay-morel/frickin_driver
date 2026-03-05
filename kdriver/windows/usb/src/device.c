/**
 * @file device.c
 *
 * @brief This file contains the device entry points and callbacks.
 * @version 0.1
 * @date 2023
 *
 * @copyright Copyright (c) 2023 MemryX. Inc. All Rights Reserved.
 *
 */
#include "private.h"
#include "device.tmh"

static NTSTATUS GetUsbPID(WDFDEVICE  Device, PWCHAR PID);
static NTSTATUS CreateUsbDeviceInterface(WDFDEVICE device);
static NTSTATUS ReadFdoRegistryKeyValue(WDFDRIVER Driver, LPWSTR Name, PULONG Value);
static NTSTATUS SetPowerPolicy(WDFDEVICE Device);
static NTSTATUS PowerPolicySuspendEnable(WDFDEVICE Device);
static NTSTATUS ReadDeviceDescriptorsAndConfig(WDFDEVICE Device);
static NTSTATUS ReadConfigurationDescriptor(WDFDEVICE Device);
static NTSTATUS SelectSettingAndConfigPipes(WDFDEVICE Device);
static NTSTATUS InitialDeviceFirmware(PDEVICE_CONTEXT devContext);
#ifdef ALLOC_PRAGMA
    #pragma alloc_text(PAGE, MemxEvtDeviceAdd)
    #pragma alloc_text(PAGE, MemxEvtDevicePrepareHardware)
    #pragma alloc_text(PAGE, MemxEvtDeviceReleaseHardware)
    #pragma alloc_text (PAGE, MemxEvtPostInterruptEnable)
    #pragma alloc_text(PAGE, MemxEvtDeviceD0Exit)
    #pragma alloc_text(PAGE, MemxEvtDeviceD0Entry)
    #pragma alloc_text(PAGE, MemxEvtDeviceCleanup)
    #pragma alloc_text(PAGE, InitialDeviceFirmware)
    #pragma alloc_text(PAGE, CreateUsbDeviceInterface)
    #pragma alloc_text(PAGE, GetUsbPID)
    #pragma alloc_text(PAGE, SetPowerPolicy)
    #pragma alloc_text(PAGE, PowerPolicySuspendEnable)
    #pragma alloc_text(PAGE, ReadDeviceDescriptorsAndConfig)
    #pragma alloc_text(PAGE, ReadConfigurationDescriptor)
    #pragma alloc_text(PAGE, SelectSettingAndConfigPipes)
    #pragma alloc_text(PAGE, ReadFdoRegistryKeyValue)
#endif

#define USB_PID_LEN 8

NTSTATUS GetUsbPID(WDFDEVICE Device, PWCHAR PID)
{
    NTSTATUS    status      = STATUS_SUCCESS;
    WCHAR       hwID[256]   = { 0 };
    ULONG       returnSize  = 0;

    PAGED_CODE();

    status = WdfDeviceQueryProperty(Device, DevicePropertyHardwareID, sizeof(hwID), hwID, &returnSize);

    if (NT_SUCCESS(status)) {
        PWCHAR pPID = wcsstr(hwID, L"PID_");

        if (pPID != NULL) {
            RtlCopyMemory((PVOID)PID, (PVOID)pPID, USB_PID_LEN * sizeof(WCHAR));
        } else {
            status = STATUS_UNSUCCESSFUL;
        }
    }

    return status;
}

NTSTATUS CreateUsbDeviceInterface(WDFDEVICE device)
{
    NTSTATUS    status                              = STATUS_SUCCESS;
    WCHAR       usbPID[USB_PID_LEN * sizeof(WCHAR)] = { 0 };

    PAGED_CODE();
    // Get PID of USB, register different interface for mutil chip scenario.
    // After calling WdfDeviceCreate, a driver can obtain device property information by calling WdfDeviceQueryProperty.
    status = GetUsbPID(device, usbPID);
    if (!NT_SUCCESS(status)) {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER, "MemxGetUsbPID failed with Status code 0x%x\n", status);
    } else {
        TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "Get PID %ws\n", usbPID);

        if (wcscmp(usbPID, L"PID_4006") == 0) {
            TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER,  "Register cascade single\n");
            status = WdfDeviceCreateDeviceInterface(device, (LPGUID)&GUID_CLASS_MEMX_CASCADE_SINGLE_USB, NULL);// Reference String
        } else if  (wcscmp(usbPID, L"PID_40FF") == 0) {
            TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER,  "Register cascade single Init\n");
            status = WdfDeviceCreateDeviceInterface(device, (LPGUID)&GUID_CLASS_MEMX_CASCADE_SINGLE_USB, NULL);// Reference String
        } else if (wcscmp(usbPID, L"PID_4007") == 0) {
            TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER,  "Register cascade multi G0 first\n");
            status = WdfDeviceCreateDeviceInterface(device, (LPGUID)&GUID_CLASS_MEMX_CASCADE_MUTLI_G0_FIRST_USB, NULL);// Reference String
        } else if (wcscmp(usbPID, L"PID_4008") == 0) {
            TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER,  "Register cascade multi G0 last\n");
            status = WdfDeviceCreateDeviceInterface(device, (LPGUID)&GUID_CLASS_MEMX_CASCADE_MUTLI_G0_LAST_USB, NULL);// Reference String
        } else if (wcscmp(usbPID, L"PID_4017") == 0) {
            TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER,  "Register cascade multi G1 first\n");
            status = WdfDeviceCreateDeviceInterface(device, (LPGUID)&GUID_CLASS_MEMX_CASCADE_MUTLI_G1_FIRST_USB, NULL);// Reference String
        } else if (wcscmp(usbPID, L"PID_4018") == 0) {
            TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER,  "Register cascade multi G1 last\n");
            status = WdfDeviceCreateDeviceInterface(device, (LPGUID)&GUID_CLASS_MEMX_CASCADE_MUTLI_G1_LAST_USB, NULL);// Reference String
        } else if (wcscmp(usbPID, L"PID_4027") == 0) {
            TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER,  "Register cascade multi G2 first\n");
            status = WdfDeviceCreateDeviceInterface(device, (LPGUID)&GUID_CLASS_MEMX_CASCADE_MUTLI_G2_FIRST_USB, NULL);// Reference String
        } else if (wcscmp(usbPID, L"PID_4028") == 0) {
            TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER,  "Register cascade multi G2 last\n");
            status = WdfDeviceCreateDeviceInterface(device, (LPGUID)&GUID_CLASS_MEMX_CASCADE_MUTLI_G2_LAST_USB, NULL);// Reference String
        } else if (wcscmp(usbPID, L"PID_4037") == 0) {
            TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER,  "Register cascade multi G3 first\n");
            status = WdfDeviceCreateDeviceInterface(device, (LPGUID)&GUID_CLASS_MEMX_CASCADE_MUTLI_G3_FIRST_USB, NULL);// Reference String
        } else if (wcscmp(usbPID, L"PID_4038") == 0) {
            TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER,  "Register cascade multi G3 last\n");
            status = WdfDeviceCreateDeviceInterface(device, (LPGUID)&GUID_CLASS_MEMX_CASCADE_MUTLI_G3_LAST_USB, NULL);// Reference String
        } else {
            TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER,  "Unkown cascade device \n");
            status = STATUS_UNSATISFIED_DEPENDENCIES;
        }
    }

    return status;
}

NTSTATUS ReadFdoRegistryKeyValue(WDFDRIVER Driver, LPWSTR Name, PULONG Value)
{
    NTSTATUS    status;
    WDFKEY      hKey = NULL;
    UNICODE_STRING  valueName;

    UNREFERENCED_PARAMETER(Driver);

    PAGED_CODE();

    status = WdfDriverOpenParametersRegistryKey(WdfGetDriver(), KEY_READ, WDF_NO_OBJECT_ATTRIBUTES, &hKey);

    if (NT_SUCCESS (status)) {
        RtlInitUnicodeString(&valueName,Name);
        status = WdfRegistryQueryULong (hKey, &valueName, Value);
        WdfRegistryClose(hKey);
    }

    return status;
}

NTSTATUS SetPowerPolicy(WDFDEVICE Device)
{
    NTSTATUS status = STATUS_SUCCESS;
    WDF_DEVICE_POWER_POLICY_WAKE_SETTINGS wakeSettings;

    PAGED_CODE();

    WDF_DEVICE_POWER_POLICY_WAKE_SETTINGS_INIT(&wakeSettings);
    status = WdfDeviceAssignSxWakeSettings(Device, &wakeSettings);
    if (!NT_SUCCESS(status)) {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER, "WdfDeviceAssignSxWakeSettings failed  0x%x\n", status);
        goto done;
    }
done:
    return status;
}

NTSTATUS PowerPolicySuspendEnable(WDFDEVICE Device)
{
    NTSTATUS status = STATUS_SUCCESS;
    WDF_DEVICE_POWER_POLICY_IDLE_SETTINGS idleSettings;

    PAGED_CODE();

    WDF_DEVICE_POWER_POLICY_IDLE_SETTINGS_INIT(&idleSettings, IdleCannotWakeFromS0);
    idleSettings.IdleTimeout = 10000; // 10-sec
    status = WdfDeviceAssignS0IdleSettings(Device, &idleSettings);

    if (!NT_SUCCESS(status)) {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER, "WdfDeviceAssignS0IdleSettings failed  0x%x\n", status);
    }

    return status;
}

NTSTATUS ReadDeviceDescriptorsAndConfig(WDFDEVICE Device)
/*++

Routine Description:

    This routine configures the USB device.
    In this routines we get the device descriptor,
    the configuration descriptor and select the
    configuration.

Arguments:

    Device - Handle to a framework device

Return Value:

    NTSTATUS - NT status value.

--*/
{
    NTSTATUS                        status          = STATUS_SUCCESS;
    WDF_USB_DEVICE_CREATE_CONFIG    config;
    WDF_USB_DEVICE_INFORMATION      info;
    PDEVICE_CONTEXT                 pDeviceContext;

    PAGED_CODE();

    // Create a USB device handle so that we can communicate with the
    // underlying USB stack. The WDFUSBDEVICE handle is used to query,
    // configure, and manage all aspects of the USB device.
    // These aspects include device properties, bus properties,
    // and I/O creation and synchronization. We only create device the first
    // the PrepareHardware is called. If the device is restarted by pnp manager
    // for resource rebalance, we will use the same device handle but then select
    // the interfaces again because the USB stack could reconfigure the device on
    // restart.
    pDeviceContext = GetDeviceContext(Device);
    if (pDeviceContext->WdfUsbTargetDevice == NULL) {
        WDF_USB_DEVICE_CREATE_CONFIG_INIT(&config, USBD_CLIENT_CONTRACT_VERSION_602);
        status = WdfUsbTargetDeviceCreateWithParameters(Device, &config, WDF_NO_OBJECT_ATTRIBUTES, &pDeviceContext->WdfUsbTargetDevice);
        if (!NT_SUCCESS(status)) {
            TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER,  "WdfUsbTargetDeviceCreateWithParameters failed with Status code %x\n", status);
            goto done;
        }
    }

    // Retrieve USBD version information, port driver capabilites and device capabilites such as speed, power, etc.
    WDF_USB_DEVICE_INFORMATION_INIT(&info);
    status = WdfUsbTargetDeviceRetrieveInformation(pDeviceContext->WdfUsbTargetDevice, &info);
    if (!NT_SUCCESS(status)) {
        goto done;
    }

    pDeviceContext->IsDeviceHighSpeed = (info.Traits & WDF_USB_DEVICE_TRAIT_AT_HIGH_SPEED) ? TRUE : FALSE;
    pDeviceContext->WaitWakeEnable = info.Traits & WDF_USB_DEVICE_TRAIT_REMOTE_WAKE_CAPABLE;

    // Before calling WdfUsbTargetDeviceQueryUsbCapability, the driver must call WdfUsbTargetDeviceCreateWithParameters to register with the underlying USB driver stack.
    status = WdfUsbTargetDeviceQueryUsbCapability(pDeviceContext->WdfUsbTargetDevice, &GUID_USB_CAPABILITY_DEVICE_CONNECTION_SUPER_SPEED_COMPATIBLE, 0, NULL, NULL);
    if (NT_SUCCESS(status)) {
        pDeviceContext->IsDeviceSuperSpeed = TRUE;
    }

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "DeviceIsHighSpeed: %s\n", pDeviceContext->IsDeviceHighSpeed ? "TRUE" : "FALSE");
    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "IsDeviceRemoteWakeable: %s\n", pDeviceContext->WaitWakeEnable ? "TRUE" : "FALSE");
    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "DeviceIsSuperSpeed: %s\n", pDeviceContext->IsDeviceSuperSpeed ? "TRUE" : "FALSE");

    WdfUsbTargetDeviceGetDeviceDescriptor(pDeviceContext->WdfUsbTargetDevice, &pDeviceContext->UsbDeviceDescriptor);
    if(pDeviceContext->UsbDeviceDescriptor.bNumConfigurations) {
        TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "USB 0x%x VID 0x%x PID 0x%x\n", pDeviceContext->UsbDeviceDescriptor.bcdUSB, pDeviceContext->UsbDeviceDescriptor.idVendor, pDeviceContext->UsbDeviceDescriptor.idProduct);
        TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "DeviceDescriptor Number of Configuration %d\n", pDeviceContext->UsbDeviceDescriptor.bNumConfigurations);
        status = ReadConfigurationDescriptor(Device);
    } else {
        status = STATUS_UNSUCCESSFUL;
    }

done:
    return status;
}

NTSTATUS ReadConfigurationDescriptor(WDFDEVICE Device)
/*++

Routine Description:

    This helper routine reads the configuration descriptor
    for the device in couple of steps.

Arguments:

    Device - Handle to a framework device

Return Value:

    NTSTATUS - NT status value

--*/
{
    NTSTATUS                        status                  = STATUS_SUCCESS;
    WDF_OBJECT_ATTRIBUTES           attributes;
    WDFMEMORY                       memory;
    USBD_STATUS                     usbStatus;
    PDEVICE_CONTEXT                 pDeviceContext;
    USHORT                          size                    = 0;
    PUCHAR                          Offset                  = NULL;

    PAGED_CODE();

    // initialize the variables
    pDeviceContext = GetDeviceContext(Device);

    // 1. Set the ConfigDescriptor pointer to NULL, WdfUsbTargetDeviceRetrieveConfigDescriptor will return the required buffer size.
    status = WdfUsbTargetDeviceRetrieveConfigDescriptor(pDeviceContext->WdfUsbTargetDevice, NULL, &size);
    if (status != STATUS_BUFFER_TOO_SMALL || size == 0) {
        goto done;
    }

    // 2. Allocate buffer space to hold the configuration information. Specify usbdevice as the parent so that it will be freed automatically.
    WDF_OBJECT_ATTRIBUTES_INIT(&attributes);
    attributes.ParentObject = pDeviceContext->WdfUsbTargetDevice;
    status = WdfMemoryCreate(&attributes, NonPagedPoolNx, 'CONF', size, &memory, &pDeviceContext->UsbConfigurationDescriptor);
    if (!NT_SUCCESS(status)) {
        goto done;
    }

    //3. Call WdfUsbTargetDeviceRetrieveConfigDescriptor again, passing it a pointer to the new buffer and the buffer's size.
    status = WdfUsbTargetDeviceRetrieveConfigDescriptor(pDeviceContext->WdfUsbTargetDevice, pDeviceContext->UsbConfigurationDescriptor, &size);
    if (!NT_SUCCESS(status)) {
        goto done;
    }

    usbStatus = USBD_ValidateConfigurationDescriptor(pDeviceContext->UsbConfigurationDescriptor, size , 3 , &Offset , POOL_TAG );
    if (usbStatus != USBD_STATUS_SUCCESS) {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER, "USBD_ValidateConfigurationDescriptor failed with Status code %x and at the offset %p\n", status , Offset );
        goto done;
    }

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "ConfigurationDescriptor Number of Interface %d\n", pDeviceContext->UsbConfigurationDescriptor->bNumInterfaces);
    if(pDeviceContext->UsbConfigurationDescriptor->bNumInterfaces){
        status = SelectSettingAndConfigPipes(Device);
    } else {
        status = STATUS_UNSUCCESSFUL;
    }

done:
    return status;
}

NTSTATUS SelectSettingAndConfigPipes(WDFDEVICE Device)
/*++

Routine Description:

    This helper routine selects the configuration, interface and
    creates a context for every pipe (end point) in that interface.

Arguments:

    Device - Handle to a framework device

Return Value:

    NT status value

--*/
{
    NTSTATUS                                status                  = STATUS_SUCCESS;
    WDF_USB_DEVICE_SELECT_CONFIG_PARAMS     configParams;
    WDF_OBJECT_ATTRIBUTES                   pipeAttributes;
    WDF_USB_PIPE_INFORMATION                pipeInfo;
    PDEVICE_CONTEXT                         pDeviceContext;
    WDFUSBPIPE                              pipe;
    UCHAR                                   i;
    UCHAR                                   numberAlternateSettings = 0;

    PAGED_CODE();

    pDeviceContext = GetDeviceContext(Device);

    // Your driver can use this function only if your device has just one USB interface.
    // zeros the structure and sets the Size member to the size of the structure.
    // It also sets the Type member to WdfUsbTargetDeviceSelectConfigTypeSingleInterface.
    WDF_USB_DEVICE_SELECT_CONFIG_PARAMS_INIT_SINGLE_INTERFACE(&configParams);
    WDF_OBJECT_ATTRIBUTES_INIT(&pipeAttributes);

    // The framework creates a framework USB pipe object for each pipe that is associated with each interface in the configuration,
    // after deleting any pipe objects that the framework might have previously created for the configuration.
    // The framework uses alternate setting zero for each interface, unless the driver specifies a different alternate setting.
    status = WdfUsbTargetDeviceSelectConfig(pDeviceContext->WdfUsbTargetDevice, &pipeAttributes, &configParams);

    if (NT_SUCCESS(status)) {
        pDeviceContext->UsbInterface                = configParams.Types.SingleInterface.ConfiguredUsbInterface;
        pDeviceContext->NumberConfiguredPipes       = configParams.Types.SingleInterface.NumberConfiguredPipes;
        numberAlternateSettings                     = WdfUsbInterfaceGetNumSettings(pDeviceContext->UsbInterface);
        pDeviceContext->SelectedAlternateSetting    = WdfUsbInterfaceGetConfiguredSettingIndex(pDeviceContext->UsbInterface);

        TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER,  "Num of ConfiguredPipes %d\n", pDeviceContext->NumberConfiguredPipes);
        TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER,  "Num of AlternateSettings %d select idx %d\n", numberAlternateSettings, pDeviceContext->SelectedAlternateSetting);

        for (i = 0; i < pDeviceContext->NumberConfiguredPipes; i++) {
            // driver can call WdfUsbInterfaceGetConfiguredPipe after it has called WdfUsbTargetDeviceSelectConfig.
            WDF_USB_PIPE_INFORMATION_INIT(&pipeInfo);
            pipe =  WdfUsbInterfaceGetConfiguredPipe(pDeviceContext->UsbInterface, i, &pipeInfo);

            switch (pipeInfo.EndpointAddress) {
                case MEMX_IN_EP:
                    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER,  "%d EP 0x%x BULK IN MaximumPacketSize 0x%x\n", i, pipeInfo.EndpointAddress, pipeInfo.MaximumPacketSize);
                    pDeviceContext->BulkReadPipe = pipe;
                    WdfUsbTargetPipeSetNoMaximumPacketSizeCheck(pDeviceContext->BulkReadPipe);
                    break;
                case MEMX_OUT_EP:
                    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER,  "%d EP 0x%x BULK OUT  MaximumPacketSize 0x%x\n", i, pipeInfo.EndpointAddress, pipeInfo.MaximumPacketSize);
                    pDeviceContext->BulkWritePipe = pipe;
                    pDeviceContext->BulkWriteMaximumPacketSize = pipeInfo.MaximumPacketSize;
                    break;
                case MEMX_FW_IN_EP:
                    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER,  "%d EP 0x%x BULK FW IN  MaximumPacketSize 0x%x\n", i, pipeInfo.EndpointAddress, pipeInfo.MaximumPacketSize);
                    pDeviceContext->FwReadPipe = pipe;
                    break;
                case MEMX_FW_OUT_EP:
                    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER,  "%d EP 0x%x BULK FW OUT  MaximumPacketSize 0x%x\n", i, pipeInfo.EndpointAddress, pipeInfo.MaximumPacketSize);
                    pDeviceContext->FwWritePipe = pipe;
                    break;
                default:
                    TraceEvents(TRACE_LEVEL_WARNING, TRACE_DRIVER,  "Unknown EP 0x%x\n", pipeInfo.EndpointAddress);
                    break;
            }
        }
    }

    return status;
}

NTSTATUS InitialDeviceFirmware(PDEVICE_CONTEXT devContext){
    PAGED_CODE();
    NTSTATUS            status = STATUS_SUCCESS;
    UNICODE_STRING      uniName;
    OBJECT_ATTRIBUTES   objAttr;
    HANDLE              handle;
    IO_STATUS_BLOCK     ioStatusBlock;

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "%!FUNC! Entry");

    RtlInitUnicodeString(&uniName, GLOBAL_FIRMWARE_DEFUALT_PATH);
    InitializeObjectAttributes(&objAttr, &uniName, OBJ_CASE_INSENSITIVE | OBJ_KERNEL_HANDLE, NULL, NULL);

    if (KeGetCurrentIrql() != PASSIVE_LEVEL)
    {// Do not try to perform any file operations at higher IRQL levels. Instead, you may use a work item or a system worker thread to perform file operations.
        status = STATUS_INVALID_DEVICE_STATE;
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER, "KeGetCurrentIrql fail irq %u\n", KeGetCurrentIrql());
    }
    else
    {
        status = ZwCreateFile(&handle, GENERIC_READ, &objAttr, &ioStatusBlock, NULL, FILE_ATTRIBUTE_NORMAL, 0, FILE_OPEN, FILE_SYNCHRONOUS_IO_NONALERT, NULL, 0);

        if (NT_SUCCESS(status))
        {
            WDF_MEMORY_DESCRIPTOR   memoryDescriptor;
            LARGE_INTEGER           byteOffset          = { .LowPart = 0, .HighPart = 0 };
            ULONG                   BUFFER_SIZE         = DEF_KB(128);
            PULONG                  plBuffer            = (PULONG)ExAllocatePool2(POOL_FLAG_PAGED, (SIZE_T)BUFFER_SIZE, 'mxfw');
            ULONG                   firmware_size       = 0;
            ULONG                   bytesToWrite        = 0;
            ULONG                   imgFmt              = 0;

            if (plBuffer != NULL)
            {
                status = ZwReadFile(handle, NULL, NULL, NULL, &ioStatusBlock, plBuffer, BUFFER_SIZE, &byteOffset, NULL);

                TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER, "ZwReadFile byteOffset %x firmware size %d\n", byteOffset.LowPart, firmware_size);

                if (NT_SUCCESS(status))
                {
                    if (plBuffer[(MEMX_FW_IMGFMT_OFFSET >> 2)] == 1) {
                        imgFmt = 1;
                        firmware_size = (MEMX_FSBL_SECTION_SIZE) + MEMX_FW_IMGSIZE_LEN + plBuffer[(MEMX_FW_IMGSIZE_OFFSET >> 2)] + MEMX_FW_IMGCRC_LEN;
                        if ((firmware_size + DEF_BYTE(8)) <= BUFFER_SIZE) {
                            plBuffer[(firmware_size) >> 2] = plBuffer[0];  //Padding FSBL Length
                            plBuffer[(firmware_size + 4) >> 2] = imgFmt;   //Padding ImgFmt
                            firmware_size = firmware_size - DEF_BYTE(4) + DEF_BYTE(8); //skip FSBL Length and plus two padding
                            TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER, "ZwReadFile byteOffset %x FSBL size %d firmware size %d\n", byteOffset.LowPart, plBuffer[0], firmware_size);
                        }
                        else {
                            TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER, "ZwReadFile OverSize firmware size %d\n", firmware_size);
                            status = STATUS_UNSUCCESSFUL;
                        }
                    }
                    else {
                        imgFmt = 0;
                        firmware_size = plBuffer[0]; //first 4 byte bin is size
                        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER, "ZwReadFile byteOffset %x firmware size %d\n", byteOffset.LowPart, firmware_size);
                    }

                    if (NT_SUCCESS(status)){
                        WDF_REQUEST_SEND_OPTIONS options;
                        WDF_REQUEST_SEND_OPTIONS_INIT(&options, WDF_REQUEST_SEND_OPTION_TIMEOUT);
                        WDF_REQUEST_SEND_OPTIONS_SET_TIMEOUT(&options, WDF_REL_TIMEOUT_IN_MS(2000));

                        //The specified pipe must be an output pipe, and the pipe's type must be WdfUsbPipeTypeBulk
                        //If you supply a NULL request handle, the framework uses an internal request object.
                        //This technique is simple to use, but the driver cannot cancel the request.
                        WDF_MEMORY_DESCRIPTOR_INIT_BUFFER(&memoryDescriptor, (PVOID) &firmware_size, DEF_BYTE(4));
                        status = WdfUsbTargetPipeWriteSynchronously(devContext->BulkWritePipe, NULL, &options, &memoryDescriptor, &bytesToWrite);
                        TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "Send Firmware Size %d - %d bytes\n", firmware_size, bytesToWrite);

                        //The specified pipe must be an output pipe, and the pipe's type must be WdfUsbPipeTypeBulk
                        PUCHAR currentBufferPointer = (PUCHAR)&plBuffer[1];

                        WDF_MEMORY_DESCRIPTOR_INIT_BUFFER(&memoryDescriptor, currentBufferPointer, firmware_size);
                        //If you supply a NULL request handle, the framework uses an internal request object.
                        //This technique is simple to use, but the driver cannot cancel the request.
                        status = WdfUsbTargetPipeWriteSynchronously(devContext->BulkWritePipe, NULL, &options, &memoryDescriptor, &bytesToWrite);

                        TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_QUEUE,"Send FW body %ld/%ld status 0x%x\n", bytesToWrite, firmware_size, status);
                    }

                    ZwClose(handle);
                }
                else
                {
                    TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER, "ZwReadFile fail status 0x%x Io status 0x%x \n", status, ioStatusBlock.Status);
                }

                ExFreePool(plBuffer);
            }
            else
            {
                TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER, "ExAllocatePool2 fail \n");
            }
        }
        else
        {
            TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER, "ZwCreateFile fail status 0x%x Io status 0x%x \n", status, ioStatusBlock.Status);
        }
    }

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "%!FUNC! Exit 0x%x\n", status);

    return status;
}

NTSTATUS MemxEvtDeviceAdd(WDFDRIVER Driver, PWDFDEVICE_INIT DeviceInit)
{
    NTSTATUS                        status              = STATUS_SUCCESS;
    WDF_FILEOBJECT_CONFIG           fileConfig;
    WDF_PNPPOWER_EVENT_CALLBACKS    pnpPowerCallbacks;
    WDF_OBJECT_ATTRIBUTES           attributes;
    WDFDEVICE                       device;
    WDF_DEVICE_PNP_CAPABILITIES     pnpCaps;
    WDF_IO_QUEUE_CONFIG             ioQueueConfig;
    WDFQUEUE                        queue;
    PDEVICE_CONTEXT                 pDevContext;
    ULONG                           maximumTransferSize;

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
    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&attributes, FILE_CONTEXT);
    WdfDeviceInitSetFileObjectConfig(DeviceInit, &fileConfig, &attributes);
    WdfDeviceInitSetIoType(DeviceInit, WdfDeviceIoDirect);
    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&attributes, REQUEST_CONTEXT);
    WdfDeviceInitSetRequestAttributes(DeviceInit, &attributes);

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
    //3.  Copy the contents of the PnP/Power Callbacks "collector structure" to
    //    our WDFDEVICE_INIT structure (which is the object-specific configurator for WDFDEVICE).
    WdfDeviceInitSetPnpPowerEventCallbacks(DeviceInit, &pnpPowerCallbacks);

    //4. Associate our device context structure type with our WDFDEVICE
    WDF_OBJECT_ATTRIBUTES_INIT_CONTEXT_TYPE(&attributes, DEVICE_CONTEXT);

    // This callback is invoked when the device is removed or the driver is unloaded.
    attributes.EvtCleanupCallback = MemxEvtDeviceCleanup;

    status = WdfDeviceCreate(&DeviceInit, &attributes, &device);
    if (!NT_SUCCESS(status)) {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER,  "WdfDeviceCreate failed with Status code 0x%x\n", status);
        goto done;
    }

    PowerPolicySuspendEnable(device);

    // Get a pointer to our device context,
    pDevContext = GetDeviceContext(device);

    // Tell the framework to set the SurpriseRemovalOK in the DeviceCaps so
    // that you don't get the popup in usermode (on Win2K) when you surprise
    // remove the device.
    WDF_DEVICE_PNP_CAPABILITIES_INIT(&pnpCaps);
    pnpCaps.SurpriseRemovalOK = WdfTrue;
    WdfDeviceSetPnpCapabilities(device, &pnpCaps);

    // Register I/O callbacks to tell the framework that you are interested
    // in handling WdfRequestTypeRead, WdfRequestTypeWrite, and
    // IRP_MJ_DEVICE_CONTROL requests.
    // WdfIoQueueDispatchParallel means that we are capable of handling
    // all the I/O request simultaneously and we are responsible for protecting
    // data that could be accessed by these callbacks simultaneously.
    // This queue will be,  by default,  automanaged by the framework with
    // respect to PNP and Power events. That is, framework will take care
    // of queuing, failing, dispatching incoming requests based on the current
    // pnp/power state of the device.s

    // create default queue for ioctrl
    WDF_IO_QUEUE_CONFIG_INIT_DEFAULT_QUEUE(&ioQueueConfig, WdfIoQueueDispatchSequential);
    ioQueueConfig.EvtIoDeviceControl    = MemxEvtIoDeviceControl;
    ioQueueConfig.EvtIoStop             = MemxEvtIoStop;
    status = WdfIoQueueCreate(device, &ioQueueConfig, WDF_NO_OBJECT_ATTRIBUTES, &queue);
    if (!NT_SUCCESS(status)) {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER, "WdfIoQueueCreate for Default queue failed 0x%0x\n", status);
        goto done;
    }

    // create bulk read queue
    WDF_IO_QUEUE_CONFIG_INIT(&ioQueueConfig, WdfIoQueueDispatchSequential);
    ioQueueConfig.EvtIoRead     = MemxEvtIoRead;
    ioQueueConfig.EvtIoStop     = MemxEvtIoStop;
    status = WdfIoQueueCreate(device, &ioQueueConfig, WDF_NO_OBJECT_ATTRIBUTES, &queue);
    if (!NT_SUCCESS(status)) {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER, "WdfIoQueueCreate for Bulk Read queue failed 0x%0x\n", status);
        goto done;
    }

    status = WdfDeviceConfigureRequestDispatching(device, queue, WdfRequestTypeRead);
    if (!NT_SUCCESS(status)) {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER,  "WdfDeviceConfigureRequestDispatching failed for Bulk Read Queue 0x%x\n", status);
        goto done;
    }

    // create bulk write queue
    WDF_IO_QUEUE_CONFIG_INIT(&ioQueueConfig, WdfIoQueueDispatchSequential);
    ioQueueConfig.EvtIoWrite    = MemxEvtIoWrite;
    ioQueueConfig.EvtIoStop     = MemxEvtIoStop;
    status = WdfIoQueueCreate(device, &ioQueueConfig, WDF_NO_OBJECT_ATTRIBUTES, &queue);
    if (!NT_SUCCESS(status)) {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER,  "WdfIoQueueCreate failed for Bulk Write Queue 0x%x\n", status);
         goto done;
    }

    status = WdfDeviceConfigureRequestDispatching(device, queue, WdfRequestTypeWrite);
    if (!NT_SUCCESS(status)) {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER,  "WdfDeviceConfigureRequestDispatching failed for Bulk Write Queue 0x%x\n", status);
        goto done;
    }

    //Get MaximumTransferSize from registry
    maximumTransferSize = 0;
    status = ReadFdoRegistryKeyValue(Driver, L"MaximumTransferSize", &maximumTransferSize);
    if (!NT_SUCCESS(status)) {
        pDevContext->MaximumTransferSize = DEFAULT_REGISTRY_TRANSFER_SIZE;
    } else {
        pDevContext->MaximumTransferSize = (maximumTransferSize) ? maximumTransferSize : DEFAULT_REGISTRY_TRANSFER_SIZE;
        TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER,  "pDevContext->MaximumTransferSize %ld\n", pDevContext->MaximumTransferSize);
    }

    // Register a device interface so that app can find our device and talk to it.
    status = CreateUsbDeviceInterface(device);
    if (!NT_SUCCESS(status)) {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER,  "MemxCreateDeviceInterface failed  0x%x\n", status);
        goto done;
    }

    pDevContext->is_read_abort = 0;
    pDevContext->reference_count = 0;
done:
    return status;
}

NTSTATUS MemxEvtDevicePrepareHardware(WDFDEVICE Device, WDFCMRESLIST RawResources, WDFCMRESLIST TranslatedResources)
{
    NTSTATUS                status          = STATUS_SUCCESS;
    WDF_OBJECT_ATTRIBUTES   objectAttribs;
    PDEVICE_CONTEXT         pDeviceContext;

    UNREFERENCED_PARAMETER(RawResources);
    UNREFERENCED_PARAMETER(TranslatedResources);
    PAGED_CODE();

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "%!FUNC! Entry");

    pDeviceContext = GetDeviceContext(Device);

    // Read the device descriptor, configuration descriptor and select the interface descriptors
    status = ReadDeviceDescriptorsAndConfig(Device);
    if (!NT_SUCCESS(status)) {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER,  "ReadDeviceDescriptorsAndConfig failed\n");
        goto done;
    }

    // Enable wait-wake and idle timeout if the device supports it
    if (pDeviceContext->WaitWakeEnable){
        status = SetPowerPolicy(Device);
        if (!NT_SUCCESS (status)) {
            TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER, "SetPowerPolicy failed\n");
            goto done;
        }
    }

    WDF_OBJECT_ATTRIBUTES_INIT(&objectAttribs);
    objectAttribs.ParentObject = pDeviceContext->WdfUsbTargetDevice;
    status = WdfUsbTargetDeviceCreateUrb(pDeviceContext->WdfUsbTargetDevice, &objectAttribs, &pDeviceContext->FwUrbMemory, &pDeviceContext->FwUrb);
    if (status != STATUS_SUCCESS) {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER,  "WdfUsbTargetDeviceCreateUrb failed\n");
         goto done;
    }

    KeInitializeMutex(&pDeviceContext->ConfigMutex, 0);

    WDF_OBJECT_ATTRIBUTES_INIT(&objectAttribs);
    objectAttribs.ParentObject = pDeviceContext->WdfUsbTargetDevice;
    status = WdfWaitLockCreate(&objectAttribs, &pDeviceContext->read_lock);
    if (!NT_SUCCESS(status)) {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER, "WdfWaitLockCreate failed 0x%x\n", status);
        goto done;
    }

    WDF_OBJECT_ATTRIBUTES_INIT(&objectAttribs);
    objectAttribs.ParentObject = pDeviceContext->WdfUsbTargetDevice;
    status = WdfSpinLockCreate(&objectAttribs, &pDeviceContext->device_lock);
    if (!NT_SUCCESS(status)) {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER, "WdfSpinLockCreate failed 0x%x\n", status);
        goto done;
    }

done:
    return status;
}

NTSTATUS MemxEvtDeviceReleaseHardware(WDFDEVICE Device, WDFCMRESLIST ResourcesTranslated)
{
    PAGED_CODE();
    NTSTATUS status = STATUS_SUCCESS;
    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "%!FUNC! Entry");

    UNREFERENCED_PARAMETER(ResourcesTranslated);
    PDEVICE_CONTEXT          devContext;
    devContext = GetDeviceContext((WDFDEVICE)Device);
    return status;
}

NTSTATUS MemxEvtDeviceD0Entry(WDFDEVICE Device, WDF_POWER_DEVICE_STATE PreviousState)
{
    PAGED_CODE();
    NTSTATUS status = STATUS_SUCCESS;
    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "%!FUNC! Entry");

    UNREFERENCED_PARAMETER(PreviousState);
    PDEVICE_CONTEXT          devContext;
    devContext  = GetDeviceContext((WDFDEVICE)Device);

    status = memx_usb_init_hw_info(devContext);
    if (!NT_SUCCESS(status)) {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER, "memx_usb_init_hw_info failed 0x%x\n", status);
        status = STATUS_SUCCESS;
    }

    return status;
}

NTSTATUS MemxEvtPostInterruptEnable(WDFDEVICE Device, WDF_POWER_DEVICE_STATE PreviousState)
{
    PAGED_CODE();
    UNREFERENCED_PARAMETER(PreviousState);
    PDEVICE_CONTEXT     devContext;
    NTSTATUS            status = STATUS_SUCCESS;

    TraceEvents(TRACE_LEVEL_INFORMATION, TRACE_DRIVER, "%!FUNC! Entry");
    devContext = GetDeviceContext((WDFDEVICE)Device);

#if 1
    WCHAR       usbPID[USB_PID_LEN * sizeof(WCHAR)] = { 0 };
    status      = GetUsbPID(Device, usbPID);
    if (!NT_SUCCESS(status)) {
        TraceEvents(TRACE_LEVEL_ERROR, TRACE_DRIVER, "MemxGetUsbPID failed with Status code 0x%x\n", status);
    } else {
        if (wcscmp(usbPID, L"PID_40FF") == 0) {
            status = InitialDeviceFirmware(devContext);
            if (!NT_SUCCESS(status)) {
                WdfDeviceSetFailed(Device, WdfDeviceFailedNoRestart);
            }
        }
    }
#endif

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

VOID MemxEvtDeviceCleanup(WDFOBJECT Device)
{
    UNREFERENCED_PARAMETER(Device);

    PAGED_CODE();
}
