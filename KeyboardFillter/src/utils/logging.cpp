#include "logging.h"

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
