#pragma once
#include <ntddk.h>
#include <shared/Common.h>
#include <kstd/synchronization/RemoveLock.hpp>
#include <kstd/synchronization/SpinLock.hpp>
#include <kstd/collections/CircularBuffer.hpp>

enum class DeviceType {
	DEVICE_CONTROL,
	DEVICE_KBFILTER
};

typedef struct _DEVICE_GENERIC
{
	DeviceType DeviceType; // Used to distinguish device types
} DEVICE_GENERIC, * PDEVICE_GENERIC;

#define KeyEventBufferType kstd::CircularBuffer<KeyEvent, KEY_EVENT_BUFFER_SIZE, kstd::SpinLock, kstd::SpinLock>
typedef struct _DEVICE_KBFILTER
{
	DeviceType DeviceType; // Need to set to DeviceType::DEVICE_KBFILTER
	kstd::RemoveLock RemoveLock;
	PDEVICE_OBJECT LowerDeviceObject;
	KeyEventBufferType KeyEventBuffer;
	kstd::SpinLock BufferLock;
	// Instead of buffer for each device, we need to have a global buffer in the driver
	// The IOCTL can't access a specific device's buffer, thus the buffer must be global
	// Key notes:
	// - Writing to the buffer happens in IRQL DISPATCH_LEVEL (in read completion routine), thus spinlock is our only synchronization option
	// - Readig from the buffer is a modifing operation, thus reading locks the buffer as well
	// Conclution: we need a global, spinlock protected, circular buffer, at nonpaged memory
	// Problem: the driver attaches itself to all keyboard devices, and all filters write to the same global buffer
	// each write must lock the spinlock, and reading locks it as well, this can block code at DISPATCH_LEVEL for too long
} DEVICE_KBFILTER, * PDEVICE_KBFILTER;
