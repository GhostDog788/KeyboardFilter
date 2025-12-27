#pragma once
#include <ntddk.h>

NTSTATUS completeIrp(PIRP Irp, NTSTATUS status = STATUS_SUCCESS, ULONG_PTR info = 0);
