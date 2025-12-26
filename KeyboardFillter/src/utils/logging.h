#pragma once
#include <ntddk.h>
#include <src/driver/DeviceTypes.h>

#define log(fmt, ...) DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_INFO_LEVEL, fmt, __VA_ARGS__)

void LogRequest(PDEVICE_KBFILTER extDev, PIRP Irp);
