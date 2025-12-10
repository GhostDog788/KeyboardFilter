#pragma once

#include <cstddef>
#include <ntddk.h>

void* __cdecl operator new(size_t size)
{
    return ExAllocatePool2(POOL_FLAG_NON_PAGED, size, 'tdsK');
}

void* __cdecl operator new(size_t size, void* place) noexcept
{
	UNREFERENCED_PARAMETER(size);
    return place; // placement new
}

void operator delete(void*, void*) noexcept {}
void __cdecl operator delete(void* ptr) noexcept
{
    ExFreePool(ptr);
}

void __cdecl operator delete(void* ptr, size_t) noexcept
{
    ExFreePool(ptr);
}
