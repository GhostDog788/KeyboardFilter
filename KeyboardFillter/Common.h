#pragma once

constexpr unsigned int KEY_EVENT_BUFFER_SIZE = 256; // must be a power of 2 if you want easy masking

#define IOCTL_KBF_GET_EVENTS CTL_CODE(FILE_DEVICE_KEYBOARD, 0x800, METHOD_BUFFERED, FILE_ANY_ACCESS)

typedef struct _KeyData
{
    unsigned long long Timestamp;  // KeQuerySystemTimePrecise or similar
    unsigned short MakeCode;   // from KEYBOARD_INPUT_DATA
    unsigned short Flags;      // KEY_MAKE, KEY_BREAK, etc.
}KeyData;
