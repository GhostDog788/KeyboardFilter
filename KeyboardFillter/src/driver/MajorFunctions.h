#pragma once
#include <ntddk.h>

NTSTATUS ForwardMajorFunction(PDEVICE_OBJECT FilterDeviceObject, PIRP Irp);
NTSTATUS HandleReadRequest(PDEVICE_OBJECT FilterDeviceObject, PIRP Irp);
NTSTATUS FilterDispatchPnp(PDEVICE_OBJECT FilterDeviceObject, PIRP Irp);
NTSTATUS ReadCompletion(PDEVICE_OBJECT DeviceObject, PIRP Irp, PVOID Context);
