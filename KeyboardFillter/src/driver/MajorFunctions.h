#pragma once
#include <ntddk.h>

NTSTATUS GenericMajorFunction(PDEVICE_OBJECT DeviceObject, PIRP Irp);
NTSTATUS HandleReadRequest(PDEVICE_OBJECT DeviceObject, PIRP Irp);
NTSTATUS HandlePnpRequest(PDEVICE_OBJECT DeviceObject, PIRP Irp);
NTSTATUS HandleDeviceControl(PDEVICE_OBJECT DeviceObject, PIRP Irp);
NTSTATUS HandleCreateOrCloseRequest(PDEVICE_OBJECT DeviceObject, PIRP Irp);

void registerMajorFunctionsHandlers(PDRIVER_OBJECT DriverObject);
