#include "KeyboardSniffing.h"
#include <ntddkbd.h>
#include <kstd/memory/New.hpp>
#include <src/driver/DeviceTypes.h>
#include <src/utils/logging.h>

void handleKeyboardResponse(PDEVICE_KBFILTER devExt, PIRP Irp)
{
	ULONG bytes = (ULONG)Irp->IoStatus.Information;
	PUCHAR buf = (PUCHAR)Irp->AssociatedIrp.SystemBuffer;
	PKEYBOARD_INPUT_DATA keyData = (PKEYBOARD_INPUT_DATA)buf;
	ULONG count = bytes / sizeof(KEYBOARD_INPUT_DATA);

	kstd::SpinLockGuard guard(devExt->BufferLock);
	for (ULONG i = 0; i < count; ++i) {
		log("Key[%lu]: MakeCode=0x%X, Flags=0x%X\n", i, keyData[i].MakeCode, keyData[i].Flags);
		KeyEvent key_data{};
		KeQuerySystemTimePrecise((PLARGE_INTEGER)&key_data.Timestamp);
		key_data.MakeCode = keyData[i].MakeCode;
		key_data.Flags = keyData[i].Flags;
		devExt->KeyEventBuffer.push(key_data);
	}
}

NTSTATUS keyboardbReadCompletion(PDEVICE_OBJECT FilterDeviceObject, PIRP Irp, PVOID Context)
{
	UNREFERENCED_PARAMETER(Context);

	auto devExt = (PDEVICE_KBFILTER)FilterDeviceObject->DeviceExtension;
	if (Irp->PendingReturned) {
		IoMarkIrpPending(Irp);
	}

	if (NT_SUCCESS(Irp->IoStatus.Status) && Irp->IoStatus.Information > 0)
	{
		ULONG bytes = (ULONG)Irp->IoStatus.Information;
		PUCHAR buf = (PUCHAR)Irp->AssociatedIrp.SystemBuffer;
		log("READ completed: Status=0x%08X, Bytes=%lu, Buf=%p\n", Irp->IoStatus.Status, bytes, buf);

		handleKeyboardResponse(devExt, Irp);
	}
	else {
		log("READ completed: Status=0x%08X, Bytes=%Iu\n", Irp->IoStatus.Status, Irp->IoStatus.Information);
	}

	devExt->RemoveLock.ManualUnlock();
	return STATUS_CONTINUE_COMPLETION;
}

NTSTATUS attachToKeyboardStack(PDRIVER_OBJECT DriverObject)
{
	NTSTATUS status = STATUS_SUCCESS;
	UNICODE_STRING targetName = RTL_CONSTANT_STRING(L"\\Device\\KeyboardClass0");
	PDEVICE_OBJECT filterDeviceObject = nullptr;
	do {
		status = IoCreateDevice(DriverObject, sizeof(DEVICE_KBFILTER), nullptr,
			FILE_DEVICE_UNKNOWN, 0, FALSE, &filterDeviceObject);
		if (!NT_SUCCESS(status)) break;
		auto devExt = (PDEVICE_KBFILTER)filterDeviceObject->DeviceExtension;
		RtlZeroMemory(devExt, sizeof(*devExt));
		devExt->DeviceType = DeviceType::DEVICE_KBFILTER;
		new (&devExt->KeyEventBuffer) kstd::CircularBuffer<KeyEvent, KEY_EVENT_BUFFER_SIZE>(true);
		new (&devExt->RemoveLock) kstd::RemoveLock();
		new (&devExt->BufferLock) kstd::SpinLock();

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

void cleanupKbFilterDevice(PDEVICE_KBFILTER devExt)
{
	devExt->RemoveLock.ManualLock();
	devExt->RemoveLock.ReleaseAndWait();
	devExt->KeyEventBuffer.~CircularBuffer(); // Explicitly call destructor

	if (devExt->LowerDeviceObject) {
		IoDetachDevice(devExt->LowerDeviceObject);
		devExt->LowerDeviceObject = nullptr;
	}
}
