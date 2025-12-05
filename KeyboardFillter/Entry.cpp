#include <ntddk.h>

#define log(fmt, ...) DbgPrintEx(DPFLTR_IHVDRIVER_ID, DPFLTR_INFO_LEVEL, fmt, __VA_ARGS__)

typedef struct _DEVICE_EXTENSION
{
	PDEVICE_OBJECT  Self;
	PDEVICE_OBJECT  LowerDeviceObject;
	// ... anything else you need
} DEVICE_EXTENSION, * PDEVICE_EXTENSION;

void DriverUnload(PDRIVER_OBJECT DriverObject);
NTSTATUS ForwardMajorFunction(PDEVICE_OBJECT FilterDeviceObject, PIRP Irp);
NTSTATUS HandleReadRequest(PDEVICE_OBJECT FilterDeviceObject, PIRP Irp);
NTSTATUS FilterDispatchPnp(PDEVICE_OBJECT FilterDeviceObject, PIRP Irp);

extern "C" NTSTATUS
DriverEntry(PDRIVER_OBJECT DriverObject, PUNICODE_STRING RegistryPath) {
	UNREFERENCED_PARAMETER(RegistryPath);

	UNICODE_STRING targetName = RTL_CONSTANT_STRING(L"\\Device\\KeyboardClass0");
	NTSTATUS status = STATUS_SUCCESS;
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

		PDEVICE_OBJECT filterDeviceObject = nullptr;
		status = IoCreateDevice(DriverObject, sizeof(DEVICE_EXTENSION), nullptr,
			FILE_DEVICE_UNKNOWN, 0, FALSE, &filterDeviceObject);
		if (!NT_SUCCESS(status)) break;
		devExt = (PDEVICE_EXTENSION)filterDeviceObject->DeviceExtension;
		RtlZeroMemory(devExt, sizeof(*devExt));
		devExt->Self = filterDeviceObject;

		PFILE_OBJECT FileObject;
		PDEVICE_OBJECT TargetDeviceObject;
		status = IoGetDeviceObjectPointer(&targetName, FILE_READ_DATA, &FileObject, &TargetDeviceObject);
		if (!NT_SUCCESS(status)) break;

		devExt->Self->Flags |= TargetDeviceObject->Flags & (DO_BUFFERED_IO | DO_DIRECT_IO);
		devExt->Self->Flags &= ~DO_DEVICE_INITIALIZING;
		devExt->Self->Flags |= DO_POWER_PAGABLE;
		devExt->Self->DeviceType = TargetDeviceObject->DeviceType;

		status = IoAttachDeviceToDeviceStackSafe(devExt->Self, TargetDeviceObject, &devExt->LowerDeviceObject);
		ObfDereferenceObject(FileObject);
		if (!NT_SUCCESS(status)) break;

		log("Attached to %wZ, lower DO=%p\n", &targetName, devExt->LowerDeviceObject);
	} while (false);
	
	if (!NT_SUCCESS(status)) {
		log("Failed to attach to %wZ: status=0x%08X\n", &targetName, status);
		if (devExt && devExt->Self) {
			IoDeleteDevice(devExt->Self);
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
	auto devExt = (PDEVICE_EXTENSION)FilterDeviceObject->DeviceExtension;
	auto stack = IoGetCurrentIrpStackLocation(Irp);
	log("KeyboardFilter: MajorFunction %u\n", stack->MajorFunction);
	IoSkipCurrentIrpStackLocation(Irp);
	return IoCallDriver(devExt->LowerDeviceObject, Irp);
}

NTSTATUS HandleReadRequest(PDEVICE_OBJECT FilterDeviceObject, PIRP Irp) {
	auto devExt = (PDEVICE_EXTENSION)FilterDeviceObject->DeviceExtension;
	log("KeyboardFilter: Read Request\n");
	IoSkipCurrentIrpStackLocation(Irp);
	return IoCallDriver(devExt->LowerDeviceObject, Irp);
}

NTSTATUS FilterDispatchPnp(PDEVICE_OBJECT FilterDeviceObject, PIRP Irp) {
	auto ext = (DEVICE_EXTENSION*)FilterDeviceObject->DeviceExtension;
	auto stack = IoGetCurrentIrpStackLocation(Irp);
	UCHAR minor = stack->MinorFunction;
	IoSkipCurrentIrpStackLocation(Irp);
	auto status = IoCallDriver(ext->LowerDeviceObject, Irp);
	if (minor == IRP_MN_REMOVE_DEVICE) {
		IoDetachDevice(ext->LowerDeviceObject);
		IoDeleteDevice(FilterDeviceObject);
	}
	return status;
}
