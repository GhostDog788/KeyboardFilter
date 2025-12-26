#pragma once
#include <ntddk.h>
#include <shared/Common.h>
#include <kstd/synchronization/RemoveLock.hpp>
#include <kstd/collections/CircularBuffer.hpp>

typedef struct _DEVICE_EXTENSION
{
	kstd::RemoveLock RemoveLock;
	PDEVICE_OBJECT LowerDeviceObject;
	kstd::CircularBuffer<KeyEvent, KEY_EVENT_BUFFER_SIZE> KeyEventBuffer;
	KSPIN_LOCK BufferLock;
	// ... anything else you need
} DEVICE_EXTENSION, * PDEVICE_EXTENSION;
