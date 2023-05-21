#pragma once

struct _IO_STATUS_BLOCK;
struct _OVERLAPPED;

namespace allio::win32 {

using NTSTATUS = long;
using HANDLE = void*;

using IO_STATUS_BLOCK = _IO_STATUS_BLOCK;
using OVERLAPPED = _OVERLAPPED;


template<typename T>
struct win32_type_traits
{
	static constexpr size_t size = sizeof(T);
};

template<>
struct win32_type_traits<IO_STATUS_BLOCK>
{
	static constexpr size_t size = sizeof(uintptr_t) * 2;
};

template<>
struct win32_type_traits<OVERLAPPED>
{
	static constexpr size_t size = sizeof(uintptr_t) * 3 + 8;
};

} // namespace allio::win32
