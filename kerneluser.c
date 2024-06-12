#pragma warning (disable : 4100 4047 4024)

#include "kerneluser.h"

#include <ntddk.h>

#include <ntstrsafe.h>

#define IOCTL_TRIGGER_LOGIN CTL_CODE(FILE_DEVICE_UNKNOWN, 0x800, METHOD_BUFFERED, FILE_ANY_ACCESS)

PDEVICE_OBJECT g_DeviceObject = NULL;

UNICODE_STRING g_SymbolicLink;

VOID DriverUnload(PDRIVER_OBJECT DriverObject);

VOID ProcessNotifyRoutineEx(PEPROCESS Process, HANDLE ProcessId, PPS_CREATE_NOTIFY_INFO CreateInfo);

NTSTATUS DriverCreateClose(PDEVICE_OBJECT DeviceObject, PIRP Irp);

NTSTATUS DriverDeviceControl(PDEVICE_OBJECT DeviceObject, PIRP Irp);

NTSTATUS WriteToPipe(PCWSTR PipeName, PCWSTR Message);

NTSTATUS DriverEntry(PDRIVER_OBJECT DriverObject, PUNICODE_STRING RegistryPath)

{

    UNREFERENCED_PARAMETER(RegistryPath);

    NTSTATUS status;

    UNICODE_STRING deviceName;

    RtlInitUnicodeString(&deviceName, L"\\Device\\ProcessMonitor");

    RtlInitUnicodeString(&g_SymbolicLink, L"\\DosDevices\\ProcessMonitor");

    status = IoCreateDevice(DriverObject, 0, &deviceName, FILE_DEVICE_UNKNOWN, 0, FALSE, &g_DeviceObject);

    if (!NT_SUCCESS(status))

    {

        DebugMessage("Failed to create device\n");

        return status;

    }

    status = IoCreateSymbolicLink(&g_SymbolicLink, &deviceName);

    if (!NT_SUCCESS(status))

    {

        IoDeleteDevice(g_DeviceObject);

        DebugMessage("Failed to create symbolic link\n");

        return status;

    }

    DriverObject->DriverUnload = DriverUnload;

    DriverObject->MajorFunction[IRP_MJ_CREATE] = DriverCreateClose;

    DriverObject->MajorFunction[IRP_MJ_CLOSE] = DriverCreateClose;

    DriverObject->MajorFunction[IRP_MJ_DEVICE_CONTROL] = DriverDeviceControl;

    status = PsSetCreateProcessNotifyRoutineEx(ProcessNotifyRoutineEx, FALSE);

    if (!NT_SUCCESS(status))

    {

        IoDeleteSymbolicLink(&g_SymbolicLink);

        IoDeleteDevice(g_DeviceObject);

        DebugMessage("Failed to set process creation notify routine\n");

        return status;

    }

    // Create a named event to signal user-mode application

    UNICODE_STRING eventName;

    RtlInitUnicodeString(&eventName, L"\\BaseNamedObjects\\ProcessMonitorEvent");

    status = ZwCreateEvent(&g_EventHandle, EVENT_ALL_ACCESS, NULL, SynchronizationEvent, FALSE);

    if (!NT_SUCCESS(status))

    {

        PsSetCreateProcessNotifyRoutineEx(ProcessNotifyRoutineEx, TRUE);

        IoDeleteSymbolicLink(&g_SymbolicLink);

        IoDeleteDevice(g_DeviceObject);

        DebugMessage("Failed to create event\n");

        return status;

    }

    status = ObReferenceObjectByHandle(g_EventHandle, EVENT_ALL_ACCESS, *ExEventObjectType, KernelMode, (PVOID*)&g_EventObject, NULL);

    if (!NT_SUCCESS(status))

    {

        ZwClose(g_EventHandle);

        PsSetCreateProcessNotifyRoutineEx(ProcessNotifyRoutineEx, TRUE);

        IoDeleteSymbolicLink(&g_SymbolicLink);

        IoDeleteDevice(g_DeviceObject);

        DebugMessage("Failed to reference event object\n");

        return status;

    }

    DebugMessage("Driver Loaded\n");

    return STATUS_SUCCESS;

}

VOID DriverUnload(PDRIVER_OBJECT DriverObject)

{

    PsSetCreateProcessNotifyRoutineEx(ProcessNotifyRoutineEx, TRUE);

    if (g_EventHandle)

    {

        ZwClose(g_EventHandle);

    }

    if (g_EventObject)

    {

        ObDereferenceObject(g_EventObject);

    }

    IoDeleteSymbolicLink(&g_SymbolicLink);

    IoDeleteDevice(g_DeviceObject);

    DebugMessage("Driver Unloaded\n");

}

NTSTATUS DriverCreateClose(PDEVICE_OBJECT DeviceObject, PIRP Irp)

{

    UNREFERENCED_PARAMETER(DeviceObject);

    Irp->IoStatus.Status = STATUS_SUCCESS;

    Irp->IoStatus.Information = 0;

    IoCompleteRequest(Irp, IO_NO_INCREMENT);

    return STATUS_SUCCESS;

}

NTSTATUS DriverDeviceControl(PDEVICE_OBJECT DeviceObject, PIRP Irp)

{

    UNREFERENCED_PARAMETER(DeviceObject);

    PIO_STACK_LOCATION irpSp = IoGetCurrentIrpStackLocation(Irp);

    NTSTATUS status = STATUS_SUCCESS;

    switch (irpSp->Parameters.DeviceIoControl.IoControlCode)

    {

    case IOCTL_TRIGGER_LOGIN:

    {

        // Signal the event

        KeSetEvent(g_EventObject, IO_NO_INCREMENT, FALSE);

        DebugMessage("Event signaled\n");

        break;

    }

    default:

        status = STATUS_INVALID_DEVICE_REQUEST;

        break;

    }

    Irp->IoStatus.Status = status;

    Irp->IoStatus.Information = 0;

    IoCompleteRequest(Irp, IO_NO_INCREMENT);

    return status;

}

VOID ProcessNotifyRoutineEx(PEPROCESS Process, HANDLE ProcessId, PPS_CREATE_NOTIFY_INFO CreateInfo)

{

    UNREFERENCED_PARAMETER(Process);

    UNREFERENCED_PARAMETER(ProcessId);

    if (CreateInfo)

    {

        DebugMessage("Process created: %wZ\n", CreateInfo->ImageFileName);

        WriteToPipe(L"\\.\\pipe\\ProcessMonitor", CreateInfo->ImageFileName->Buffer);

        // Signal the event to user-mode application

        KeSetEvent(g_EventObject, IO_NO_INCREMENT, FALSE);

    }

}

NTSTATUS WriteToPipe(PCWSTR PipeName, PCWSTR Message)

{

    UNICODE_STRING pipeName;

    OBJECT_ATTRIBUTES objectAttributes;

    IO_STATUS_BLOCK ioStatusBlock;

    HANDLE fileHandle;

    NTSTATUS status;

    RtlInitUnicodeString(&pipeName, PipeName);

    InitializeObjectAttributes(&objectAttributes, &pipeName, OBJ_CASE_INSENSITIVE, NULL, NULL);

    status = ZwCreateFile(&fileHandle, GENERIC_WRITE | FILE_WRITE_DATA, &objectAttributes, &ioStatusBlock, NULL, FILE_ATTRIBUTE_NORMAL, 0, FILE_OPEN,FILE_SYNCHRONOUS_IO_NONALERT, NULL, 0);

    if (!NT_SUCCESS(status))

    {

        DebugMessage("Failed to open pipe: %wZ\n", &pipeName);

        return status;

    }

    size_t messageLength = (wcslen(Message) + 1) * sizeof(WCHAR);

    status = ZwWriteFile(fileHandle, NULL, NULL, NULL, &ioStatusBlock, (PVOID)Message, (ULONG)messageLength, NULL, NULL);

    if (!NT_SUCCESS(status))

    {

        DebugMessage("Failed to write to pipe: %wZ\n", &pipeName);

    }

    ZwClose(fileHandle);

    return status;

}






        
