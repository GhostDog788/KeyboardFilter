#include "IrpHandling.h"

NTSTATUS completeIrp(PIRP Irp, NTSTATUS status, ULONG_PTR info) {
	Irp->IoStatus.Status = status;
	Irp->IoStatus.Information = info;
	IoCompleteRequest(Irp, IO_NO_INCREMENT);
	return status;
}
