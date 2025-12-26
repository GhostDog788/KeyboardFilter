#pragma once
#include <ntddk.h>

NTSTATUS createControlDevice(PDRIVER_OBJECT DriverObject);
void cleanupControlDevice(PDEVICE_OBJECT DeviceObject = nullptr);
