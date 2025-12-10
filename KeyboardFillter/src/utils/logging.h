#pragma once
#include <ntddk.h>
#include <src/driver/Definitions.h>

#define log(fmt, ...) DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_INFO_LEVEL, fmt, __VA_ARGS__)

void LogRequest(PDEVICE_EXTENSION extDev, PIRP Irp);
