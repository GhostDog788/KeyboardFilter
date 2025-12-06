#include <ntddk.h>
#include "RemoveLock.hpp"

#define log(fmt, ...) DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_INFO_LEVEL, fmt, __VA_ARGS__)

typedef struct _DEVICE_EXTENSION
{
	kstd::RemoveLock RemoveLock;
	PDEVICE_OBJECT  LowerDeviceObject;
	// ... anything else you need
} DEVICE_EXTENSION, * PDEVICE_EXTENSION;

void DriverUnload(PDRIVER_OBJECT DriverObject);
void LogRequest(PDEVICE_EXTENSION extDev, PIRP Irp);
NTSTATUS ForwardMajorFunction(PDEVICE_OBJECT FilterDeviceObject, PIRP Irp);
NTSTATUS HandleReadRequest(PDEVICE_OBJECT FilterDeviceObject, PIRP Irp);
NTSTATUS FilterDispatchPnp(PDEVICE_OBJECT FilterDeviceObject, PIRP Irp);

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
		devExt->RemoveLock.Initialize();

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

		if (devExt->LowerDeviceObject) {
			IoDetachDevice(devExt->LowerDeviceObject);
			devExt->LowerDeviceObject = nullptr;
		}

		PDEVICE_OBJECT nextFilterDeviceObject = currentFilterDeviceObject->NextDevice;
		IoDeleteDevice(currentFilterDeviceObject);
		currentFilterDeviceObject = nextFilterDeviceObject;
	}
}

NTSTATUS ForwardMajorFunction(PDEVICE_OBJECT FilterDeviceObject, PIRP Irp) {
	NTSTATUS status;
	auto devExt = (PDEVICE_EXTENSION)FilterDeviceObject->DeviceExtension;
	kstd::RemoveLockGuard guard = devExt->RemoveLock.LockAcquire(Irp, status);
	if (!NT_SUCCESS(status)) return status;
	LogRequest(devExt, Irp);
	IoSkipCurrentIrpStackLocation(Irp);
	return IoCallDriver(devExt->LowerDeviceObject, Irp);
}

NTSTATUS HandleReadRequest(PDEVICE_OBJECT FilterDeviceObject, PIRP Irp) {
	NTSTATUS status;
	auto devExt = (PDEVICE_EXTENSION)FilterDeviceObject->DeviceExtension;
	kstd::RemoveLockGuard guard = devExt->RemoveLock.LockAcquire(Irp, status);
	if (!NT_SUCCESS(status)) return status;
	LogRequest(devExt, Irp);

	IoSkipCurrentIrpStackLocation(Irp);
	return IoCallDriver(devExt->LowerDeviceObject, Irp);
}

NTSTATUS FilterDispatchPnp(PDEVICE_OBJECT FilterDeviceObject, PIRP Irp) {
	NTSTATUS status;
	auto devExt = (DEVICE_EXTENSION*)FilterDeviceObject->DeviceExtension;
	kstd::RemoveLockGuard guard = devExt->RemoveLock.LockAcquire(Irp, status);
	if (!NT_SUCCESS(status)) return status;
	auto stack = IoGetCurrentIrpStackLocation(Irp);
	UCHAR minor = stack->MinorFunction;
	LogRequest(devExt, Irp);

	IoSkipCurrentIrpStackLocation(Irp);
	status = IoCallDriver(devExt->LowerDeviceObject, Irp);
	if (minor == IRP_MN_REMOVE_DEVICE) {
		guard.ReleaseAndWait();
		IoDetachDevice(devExt->LowerDeviceObject);
		IoDeleteDevice(FilterDeviceObject);
	}
	return status;
}

void LogRequest(PDEVICE_EXTENSION extDev, PIRP Irp) {
	auto thread_id = Irp->Tail.Overlay.Thread;
	ULONG tid = 0, pid = 0;
	if (thread_id) {
		tid = HandleToUlong(PsGetThreadId(thread_id));
		pid = HandleToUlong(PsGetThreadProcessId(thread_id));
	}
	auto stack = IoGetCurrentIrpStackLocation(Irp);
	log("Intercepted driver: %wZ: PID: %d, TID: %d, MJ=%d\n",
		&extDev->LowerDeviceObject->DriverObject->DriverName, pid, tid, stack->MajorFunction);
}