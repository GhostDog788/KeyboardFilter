#include <ntddk.h>
#include <kstd/memory/New.hpp>
#include <src/utils/logging.h>
#include <src/driver/Definitions.h>
#include <src/driver/MajorFunctions.h>

void DriverUnload(PDRIVER_OBJECT DriverObject);

extern "C" NTSTATUS
DriverEntry(PDRIVER_OBJECT DriverObject, PUNICODE_STRING RegistryPath) {
	UNREFERENCED_PARAMETER(RegistryPath);

	UNICODE_STRING targetName = RTL_CONSTANT_STRING(L"\\Device\\KeyboardClass0");
	NTSTATUS status = STATUS_SUCCESS;
	PDEVICE_OBJECT filterDeviceObject = nullptr;
	PDEVICE_EXTENSION devExt = nullptr;
	do {
		for (int i = 0; i < ARRAYSIZE(DriverObject->MajorFunction); i++) {
			if (i == IRP_MJ_READ) {
				DriverObject->MajorFunction[i] = HandleReadRequest;
				continue;
			}
			if (i == IRP_MJ_PNP) {
				DriverObject->MajorFunction[i] = FilterDispatchPnp;
				continue;
			}
			DriverObject->MajorFunction[i] = ForwardMajorFunction;
		}
		DriverObject->DriverUnload = DriverUnload;

		status = IoCreateDevice(DriverObject, sizeof(DEVICE_EXTENSION), nullptr,
			FILE_DEVICE_UNKNOWN, 0, FALSE, &filterDeviceObject);
		if (!NT_SUCCESS(status)) break;
		devExt = (PDEVICE_EXTENSION)filterDeviceObject->DeviceExtension;
		RtlZeroMemory(devExt, sizeof(*devExt));
		new (&devExt->KeyEventBuffer) kstd::CircularBuffer<KeyEvent, KEY_EVENT_BUFFER_SIZE>(true);
		devExt->RemoveLock.Initialize();
		KeInitializeSpinLock(&devExt->BufferLock);

		PFILE_OBJECT FileObject;
		PDEVICE_OBJECT TargetDeviceObject;
		status = IoGetDeviceObjectPointer(&targetName, FILE_READ_DATA, &FileObject, &TargetDeviceObject);
		if (!NT_SUCCESS(status)) break;

		filterDeviceObject->Flags |= TargetDeviceObject->Flags & (DO_BUFFERED_IO | DO_DIRECT_IO);
		filterDeviceObject->Flags &= ~DO_DEVICE_INITIALIZING;
		filterDeviceObject->Flags |= DO_POWER_PAGABLE;
		filterDeviceObject->DeviceType = TargetDeviceObject->DeviceType;

		status = IoAttachDeviceToDeviceStackSafe(filterDeviceObject, TargetDeviceObject, &devExt->LowerDeviceObject);
		ObfDereferenceObject(FileObject);
		if (!NT_SUCCESS(status)) break;

		log("Attached to %wZ, lower DO=%p\n", &targetName, devExt->LowerDeviceObject);
	} while (false);

	if (!NT_SUCCESS(status)) {
		log("Failed to attach to %wZ: status=0x%08X\n", &targetName, status);
		if (filterDeviceObject) {
			IoDeleteDevice(filterDeviceObject);
		}
	}
	return status;
}

void DriverUnload(PDRIVER_OBJECT DriverObject) {
	auto filterDeviceObjects = DriverObject->DeviceObject;

	auto currentFilterDeviceObject = filterDeviceObjects; // I know its redundant. This is for clarity.
	while (currentFilterDeviceObject != nullptr) {
		auto devExt = (PDEVICE_EXTENSION)currentFilterDeviceObject->DeviceExtension;

		devExt->RemoveLock.ManualLock();
		devExt->RemoveLock.ReleaseAndWait();
		devExt->KeyEventBuffer.~CircularBuffer(); // Explicitly call destructor

		if (devExt->LowerDeviceObject) {
			IoDetachDevice(devExt->LowerDeviceObject);
			devExt->LowerDeviceObject = nullptr;
		}

		PDEVICE_OBJECT nextFilterDeviceObject = currentFilterDeviceObject->NextDevice;
		IoDeleteDevice(currentFilterDeviceObject);
		currentFilterDeviceObject = nextFilterDeviceObject;
	}
}
