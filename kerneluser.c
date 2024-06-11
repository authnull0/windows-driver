#pragma warning (disable : 4100 4047 4024)

#include <ntddk.h>

#define DEVICE_NAME     L"\\Device\\MyDriver"
#define LINK_NAME       L"\\DosDevices\\MyDriver"

UNICODE_STRING deviceName;
UNICODE_STRING linkName;

PDEVICE_OBJECT pDeviceObject;

// IRP Handler
NTSTATUS MyDriver_CreateClose(PDEVICE_OBJECT DeviceObject, PIRP Irp) {
    UNREFERENCED_PARAMETER(DeviceObject);

    Irp->IoStatus.Status = STATUS_SUCCESS;
    Irp->IoStatus.Information = 0;

    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return STATUS_SUCCESS;
}

// Driver Unload routine
VOID MyDriver_Unload(PDRIVER_OBJECT DriverObject) {
    UNREFERENCED_PARAMETER(DriverObject);

    IoDeleteSymbolicLink(&linkName);
    IoDeleteDevice(pDeviceObject);
}

// Dispatch routine
NTSTATUS MyDriver_Dispatch(PDEVICE_OBJECT DeviceObject, PIRP Irp) {
    UNREFERENCED_PARAMETER(DeviceObject);

    switch (IoGetCurrentIrpStackLocation(Irp)->MajorFunction) {
    case IRP_MJ_CREATE:
    case IRP_MJ_CLOSE:
        return MyDriver_CreateClose(DeviceObject, Irp);
    default:
        Irp->IoStatus.Status = STATUS_NOT_SUPPORTED;
        break;
    }

    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return STATUS_SUCCESS;
}

// Function to send signal to user mode
VOID SendSignalToUserMode() {
    UNICODE_STRING pipeName;
    IO_STATUS_BLOCK ioStatusBlock;
    HANDLE pipeHandle;
    OBJECT_ATTRIBUTES objectAttributes;

    RtlInitUnicodeString(&pipeName, L"\\??\\pipe\\KernelModeAuthPipe");

    InitializeObjectAttributes(&objectAttributes, &pipeName, OBJ_CASE_INSENSITIVE, NULL, NULL);

    // Open the named pipe
    NTSTATUS status = ZwCreateFile(&pipeHandle, GENERIC_WRITE, &objectAttributes, &ioStatusBlock, NULL,
        FILE_ATTRIBUTE_NORMAL, 0, FILE_OPEN, FILE_NON_DIRECTORY_FILE, NULL, 0);

    if (NT_SUCCESS(status)) {
        // Write a single byte to the pipe to signal the user mode application
        UCHAR data = 0;
        ZwWriteFile(pipeHandle, NULL, NULL, NULL, &ioStatusBlock, &data, sizeof(data), NULL, NULL);

        // Close the pipe handle
        ZwClose(pipeHandle);
    }
}

// Driver Entry
NTSTATUS DriverEntry(PDRIVER_OBJECT pDriverObject, PUNICODE_STRING RegistryPath) {
    UNREFERENCED_PARAMETER(RegistryPath);

    pDriverObject->DriverUnload = MyDriver_Unload;
    pDriverObject->MajorFunction[IRP_MJ_CREATE] = MyDriver_Dispatch;
    pDriverObject->MajorFunction[IRP_MJ_CLOSE] = MyDriver_Dispatch;

    // Create the device object
    RtlInitUnicodeString(&deviceName, DEVICE_NAME);
    RtlInitUnicodeString(&linkName, LINK_NAME);

    NTSTATUS status = IoCreateDevice(pDriverObject, 0, &deviceName, FILE_DEVICE_UNKNOWN,
        FILE_DEVICE_SECURE_OPEN, FALSE, &pDeviceObject);

    if (!NT_SUCCESS(status)) {
        return status;
    }

    // Create symbolic link
    IoCreateSymbolicLink(&linkName, &deviceName);

    // Send signal to user mode to initialize
    SendSignalToUserMode();

    return STATUS_SUCCESS;
}
