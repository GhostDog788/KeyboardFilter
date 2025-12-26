#pragma once
#include <src/driver/DeviceTypes.h>

void handleKeyboardResponse(PDEVICE_KBFILTER devExt, PIRP Irp);
NTSTATUS keyboardbReadCompletion(PDEVICE_OBJECT FilterDeviceObject, PIRP Irp, PVOID Context);
NTSTATUS attachToKeyboardStack(PDRIVER_OBJECT DriverObject);
void cleanupKbFilterDevice(PDEVICE_KBFILTER devExt);
