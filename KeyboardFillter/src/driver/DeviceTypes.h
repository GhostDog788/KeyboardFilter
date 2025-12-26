#pragma once
#include <ntddk.h>
#include <shared/Common.h>
#include <kstd/synchronization/RemoveLock.hpp>
#include <kstd/collections/CircularBuffer.hpp>

enum class DeviceType {
	DEVICE_CONTROL,
	DEVICE_KBFILTER
};

typedef struct _DEVICE_GENERIC
{
	DeviceType DeviceType; // Used to distinguish device types
} DEVICE_GENERIC, * PDEVICE_GENERIC;

typedef struct _DEVICE_KBFILTER
{
	DeviceType DeviceType; // Need to set to DeviceType::DEVICE_KBFILTER
	kstd::RemoveLock RemoveLock;
	PDEVICE_OBJECT LowerDeviceObject;
	kstd::CircularBuffer<KeyEvent, KEY_EVENT_BUFFER_SIZE> KeyEventBuffer;
	KSPIN_LOCK BufferLock;
} DEVICE_KBFILTER, * PDEVICE_KBFILTER;
