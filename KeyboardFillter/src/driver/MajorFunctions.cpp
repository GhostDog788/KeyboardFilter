#include "MajorFunctions.h"
#include "Definitions.h"
#include "KeyboardSniffing.h"
#include <src/utils/logging.h>

NTSTATUS ForwardMajorFunction(PDEVICE_OBJECT FilterDeviceObject, PIRP Irp) {
	NTSTATUS status;
	auto devExt = (PDEVICE_EXTENSION)FilterDeviceObject->DeviceExtension;
	kstd::RemoveLockGuard guard(devExt->RemoveLock.LockAcquire(Irp, status));
	if (!NT_SUCCESS(status)) return status;

	LogRequest(devExt, Irp);
	IoSkipCurrentIrpStackLocation(Irp);
	return IoCallDriver(devExt->LowerDeviceObject, Irp);
}

NTSTATUS HandleReadRequest(PDEVICE_OBJECT FilterDeviceObject, PIRP Irp) {
	auto devExt = (PDEVICE_EXTENSION)FilterDeviceObject->DeviceExtension;
	NTSTATUS status = devExt->RemoveLock.ManualLock(Irp);
	if (!NT_SUCCESS(status)) return status;

	LogRequest(devExt, Irp);
	IoCopyCurrentIrpStackLocationToNext(Irp);
	IoSetCompletionRoutine(Irp, ReadCompletion, FilterDeviceObject, true, true, true);
	return IoCallDriver(devExt->LowerDeviceObject, Irp);
}

NTSTATUS FilterDispatchPnp(PDEVICE_OBJECT FilterDeviceObject, PIRP Irp) {
	NTSTATUS status;
	auto devExt = (DEVICE_EXTENSION*)FilterDeviceObject->DeviceExtension;
	kstd::RemoveLockGuard guard(devExt->RemoveLock.LockAcquire(Irp, status));
	if (!NT_SUCCESS(status)) return status;
	auto stack = IoGetCurrentIrpStackLocation(Irp);
	UCHAR minor = stack->MinorFunction;
	LogRequest(devExt, Irp);

	IoSkipCurrentIrpStackLocation(Irp);
	status = IoCallDriver(devExt->LowerDeviceObject, Irp);
	if (minor == IRP_MN_REMOVE_DEVICE) {
		guard.Clear();
		devExt->RemoveLock.ReleaseAndWait();
		IoDetachDevice(devExt->LowerDeviceObject);
		IoDeleteDevice(FilterDeviceObject);
	}
	return status;
}

NTSTATUS ReadCompletion(PDEVICE_OBJECT FilterDeviceObject, PIRP Irp, PVOID Context)
{
	UNREFERENCED_PARAMETER(Context);

	auto devExt = (DEVICE_EXTENSION*)FilterDeviceObject->DeviceExtension;
	if (Irp->PendingReturned) {
		IoMarkIrpPending(Irp);
	}

	if (NT_SUCCESS(Irp->IoStatus.Status) && Irp->IoStatus.Information > 0)
	{
		ULONG bytes = (ULONG)Irp->IoStatus.Information;
		PUCHAR buf = (PUCHAR)Irp->AssociatedIrp.SystemBuffer;
		log("READ completed: Status=0x%08X, Bytes=%lu, Buf=%p\n", Irp->IoStatus.Status, bytes, buf);

		HandleKeyboardResponse(devExt, Irp);
	}
	else {
		log("READ completed: Status=0x%08X, Bytes=%Iu\n", Irp->IoStatus.Status, Irp->IoStatus.Information);
	}

	devExt->RemoveLock.ManualUnlock();
	return STATUS_CONTINUE_COMPLETION;
}
