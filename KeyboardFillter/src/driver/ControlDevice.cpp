#include "ControlDevice.h"
#include <shared/Common.h>
#include <src/utils/logging.h>
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
