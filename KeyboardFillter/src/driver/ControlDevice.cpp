#include "ControlDevice.h"
#include <shared/Common.h>
#include <src/utils/logging.h>
#include <src/utils/IrpHandling.h>
#include <src/driver/DeviceTypes.h>

NTSTATUS createControlDevice(PDRIVER_OBJECT DriverObject) {
	UNICODE_STRING devName = RTL_CONSTANT_STRING(DEVICE_PATH);
	UNICODE_STRING symLink = RTL_CONSTANT_STRING(DEVICE_LINK);
	PDEVICE_OBJECT DeviceObject = nullptr;
	auto status = STATUS_SUCCESS;
	do {
		status = IoCreateDevice(DriverObject, sizeof(DEVICE_KBFILTER), nullptr,
			FILE_DEVICE_UNKNOWN, 0, FALSE, &DeviceObject);
		if (!NT_SUCCESS(status)) {
			log("failed to create device (0x%08X)\n", status);
			break;
		}

		auto devExt = (PDEVICE_GENERIC)DeviceObject->DeviceExtension;
		RtlZeroMemory(devExt, sizeof(*devExt));
		devExt->DeviceType = DeviceType::DEVICE_CONTROL;

		DeviceObject->Flags |= DO_DIRECT_IO;
		status = IoCreateSymbolicLink(&symLink, &devName);
		if (!NT_SUCCESS(status)) {
			log("failed to create symbolic link (0x%08X)\n", status);
			break;
		}
	} while (false);
	if (!NT_SUCCESS(status)) {
		if (DeviceObject) IoDeleteDevice(DeviceObject);
	}
	return status;
}

void cleanupControlDevice(PDEVICE_OBJECT DeviceObject) {
	UNICODE_STRING symLink = RTL_CONSTANT_STRING(DEVICE_LINK);
	IoDeleteSymbolicLink(&symLink);
	if (DeviceObject) IoDeleteDevice(DeviceObject);
}

NTSTATUS handleControlDeviceIOCTL(PIRP Irp)
{
	auto irpSp = IoGetCurrentIrpStackLocation(Irp);
	auto& dic = irpSp->Parameters.DeviceIoControl;
	auto status = STATUS_INVALID_DEVICE_REQUEST;
	ULONG_PTR len = 0;
	switch (dic.IoControlCode) {
		case IOCTL_GET_KEY_EVENTS:
		{
			// TODO: implement key event retrieval IOCTL
			// Get input of IOCTL and convert it to a struct from Common.h
			// Make sure the output buffer can hold the amount of key events requested in the input struct
			// Inc the read lock of the global key event buffer
			// Pop the requested amount of key events from the global key event buffer into the output buffer
			// Dec the read lock of the global key event buffer
		}
	default:
		status = STATUS_INVALID_DEVICE_REQUEST;
		break;
	}
	return completeIrp(Irp, status, len);
}
